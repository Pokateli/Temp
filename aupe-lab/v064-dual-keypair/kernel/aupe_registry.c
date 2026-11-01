// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "aupe_registry.h"

struct aupe_rx_registry {
    struct mutex lock;
    struct aupe_handle_table table;
};

struct aupe_rx_registry *aupe_registry_create(gfp_t gfp)
{
    struct aupe_rx_registry *r;
    might_sleep();
    r = kzalloc(sizeof(*r), gfp);
    if (!r)
        return ERR_PTR(-ENOMEM);
    mutex_init(&r->lock);
    aupe_handles_init_seed(&r->table, get_random_u64());
    return r;
}

int aupe_registry_add(struct aupe_rx_registry *r, gfp_t gfp, af_u64 *handle)
{
    struct aupe_rx_peer *p;
    int rv;
    if (!r || IS_ERR(r) || !handle)
        return -EINVAL;
    *handle = 0;
    might_sleep();
    p = aupe_rx_peer_create(gfp);
    if (IS_ERR(p))
        return PTR_ERR(p);
    mutex_lock(&r->lock);
    rv = aupe_handles_publish(&r->table, p, handle);
    mutex_unlock(&r->lock);
    if (rv != AF_WAIT) {
        aupe_rx_peer_destroy(p);
        return rv == AUPE_HANDLE_FULL ? -ENOSPC : -EINVAL;
    }
    return 0;
}

struct aupe_rx_peer *aupe_registry_acquire(struct aupe_rx_registry *r,
                                         af_u64 handle)
{
    struct aupe_rx_peer *p, *held;
    if (!r || IS_ERR(r))
        return ERR_PTR(-EINVAL);
    might_sleep();
    mutex_lock(&r->lock);
    p = aupe_handles_lookup(&r->table, handle);
    if (p)
        held = aupe_rx_peer_get(p);
    else
        held = ERR_PTR(-ENOENT);
    mutex_unlock(&r->lock);
    return held;
}

int aupe_registry_bind(struct aupe_rx_registry *r, af_u64 handle,
                       af_u64 epoch)
{
    struct aupe_rx_peer *p = aupe_registry_acquire(r, handle);
    int rv;
    if (IS_ERR(p))
        return AF_CLOSED;
    rv = aupe_rx_peer_bind(p, epoch);
    aupe_rx_peer_put(p);
    return rv;
}

int aupe_registry_dispatch(struct aupe_rx_registry *r, af_u64 handle,
                            af_u64 epoch, af_u64 now, const af_u8 *pkt,
                            af_u32 n, af_u8 *scratch, af_u32 capacity,
                            aupe_rx_emit_fn emit, void *emit_context)
{
    struct aupe_rx_peer *p = aupe_registry_acquire(r, handle);
    int rv;
    if (IS_ERR(p))
        return AF_CLOSED;
    rv = aupe_rx_peer_dispatch(p, epoch, now, pkt, n, scratch,
                               capacity, emit, emit_context);
    aupe_rx_peer_put(p);
    return rv;
}

int aupe_registry_dispatch_counter(struct aupe_rx_registry *r,
                                   af_u64 handle, af_u64 epoch,
                                   af_u64 counter, af_u64 now,
                                   const af_u8 *pkt, af_u32 n,
                                   af_u8 *scratch, af_u32 capacity,
                                   aupe_rx_emit_fn emit, void *ctx)
{
    struct aupe_rx_peer *p = aupe_registry_acquire(r, handle);
    int rv;
    if (IS_ERR(p)) return AF_CLOSED;
    rv = aupe_rx_peer_dispatch_counter(p, epoch, counter, now, pkt, n,
                                       scratch, capacity, emit, ctx);
    aupe_rx_peer_put(p);
    return rv;
}

int aupe_registry_revoke(struct aupe_rx_registry *r, af_u64 handle)
{
    struct aupe_rx_peer *p;
    if (!r || IS_ERR(r))
        return -EINVAL;
    might_sleep();
    mutex_lock(&r->lock);
    p = aupe_handles_remove(&r->table, handle);
    if (p)
        aupe_rx_peer_close(p);
    mutex_unlock(&r->lock);
    if (!p)
        return -ENOENT;
    aupe_rx_peer_put(p);
    return 0;
}

void aupe_registry_destroy(struct aupe_rx_registry *r)
{
    struct aupe_rx_peer *p;
    unsigned i;
    if (!r || IS_ERR(r))
        return;
    might_sleep();
    /* Only the owner may do this after quiescing ALL registry users. */
    mutex_lock(&r->lock);
    for (i = 0; i < AUPE_HANDLE_SLOTS; ++i) {
        p = aupe_handles_drain(&r->table, i);
        if (p)
            aupe_rx_peer_destroy(p);
    }
    mutex_unlock(&r->lock);
    kfree(r);
}

int aupe_registry_install_keypair(struct aupe_rx_registry *r, af_u64 handle,
                                  af_u64 epoch, af_u64 now)
{
    struct aupe_rx_peer *p = aupe_registry_acquire(r, handle);
    int rv;
    if (IS_ERR(p)) return AF_CLOSED;
    rv = aupe_rx_peer_install_keypair(p, epoch, now);
    aupe_rx_peer_put(p);
    return rv;
}
int aupe_registry_retire_previous(struct aupe_rx_registry *r, af_u64 handle,
                                  af_u64 old_epoch)
{
    struct aupe_rx_peer *p = aupe_registry_acquire(r, handle);
    int rv;
    if (IS_ERR(p)) return AF_CLOSED;
    rv = aupe_rx_peer_retire_previous(p, old_epoch);
    aupe_rx_peer_put(p);
    return rv;
}
int aupe_registry_dispatch_keypair(struct aupe_rx_registry *r, af_u64 handle,
                                   af_u64 keypair_epoch, af_u64 counter,
                                   af_u64 now, const af_u8 *pkt, af_u32 n,
                                   af_u8 *scratch, af_u32 capacity,
                                   aupe_rx_emit_fn emit, void *ctx)
{
    struct aupe_rx_peer *p = aupe_registry_acquire(r, handle);
    int rv;
    if (IS_ERR(p)) return AF_CLOSED;
    rv = aupe_rx_peer_dispatch_keypair(p, keypair_epoch, counter, now,
                                       pkt, n, scratch, capacity, emit, ctx);
    aupe_rx_peer_put(p);
    return rv;
}
