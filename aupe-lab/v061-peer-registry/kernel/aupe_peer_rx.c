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
#include "aupe_peer_rx.h"

/* Temporary global ceiling, until the main AUPE netns manager is integrated. */
#define AUPE_LAB_MAX_RX_PEERS 64
static atomic_t aupe_live_peers = ATOMIC_INIT(0);

struct aupe_rx_peer {
    struct mutex lock;
    refcount_t refs;                  /* owner + pinned worker references */
    struct aupe_rx_gate gate;        /* serializes Noise epoch / shutdown */
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
    r = aupe_gate_bind(&p->gate, epoch);
    if (r == AF_WAIT) {
        r = af_mux_bind(&p->mux, p->a, p->b, epoch);
        if (r != AF_WAIT)
            aupe_gate_unbind(&p->gate);
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
    r = rx_gate_check(p, epoch);
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
    r = rx_gate_check(p, epoch);
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
    r = rx_gate_check(p, epoch);
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

int aupe_rx_peer_expire(struct aupe_rx_peer *p, af_u64 epoch, af_u64 now)
{
    int r;
    if (!p || IS_ERR(p))
        return AF_BAD;
    might_sleep();
    mutex_lock(&p->lock);
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
