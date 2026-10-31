// SPDX-License-Identifier: GPL-2.0-only
#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/string.h>
#include "aupe_peer_rx.h"

/* Hard cap pending real controller/netns quotas. No per-packet allocation.
 * Upper bound includes two large assembly buffers per peer. */
#define AUPE_LAB_MAX_RX_PEERS 64
static atomic_t aupe_live_peers = ATOMIC_INIT(0);

struct aupe_rx_peer {
    struct mutex lock;
    struct af_mux mux;
    struct af_state *a;
    struct af_state *b;
};

struct aupe_rx_peer *aupe_rx_peer_create(gfp_t gfp)
{
    struct aupe_rx_peer *p;
    /* All callers are required to run in sleepable process context. */
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
    return p;
fail_a:
    kvfree(p->a);
fail_obj:
    kfree(p);
fail_quota:
    atomic_dec(&aupe_live_peers);
    return ERR_PTR(-ENOMEM);
}
void aupe_rx_peer_destroy(struct aupe_rx_peer *p)
{
    if (!p || IS_ERR(p))
        return;
    might_sleep();
    /* Caller must first quiesce all RX/maintenance references. */
    mutex_lock(&p->lock);
    af_mux_unbind(&p->mux);
    mutex_unlock(&p->lock);
    kvfree_sensitive(p->a, sizeof(*p->a));
    kvfree_sensitive(p->b, sizeof(*p->b));
    kfree(p);
    atomic_dec(&aupe_live_peers);
}
int aupe_rx_peer_bind(struct aupe_rx_peer *p, af_u64 epoch)
{
    int r;
    if (!p || IS_ERR(p))
        return -EINVAL;
    might_sleep();
    mutex_lock(&p->lock);
    r = af_mux_bind(&p->mux, p->a, p->b, epoch);
    mutex_unlock(&p->lock);
    return r;
}
void aupe_rx_peer_unbind(struct aupe_rx_peer *p)
{
    if (!p || IS_ERR(p))
        return;
    might_sleep();
    mutex_lock(&p->lock);
    af_mux_unbind(&p->mux);
    mutex_unlock(&p->lock);
}
int aupe_rx_peer_feed(struct aupe_rx_peer *p, af_u64 epoch, af_u64 now,
                      const af_u8 *pkt, af_u32 n, unsigned *slot)
{
    int r;
    if (!p || IS_ERR(p))
        return AF_BAD;
    might_sleep();
    mutex_lock(&p->lock);
    r = af_mux_feed(&p->mux, epoch, now, pkt, n, slot);
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
    r = af_mux_take(&p->mux, epoch, slot, out, cap);
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
    r = af_mux_expire(&p->mux, epoch, now);
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
    r = af_mux_feed(&p->mux, epoch, now, pkt, n, &slot);
    if (r == AF_DONE) {
        int size = af_mux_take(&p->mux, epoch, slot, out, capacity);
        if (size > (int)AF_IPV4_HLEN) {
            *rebuilt = (af_u32)size;
        } else {
            /* The output buffer is undersized or invalid: caller drops.
             * A previously completed packet remains pending until timeout;
             * do not risk returning AF_DONE with no output. */
            r = (size < 0) ? size : AF_BAD;
        }
    }
    mutex_unlock(&p->lock);
    return r;
}