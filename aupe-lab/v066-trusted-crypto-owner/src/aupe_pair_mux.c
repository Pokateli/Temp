/* SPDX-License-Identifier: GPL-2.0-only */
#include "aupe_pair_mux.h"
#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif
int aupe_pair_mux_init(struct aupe_pair_mux *p, struct af_state *a,
                       struct af_state *b, struct af_state *c,
                       struct af_state *d)
{
    struct af_state *s[4] = {a, b, c, d};
    unsigned i, j;
    if (!p) return AF_BAD;
    for (i = 0; i < 4; ++i) {
        if (!s[i]) return AF_BAD;
        for (j = 0; j < i; ++j)
            if (s[i] == s[j]) return AF_BAD;
    }
    memset(p, 0, sizeof(*p));
    for (i = 0; i < 4; ++i) p->workspace[i] = s[i];
    return AF_WAIT;
}
int aupe_pair_mux_rotate(struct aupe_pair_mux *p, af_u64 epoch)
{
    struct af_mux old_active;
    struct af_state *a, *b;
    if (!p || !p->workspace[0] || !epoch) return AF_BAD;
    if (p->active.bound && epoch <= p->active.epoch)
        return AF_STALE_EPOCH;
    if (p->previous.bound) af_mux_unbind(&p->previous);
    if (p->active.bound) {
        old_active = p->active;
        /* Double-buffer two complete muxes; no cross-keypair overlap. */
        if (old_active.slot[0] == p->workspace[0]) {
            a = p->workspace[2]; b = p->workspace[3];
        } else if (old_active.slot[0] == p->workspace[2]) {
            a = p->workspace[0]; b = p->workspace[1];
        } else return AF_BAD;
        p->previous = old_active;
    } else {
        a = p->workspace[0]; b = p->workspace[1];
    }
    memset(&p->active, 0, sizeof(p->active));
    return af_mux_bind(&p->active, a, b, epoch);
}
void aupe_pair_mux_retire_previous(struct aupe_pair_mux *p)
{
    if (p) af_mux_unbind(&p->previous);
}
void aupe_pair_mux_clear(struct aupe_pair_mux *p)
{
    if (!p) return;
    af_mux_unbind(&p->active);
    af_mux_unbind(&p->previous);
}
struct af_mux *aupe_pair_mux_select(struct aupe_pair_mux *p, af_u64 epoch)
{
    if (!p || !epoch) return NULL;
    if (p->active.bound && p->active.epoch == epoch)
        return &p->active;
    if (p->previous.bound && p->previous.epoch == epoch)
        return &p->previous;
    return NULL;
}
