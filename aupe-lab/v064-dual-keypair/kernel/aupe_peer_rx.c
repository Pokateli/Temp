// SPDX-License-Identifier: GPL-2.0-only
#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/refcount.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include "aupe_gate.h"
#include "aupe_replay.h"
#include "aupe_keypairs.h"
#include "aupe_peer_rx.h"

/* Temporary global ceiling, until the main AUPE netns manager is integrated. */
#define AUPE_LAB_MAX_RX_PEERS 64
static atomic_t aupe_live_peers = ATOMIC_INIT(0);

struct aupe_rx_peer {
    struct mutex lock;
    refcount_t refs;                  /* owner + pinned worker references */
    struct aupe_rx_gate gate;        /* serializes Noise epoch / shutdown */
    struct aupe_replay_window replay; /* authenticated transport counters */
    struct aupe_keypair_bank keypairs; /* concurrent keypair replay windows */
    bool keypair_mode; /* irreversible: legacy RX entry points remain closed */
    struct aupe_rx_stats stats;
    struct af_mux mux;
    struct af_state *a;
    struct af_state *b;
};

struct aupe_rx_peer *aupe_rx_peer_create(gfp_t gfp)
{
    struct aupe_rx_peer *p;
    might_sleep();
    if (atomic_inc_return(&aupe_live_peers) > AUPE_LAB_MAX_RX_PEERS) {
        atomic_dec(&aupe_live_peers);
        return ERR_PTR(-EDQUOT);
    }
    p = kzalloc(sizeof(*p), gfp);
    if (!p)
        goto fail_quota;
    p->a = kvzalloc(sizeof(*p->a), gfp);
    if (!p->a)
        goto fail_obj;
    p->b = kvzalloc(sizeof(*p->b), gfp);
    if (!p->b)
        goto fail_a;
    mutex_init(&p->lock);
    refcount_set(&p->refs, 1);
    return p;
fail_a:
    kvfree_sensitive(p->a, sizeof(*p->a));
fail_obj:
    kfree(p);
fail_quota:
    atomic_dec(&aupe_live_peers);
    return ERR_PTR(-ENOMEM);
}

struct aupe_rx_peer *aupe_rx_peer_get(struct aupe_rx_peer *p)
{
    if (!p || IS_ERR(p))
        return ERR_PTR(-EINVAL);
    might_sleep();
    /* The OWNER protects initial p against concurrent final put. */
    if (!refcount_inc_not_zero(&p->refs))
        return ERR_PTR(-ESHUTDOWN);
    mutex_lock(&p->lock);
    if (p->gate.state == AUPE_GATE_CLOSED) {
        mutex_unlock(&p->lock);
        aupe_rx_peer_put(p);
        return ERR_PTR(-ESHUTDOWN);
    }
    mutex_unlock(&p->lock);
    return p;
}

void aupe_rx_peer_put(struct aupe_rx_peer *p)
{
    if (!p || IS_ERR(p))
        return;
    might_sleep();
    if (!refcount_dec_and_test(&p->refs))
        return;
    /* Final release: may occur only after owner unpublished pointer.
     * No new get() is permitted; closing is idempotent. */
    mutex_lock(&p->lock);
    aupe_gate_close(&p->gate);
    af_mux_unbind(&p->mux);
    aupe_replay_clear(&p->replay);
    aupe_keypairs_clear(&p->keypairs);
    mutex_unlock(&p->lock);
    kvfree_sensitive(p->a, sizeof(*p->a));
    kvfree_sensitive(p->b, sizeof(*p->b));
    memzero_explicit(&p->stats, sizeof(p->stats));
    kfree(p);
    atomic_dec(&aupe_live_peers);
}

void aupe_rx_peer_close(struct aupe_rx_peer *p)
{
    if (!p || IS_ERR(p))
        return;
    might_sleep();
    mutex_lock(&p->lock);
    aupe_gate_close(&p->gate);
    af_mux_unbind(&p->mux);
    aupe_replay_clear(&p->replay);
    aupe_keypairs_clear(&p->keypairs);
    mutex_unlock(&p->lock);
}

void aupe_rx_peer_destroy(struct aupe_rx_peer *p)
{
    if (!p || IS_ERR(p))
        return;
    aupe_rx_peer_close(p);
    aupe_rx_peer_put(p);
}

int aupe_rx_peer_bind(struct aupe_rx_peer *p, af_u64 epoch)
{
    int r;
    if (!p || IS_ERR(p))
        return AF_BAD;
    might_sleep();
    mutex_lock(&p->lock);
    r = p->keypair_mode ? AF_STALE_EPOCH : aupe_gate_bind(&p->gate, epoch);
    if (r == AF_ALREADY_BOUND) {
        /* Preserve pending fragments on duplicate session events. */
        r = AF_WAIT;
    } else if (r == AF_WAIT) {
        r = af_mux_bind(&p->mux, p->a, p->b, epoch);
        if (r != AF_WAIT)
            aupe_gate_unbind(&p->gate);
        else
            aupe_replay_bind(&p->replay, epoch);
    }
    mutex_unlock(&p->lock);
    return r;
}

