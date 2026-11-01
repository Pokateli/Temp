/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AUPE_PAIR_MUX_H
#define AUPE_PAIR_MUX_H
#include "aupe_mux.h"
/* Both current/previous keypairs get their OWN two-flow IPv4 assembly.
 * Four separate workspaces must be allocated outside the RX path.
 * Owner must serialize all calls; pointers are not borrowed past close.
 */
struct aupe_pair_mux {
    struct af_mux active, previous;
    struct af_state *workspace[4];
};
int aupe_pair_mux_init(struct aupe_pair_mux *p, struct af_state *a,
                       struct af_state *b, struct af_state *c,
                       struct af_state *d);
/* Valid only for a strictly increasing authenticated local keypair epoch.
 * The previous active mux and all pending fragments survive rotation.
 * Any older previous mux is scrubbed. No allocation occurs here.
 */
int aupe_pair_mux_rotate(struct aupe_pair_mux *p, af_u64 new_epoch);
void aupe_pair_mux_retire_previous(struct aupe_pair_mux *p);
void aupe_pair_mux_clear(struct aupe_pair_mux *p);
struct af_mux *aupe_pair_mux_select(struct aupe_pair_mux *p, af_u64 epoch);
#endif
