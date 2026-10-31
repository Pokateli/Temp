// SPDX-License-Identifier: GPL-2.0-only
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include "aupe_registry.h"

/* Load-time smoke on an isolated lab module. No UDP, routes, or WG hooks.
 * A failed test fails the module load; all peer resources are released. */
static int __init aupe_lab_init(void)
{
    struct aupe_rx_registry *r;
    struct aupe_rx_peer *a, *b;
    af_u64 ah = 0, bh = 0;
    int err = -EINVAL;

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
    if (aupe_rx_peer_bind(a, 101) != AF_WAIT)
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
    if (aupe_rx_peer_bind(b, 202) == AF_WAIT &&
        aupe_rx_peer_bind(a, 202) == AF_CLOSED)
        err = 0;
    aupe_rx_peer_put(b);
release_a:
    aupe_rx_peer_put(a);
done:
    aupe_registry_destroy(r);
    if (err)
        pr_err("aupe 0.5.61 kernel peer registry smoke failed\n");
    else
        pr_info("aupe 0.5.61 peer registry smoke OK; lab only, no dataplane\n");
    return err;
}
static void __exit aupe_lab_exit(void) { }
module_init(aupe_lab_init);
module_exit(aupe_lab_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AUPE 0.5.61 peer registry and fragment RX lab");
MODULE_VERSION("0.5.61");
