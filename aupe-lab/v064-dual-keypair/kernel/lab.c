// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/string.h>
#include "aupe_registry.h"

static af_u8 smoke_scratch[AF_MAX_PACKET];
static const af_u8 smoke_ipv4[28] = {
    0x45,0x00,0x00,0x1c,0x00,0x01,0x00,0x00,0x40,0x11,
    0x1e,0xac,0xac,0x10,0x02,0x02,0xac,0x10,0x02,0x02,
    1,2,3,4,5,6,7,8
};
static int smoke_emitted(void *ctx, const af_u8 *packet, af_u32 length)
{
    int *count = ctx;
    if (length != sizeof(smoke_ipv4) ||
        memcmp(packet, smoke_ipv4, sizeof(smoke_ipv4)))
        return -EINVAL;
    ++*count;
    return 0;
}

/* Load-time smoke on an isolated lab module. No UDP, routes, or WG hooks.
 * A failed test fails the module load; all peer resources are released. */
static int __init aupe_lab_init(void)
{
    struct aupe_rx_registry *r;
    struct aupe_rx_peer *a, *b;
    af_u64 ah = 0, bh = 0;
    int err = -EINVAL, emitted = 0;

    r = aupe_registry_create(GFP_KERNEL);
    if (IS_ERR(r))
        return PTR_ERR(r);
    if (aupe_registry_add(r, GFP_KERNEL, &ah))
        goto done;
    if (aupe_registry_add(r, GFP_KERNEL, &bh))
        goto done;
    a = aupe_registry_acquire(r, ah);
    if (IS_ERR(a))
        goto done;
    if (aupe_registry_bind(r, ah, 101) != AF_WAIT)
        goto release_a;
    if (aupe_registry_bind(r, ah, 101) != AF_WAIT ||
        aupe_registry_bind(r, ah, 100) != AF_STALE_EPOCH)
        goto release_a;
    if (aupe_registry_revoke(r, ah))
        goto release_a;
    {
        struct aupe_rx_peer *unexpected = aupe_registry_acquire(r, ah);
        if (!IS_ERR(unexpected)) {
            aupe_rx_peer_put(unexpected);
            goto release_a;
        }
    }
    if (aupe_rx_peer_bind(a, 102) != AF_CLOSED)
        goto release_a;
    b = aupe_registry_acquire(r, bh);
    if (IS_ERR(b))
        goto release_a;
    if (aupe_registry_bind(r, bh, 202) == AF_WAIT &&
        aupe_registry_bind(r, bh, 202) == AF_WAIT &&
        aupe_registry_bind(r, bh, 201) == AF_STALE_EPOCH &&
        aupe_registry_bind(r, ah, 202) == AF_CLOSED &&
        aupe_rx_peer_bind(a, 202) == AF_CLOSED &&
        aupe_registry_dispatch_counter(r, bh, 202, 0, 100,
                               smoke_ipv4, sizeof(smoke_ipv4),
                               smoke_scratch, sizeof(smoke_scratch),
                               smoke_emitted, &emitted) == AF_BYPASS &&
        emitted == 1 &&
        aupe_registry_dispatch_counter(r, bh, 202, 0, 101,
                               smoke_ipv4, sizeof(smoke_ipv4),
                               smoke_scratch, sizeof(smoke_scratch),
                               smoke_emitted, &emitted) == AUPE_REPLAY_DUPLICATE &&
        emitted == 1 &&
        aupe_registry_dispatch_counter(r, ah, 202, 1, 101,
                               smoke_ipv4, sizeof(smoke_ipv4),
                               smoke_scratch, sizeof(smoke_scratch),
                               smoke_emitted, &emitted) == AF_CLOSED &&
        emitted == 1 &&
        aupe_registry_install_keypair(r, bh, 203, 1000) == AF_WAIT &&
        aupe_registry_install_keypair(r, bh, 203, 1000) == AF_WAIT &&
        aupe_registry_bind(r, bh, 204) == AF_STALE_EPOCH &&
        aupe_registry_dispatch_counter(r, bh, 203, 2, 1001,
                               smoke_ipv4, sizeof(smoke_ipv4),
                               smoke_scratch, sizeof(smoke_scratch),
                               smoke_emitted, &emitted) == AF_STALE_EPOCH &&
        aupe_registry_dispatch_keypair(r, bh, 203, 0, 1002,
                               smoke_ipv4, sizeof(smoke_ipv4),
                               smoke_scratch, sizeof(smoke_scratch),
                               smoke_emitted, &emitted) == AF_BYPASS &&
        aupe_registry_dispatch_keypair(r, bh, 203, 0, 1003,
                               smoke_ipv4, sizeof(smoke_ipv4),
                               smoke_scratch, sizeof(smoke_scratch),
                               smoke_emitted, &emitted) == AUPE_REPLAY_DUPLICATE &&
        aupe_registry_install_keypair(r, bh, 204, 2000) == AF_WAIT &&
        aupe_registry_dispatch_keypair(r, bh, 203, 1, 2001,
                               smoke_ipv4, sizeof(smoke_ipv4),
                               smoke_scratch, sizeof(smoke_scratch),
                               smoke_emitted, &emitted) == AF_BYPASS &&
        aupe_registry_dispatch_keypair(r, bh, 204, 0, 2002,
                               smoke_ipv4, sizeof(smoke_ipv4),
                               smoke_scratch, sizeof(smoke_scratch),
                               smoke_emitted, &emitted) == AF_BYPASS &&
        aupe_registry_retire_previous(r, bh, 203) == AF_WAIT &&
        aupe_registry_dispatch_keypair(r, bh, 203, 2, 2003,
                               smoke_ipv4, sizeof(smoke_ipv4),
                               smoke_scratch, sizeof(smoke_scratch),
                               smoke_emitted, &emitted) == AF_STALE_EPOCH &&
        emitted == 4)
        err = 0;
    aupe_rx_peer_put(b);
release_a:
    aupe_rx_peer_put(a);
done:
    aupe_registry_destroy(r);
    if (err)
        pr_err("aupe 0.5.64 kernel peer registry smoke failed\n");
    else
        pr_info("aupe 0.5.64 dual-keypair registry smoke OK; lab only, no dataplane\n");
    return err;
}
static void __exit aupe_lab_exit(void) { }
module_init(aupe_lab_init);
module_exit(aupe_lab_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AUPE 0.5.64 dual-keypair authenticated RX lab");
MODULE_VERSION("0.5.64");