void aupe_rx_peer_unbind(struct aupe_rx_peer *p)
{
    if (!p || IS_ERR(p))
        return;
    might_sleep();
    mutex_lock(&p->lock);
    aupe_gate_unbind(&p->gate);
    af_mux_unbind(&p->mux);
    aupe_replay_clear(&p->replay);
    aupe_keypairs_clear(&p->keypairs);
    mutex_unlock(&p->lock);
}

static int rx_gate_check(struct aupe_rx_peer *p, af_u64 epoch)
{
    return aupe_gate_check(&p->gate, epoch);
}

static void rx_record(struct aupe_rx_stats *stats, int r)
{
    if (r == AF_WAIT)
        stats->rx_wait++;
    else if (r == AF_DONE)
        stats->rx_reassembled++;
    else if (r == AF_BYPASS)
        stats->rx_bypass++;
    else {
        stats->rx_drop++;
        if (r == AF_STALE_EPOCH)
            stats->rx_stale_epoch++;
        else if (r == AF_CLOSED)
            stats->rx_closed++;
    }
}

int aupe_rx_peer_feed(struct aupe_rx_peer *p, af_u64 epoch, af_u64 now,
                      const af_u8 *pkt, af_u32 n, unsigned *slot)
{
    int r;
    if (!p || IS_ERR(p) || !slot)
        return AF_BAD;
    *slot = AF_MUX_SLOTS;
    might_sleep();
    mutex_lock(&p->lock);
    r = p->keypair_mode ? AF_STALE_EPOCH : rx_gate_check(p, epoch);
    if (r == AF_WAIT)
        r = af_mux_feed(&p->mux, epoch, now, pkt, n, slot);
    rx_record(&p->stats, r);
    mutex_unlock(&p->lock);
    return r;
}

int aupe_rx_peer_take(struct aupe_rx_peer *p, af_u64 epoch, unsigned slot,
                      af_u8 *out, af_u32 cap)
{
    int r;
    if (!p || IS_ERR(p))
        return AF_BAD;
    might_sleep();
    mutex_lock(&p->lock);
    r = p->keypair_mode ? AF_STALE_EPOCH : rx_gate_check(p, epoch);
    if (r == AF_WAIT)
        r = af_mux_take(&p->mux, epoch, slot, out, cap);
    mutex_unlock(&p->lock);
    return r;
}

int aupe_rx_peer_ingest(struct aupe_rx_peer *p, af_u64 epoch, af_u64 now,
                        const af_u8 *pkt, af_u32 n, af_u8 *out,
                        af_u32 capacity, af_u32 *rebuilt)
{
    unsigned slot = AF_MUX_SLOTS;
    int r;
    if (!p || IS_ERR(p) || !rebuilt || !out)
        return AF_BAD;
    *rebuilt = 0;
    might_sleep();
    mutex_lock(&p->lock);
    r = p->keypair_mode ? AF_STALE_EPOCH : rx_gate_check(p, epoch);
    if (r == AF_WAIT)
        r = af_mux_feed(&p->mux, epoch, now, pkt, n, &slot);
    if (r == AF_DONE) {
        int size = af_mux_take(&p->mux, epoch, slot, out, capacity);
        if (size > (int)AF_IPV4_HLEN)
            *rebuilt = (af_u32)size;
        else
            r = (size < 0) ? size : AF_BAD;
    }
    rx_record(&p->stats, r);
    mutex_unlock(&p->lock);
    return r;
}

int aupe_rx_peer_dispatch(struct aupe_rx_peer *p, af_u64 epoch, af_u64 now,
                          const af_u8 *pkt, af_u32 n, af_u8 *scratch,
                          af_u32 capacity, aupe_rx_emit_fn emit, void *ctx)
{
    int r;
    if (!p || IS_ERR(p))
        return AF_BAD;
    might_sleep();
    mutex_lock(&p->lock);
    r = p->keypair_mode ? AF_STALE_EPOCH :
        aupe_dispatch_verified(&p->gate, &p->mux, epoch, now,
                               pkt, n, scratch, capacity, emit, ctx);
    rx_record(&p->stats, r);
    mutex_unlock(&p->lock);
    return r;
}

int aupe_rx_peer_dispatch_counter(struct aupe_rx_peer *p, af_u64 epoch,
                                  af_u64 counter, af_u64 now,
                                  const af_u8 *pkt, af_u32 n,
                                  af_u8 *scratch, af_u32 capacity,
                                  aupe_rx_emit_fn emit, void *ctx)
{
    int r;
    if (!p || IS_ERR(p)) return AF_BAD;
    might_sleep();
    mutex_lock(&p->lock);
    r = p->keypair_mode ? AF_STALE_EPOCH :
        aupe_dispatch_counter(&p->gate, &p->mux, &p->replay, epoch,
                              counter, now, pkt, n, scratch, capacity,
                              emit, ctx);
    rx_record(&p->stats, r);
    if (r == AUPE_REPLAY_DUPLICATE || r == AUPE_REPLAY_LIMIT)
        p->stats.rx_replay_drop++;
    mutex_unlock(&p->lock);
    return r;
}

