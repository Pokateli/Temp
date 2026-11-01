// SPDX-License-Identifier: GPL-2.0-only
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "aupe_crypto_owner.h"

struct aupe_crypto_peer {
    struct mutex lock;
    struct aupe_rx_registry *registry; /* owner guarantees registry lifetime */
    af_u64 handle;
    struct aupe_crypto_binding binding;
};

struct aupe_crypto_peer *aupe_crypto_peer_create(
    struct aupe_rx_registry *r, af_u64 handle,
    const af_u8 confirmed_public_key[32], gfp_t gfp)
{
    struct aupe_crypto_peer *p;
    struct aupe_rx_peer *found;
    int rv;
    if (!r || IS_ERR(r) || !handle || !confirmed_public_key)
        return ERR_PTR(-EINVAL);
    might_sleep();
    /* Identity is a trusted handshake result, not an externally supplied
     * tunnel IP; the real Noise owner must enforce public-key uniqueness. */
    found = aupe_registry_acquire(r, handle);
    if (IS_ERR(found)) return ERR_CAST(found);
    aupe_rx_peer_put(found);
    p = kzalloc(sizeof(*p), gfp);
    if (!p) return ERR_PTR(-ENOMEM);
    mutex_init(&p->lock);
    rv = aupe_binding_init(&p->binding, confirmed_public_key);
    if (rv != AF_WAIT) {
        kfree(p);
        return ERR_PTR(-EINVAL);
    }
    p->registry = r;
    p->handle = handle;
    return p;
}

int aupe_crypto_peer_install(struct aupe_crypto_peer *p,
                             const af_u8 authenticated_pubkey[32],
                             struct aupe_crypto_ref ref, af_u64 now)
{
    af_u64 epoch = 0;
    int rv;
    if (!p || IS_ERR(p)) return AF_BAD;
    might_sleep();
    mutex_lock(&p->lock);
    rv = aupe_binding_install(&p->binding, authenticated_pubkey,
                              ref, now, &epoch);
    if (rv == AF_ALREADY_BOUND) {
        rv = AF_WAIT;
    } else if (rv == AF_WAIT) {
        rv = aupe_registry_install_keypair(p->registry, p->handle,
                                            epoch, now);
        if (rv != AF_WAIT) {
            /* Never leave a published owner mapping whose RX keypair
             * bank did not accept the epoch. Fail closed, not rollback. */
            aupe_binding_close(&p->binding);
            aupe_registry_revoke(p->registry, p->handle);
        }
    }
    mutex_unlock(&p->lock);
    return rv;
}

int aupe_crypto_verified_rx(struct aupe_crypto_peer *p,
                             struct aupe_crypto_ref ref, af_u64 counter,
                             af_u64 now, const af_u8 *plaintext, af_u32 n,
                             af_u8 *scratch, af_u32 capacity,
                             aupe_rx_emit_fn emit, void *ctx)
{
    af_u64 trusted_epoch = 0;
    int rv;
    if (!p || IS_ERR(p)) return AF_BAD;
    might_sleep();
    mutex_lock(&p->lock);
    rv = aupe_binding_resolve(&p->binding, ref, now, &trusted_epoch);
    if (rv == AF_WAIT)
        rv = aupe_registry_dispatch_keypair(p->registry, p->handle,
                             trusted_epoch, counter, now, plaintext, n,
                             scratch, capacity, emit, ctx);
    mutex_unlock(&p->lock);
    return rv;
}

int aupe_crypto_peer_retire(struct aupe_crypto_peer *p,
                            struct aupe_crypto_ref previous)
{
    af_u64 epoch;
    int rv;
    if (!p || IS_ERR(p)) return AF_BAD;
    might_sleep();
    mutex_lock(&p->lock);
    if (p->binding.closed || !p->binding.previous.valid ||
        p->binding.previous.ref.keypair != previous.keypair ||
        p->binding.previous.ref.generation != previous.generation)
        rv = p->binding.closed ? AF_CLOSED : AF_STALE_EPOCH;
    else {
        epoch = p->binding.previous.epoch;
        rv = aupe_registry_retire_previous(p->registry, p->handle, epoch);
        if (rv == AF_WAIT)
            rv = aupe_binding_retire_previous(&p->binding, previous);
    }
    mutex_unlock(&p->lock);
    return rv;
}

void aupe_crypto_peer_destroy(struct aupe_crypto_peer *p)
{
    if (!p || IS_ERR(p)) return;
    might_sleep();
    /* Owner has ALREADY quiesced all users of this pointer; this mutex
     * serializes the teardown against any last in-flight operation. */
    mutex_lock(&p->lock);
    aupe_binding_close(&p->binding);
    aupe_registry_revoke(p->registry, p->handle);
    mutex_unlock(&p->lock);
    memzero_explicit(&p->handle, sizeof(p->handle));
    kfree(p);
}
