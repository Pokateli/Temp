// SPDX-License-Identifier: GPL-2.0-only
#include <linux/module.h>
#include <linux/init.h>
#include "aupe_frag.h"
/* Link-test only. No UDP socket, hooks, routes or userspace API are created. */
static int __init af_lab_init(void)
{
    pr_info("aupe frag 0.5.58 lab core linked; no dataplane registered\n");
    return 0;
}
static void __exit af_lab_exit(void) { }
module_init(af_lab_init);
module_exit(af_lab_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AUPE 0.5.58 fragment safety link-only laboratory");
MODULE_VERSION("0.5.58");
