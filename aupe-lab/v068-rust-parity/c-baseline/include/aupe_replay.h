/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AUPE_REPLAY_H
#define AUPE_REPLAY_H
#include "aupe_frag.h"

/* Standalone transport-counter replay guard; NOT AEAD or Noise verification.
 * Owner passes only a confirmed local keypair epoch and an authenticated
 * transport counter. Caller serializes access (peer mutex in kernel).
 * One guard belongs to exactly one accepted keypair epoch.
 */
#define AUPE_REPLAY_WINDOW 8192U
#define AUPE_REPLAY_WORDS (AUPE_REPLAY_WINDOW / 64U)
#define AUPE_REPLAY_MAX_COUNTER (~(af_u64)0 - AUPE_REPLAY_WINDOW)
#define AUPE_REPLAY_DUPLICATE (-13)
#define AUPE_REPLAY_LIMIT (-14)
struct aupe_replay_window {
    af_u64 seen[AUPE_REPLAY_WORDS];
    af_u64 epoch, highest;
    af_u8 valid, initialized;
};
/* Wipes prior bitmap. Trusted epoch must be nonzero and newly confirmed. */
void aupe_replay_bind(struct aupe_replay_window *w, af_u64 epoch);
void aupe_replay_clear(struct aupe_replay_window *w);
/* AF_WAIT = accepted exactly once; negative = rejected, state untouched. */
int aupe_replay_accept(struct aupe_replay_window *w, af_u64 epoch,
                       af_u64 authenticated_counter);
#endif