int aupe_rx_peer_expire(struct aupe_rx_peer *p, af_u64 epoch, af_u64 now)
{
    int r;
    if (!p || IS_ERR(p))
        return AF_BAD;
    might_sleep();
    mutex_lock(&p->lock);
    /* Expiration is housekeeping, not packet delivery: permit it for the
     * active bank generation even though legacy RX is disabled. */
    r = rx_gate_check(p, epoch);
    if (r == AF_WAIT) {
        r = af_mux_expire(&p->mux, epoch, now);
        if (r > 0)
            p->stats.rx_expired_slots += r;
    }
    mutex_unlock(&p->lock);
    return r;
}

int aupe_rx_peer_snapshot(struct aupe_rx_peer *p, struct aupe_rx_stats *out)
{
    if (!p || IS_ERR(p) || !out)
        return AF_BAD;
    might_sleep();
    mutex_lock(&p->lock);
    *out = p->stats;
    mutex_unlock(&p->lock);
    return AF_WAIT;
}

int aupe_rx_peer_install_keypair(struct aupe_rx_peer *p, af_u64 epoch,
                                 af_u64 now_ms)
{
    int rv;
    if (!p || IS_ERR(p) || !epoch) return AF_BAD;
    might_sleep();
    mutex_lock(&p->lock);
    if (p->gate.state == AUPE_GATE_CLOSED) {
        rv = AF_CLOSED;
        goto done;
    }
    if (p->keypairs.enabled) {
        if (epoch == p->keypairs.active.epoch) {
            if (now_ms < p->keypairs.last_now_ms) {
                rv = AF_BAD;
            } else {
                /* Idempotent notification must not restore old clock state. */
                p->keypairs.last_now_ms = now_ms;
                rv = AF_WAIT;
            }
            goto done;
        }
        if (epoch <= p->keypairs.high_watermark) {
            rv = AF_STALE_EPOCH;
            goto done;
        }
    } else if (epoch <= p->gate.high_watermark) {
        rv = AF_STALE_EPOCH;
        goto done;
    }
    /* Validate time before changing any gate/mux state. */
    if (now_ms > (~(af_u64)0 - AUPE_KEYPAIR_GRACE_MS) ||
        (p->keypairs.clock_started && now_ms < p->keypairs.last_now_ms)) {
        rv = AF_BAD;
        goto done;
    }
    /* All buffers were allocated at create. No memory allocation in RX. */
    rv = aupe_gate_bind(&p->gate, epoch);
    if (rv != AF_WAIT) goto done;
    rv = af_mux_bind(&p->mux, p->a, p->b, epoch);
    if (rv != AF_WAIT) {
        aupe_gate_unbind(&p->gate);
        goto done;
    }
    /* Previously accepted keypair survives the next generation briefly. */
    rv = aupe_keypairs_install(&p->keypairs, epoch, now_ms);
    if (rv == AF_WAIT) {
        p->keypair_mode = true;
        aupe_replay_clear(&p->replay); /* disallow legacy replay bypass */
    }
done:
    mutex_unlock(&p->lock);
    return rv;
}
int aupe_rx_peer_retire_previous(struct aupe_rx_peer *p, af_u64 epoch)
{
    int rv;
    if (!p || IS_ERR(p)) return AF_BAD;
    might_sleep();
    mutex_lock(&p->lock);
    rv = p->gate.state == AUPE_GATE_CLOSED ? AF_CLOSED :
         aupe_keypairs_retire_previous(&p->keypairs, epoch);
    mutex_unlock(&p->lock);
    return rv;
}
int aupe_rx_peer_dispatch_keypair(struct aupe_rx_peer *p,
                                  af_u64 keypair_epoch, af_u64 counter,
                                  af_u64 now_ms, const af_u8 *pkt,
                                  af_u32 n, af_u8 *scratch, af_u32 capacity,
                                  aupe_rx_emit_fn emit, void *ctx)
{
    int rv;
    if (!p || IS_ERR(p)) return AF_BAD;
    might_sleep();
    mutex_lock(&p->lock);
    rv = aupe_dispatch_keypair(&p->gate, &p->mux, &p->keypairs,
                               keypair_epoch, counter, now_ms, pkt, n,
                               scratch, capacity, emit, ctx);
    rx_record(&p->stats, rv);
    if (rv == AUPE_REPLAY_DUPLICATE || rv == AUPE_REPLAY_LIMIT)
        p->stats.rx_replay_drop++;
    mutex_unlock(&p->lock);
    return rv;
}
