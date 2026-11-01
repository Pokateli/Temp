// SPDX-License-Identifier: GPL-2.0-only
/* Linux-only, process-context owner for the AUPE 0.5.70 no_std Rust core.
 * Object compile tested on Debian 6.12 Kbuild; NOT linked/loadable until
 * CONFIG_RUST and exact kernel Rust ABI are available. See KERNEL_STATUS.md.
 */
#include <linux/atomic.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/refcount.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include "aupe_rs_linux_owner.h"

#define AUPE_RS_MAX_LIVE_PEERS 64
#define AUPE_RS_WORKSPACES 4
static atomic_t live_rs_peers = ATOMIC_INIT(0);

struct aupe_rs_linux_owner {
    struct mutex mutex;
    refcount_t refs;
    bool revoked;
    struct aupe_rs_kernel_peer *core;
    struct af_state *ws[AUPE_RS_WORKSPACES];
    size_t core_size, ws_size;
};

static void aupe_rs_free(struct aupe_rs_linux_owner *o)
{
    int i;
    if (!o)
        return;
    for (i = 0; i < AUPE_RS_WORKSPACES; ++i)
        if (o->ws[i])
            kvfree_sensitive(o->ws[i], o->ws_size);
    if (o->core)
        kvfree_sensitive(o->core, o->core_size);
    memzero_explicit(o, sizeof(*o));
    kfree(o);
    atomic_dec(&live_rs_peers);
}

struct aupe_rs_linux_owner *aupe_rs_owner_create(gfp_t gfp,
                                                 const af_u8 key[32])
{
    struct aupe_rs_linux_owner *o;
    size_t csize, wsize, calign, walign;
    int i, rv;

    if (!key)
        return ERR_PTR(-EINVAL);
    might_sleep();
    if (atomic_inc_return(&live_rs_peers) > AUPE_RS_MAX_LIVE_PEERS) {
        atomic_dec(&live_rs_peers);
        return ERR_PTR(-EDQUOT);
    }
    csize = aupe_rs_peer_size();
    wsize = aupe_rs_workspace_size();
    calign = aupe_rs_peer_align();
    walign = aupe_rs_workspace_align();
    /* Kernel allocations meet this minimum alignment; do not silently
     * accept a changed future Rust struct requiring stronger alignment. */
    if (!csize || !wsize || csize > 65536 || wsize > 131072 ||
        calign > __alignof__(unsigned long long) ||
        walign > __alignof__(unsigned long long) ||
        !is_power_of_2(calign) || !is_power_of_2(walign)) {
        atomic_dec(&live_rs_peers);
        return ERR_PTR(-EINVAL);
    }
    o = kzalloc(sizeof(*o), gfp);
    if (!o)
        goto fail_quota;
    o->core_size = csize;
    o->ws_size = wsize;
    mutex_init(&o->mutex);
    refcount_set(&o->refs, 1);
    o->core = kvzalloc(csize, gfp);
    if (!o->core)
        goto fail_obj;
    for (i = 0; i < AUPE_RS_WORKSPACES; ++i) {
        o->ws[i] = kvzalloc(wsize, gfp);
        if (!o->ws[i])
            goto fail_obj;
    }
    rv = aupe_rs_peer_init(o->core, key, o->ws[0], o->ws[1], o->ws[2], o->ws[3]);
    if (rv != AF_WAIT)
        goto fail_obj;
    return o;
fail_obj:
    aupe_rs_free(o);
    return ERR_PTR(-ENOMEM);
fail_quota:
    atomic_dec(&live_rs_peers);
    return ERR_PTR(-ENOMEM);
}

struct aupe_rs_linux_owner *aupe_rs_owner_acquire(struct aupe_rs_linux_owner *o)
{
    if (!o || IS_ERR(o))
        return ERR_PTR(-EINVAL);
    might_sleep();
    /* External owner publication lock prevents simultaneous final free. */
    if (!refcount_inc_not_zero(&o->refs))
        return ERR_PTR(-ESHUTDOWN);
    mutex_lock(&o->mutex);
    if (o->revoked) {
        mutex_unlock(&o->mutex);
        aupe_rs_owner_put(o);
        return ERR_PTR(-ESHUTDOWN);
    }
    mutex_unlock(&o->mutex);
    return o;
}

void aupe_rs_owner_close(struct aupe_rs_linux_owner *o)
{
    if (!o || IS_ERR(o))
        return;
    might_sleep();
    mutex_lock(&o->mutex);
    if (!o->revoked) {
        o->revoked = true;
        aupe_rs_peer_close(o->core);
    }
    mutex_unlock(&o->mutex);
}

void aupe_rs_owner_put(struct aupe_rs_linux_owner *o)
{
    if (!o || IS_ERR(o))
        return;
    might_sleep();
    if (!refcount_dec_and_test(&o->refs))
        return;
    /* Owner was unpublished; this is its final reference. */
    aupe_rs_owner_close(o);
    aupe_rs_free(o);
}

int aupe_rs_owner_install(struct aupe_rs_linux_owner *o,
                          const af_u8 key[32], struct aupe_crypto_ref ref,
                          af_u64 now, af_u64 *epoch)
{
    int rv;
    if (!o || IS_ERR(o) || !key || !epoch)
        return AF_BAD;
    might_sleep();
    *epoch = 0;
    mutex_lock(&o->mutex);
    rv = o->revoked ? AF_CLOSED :
         aupe_rs_peer_install(o->core, key, ref, now, epoch);
    mutex_unlock(&o->mutex);
    return rv;
}

int aupe_rs_owner_verified_rx(struct aupe_rs_linux_owner *o,
                               struct aupe_crypto_ref ref,
                               af_u64 counter, af_u64 now,
                               const af_u8 *pkt, af_u32 n,
                               af_u8 *scratch, af_u32 capacity,
                               aupe_rx_emit_fn emit, void *context)
{
    int rv;
    if (!o || IS_ERR(o))
        return AF_BAD;
    might_sleep();
    mutex_lock(&o->mutex);
    rv = o->revoked ? AF_CLOSED :
         aupe_rs_peer_verified_rx(o->core, ref, counter, now, pkt,
                                  n, scratch, capacity, emit, context);
    mutex_unlock(&o->mutex);
    return rv;
}

int aupe_rs_owner_retire_previous(struct aupe_rs_linux_owner *o,
                                  struct aupe_crypto_ref ref)
{
    int rv;
    if (!o || IS_ERR(o))
        return AF_BAD;
    might_sleep();
    mutex_lock(&o->mutex);
    rv = o->revoked ? AF_CLOSED : aupe_rs_peer_retire_previous(o->core, ref);
    mutex_unlock(&o->mutex);
    return rv;
}
