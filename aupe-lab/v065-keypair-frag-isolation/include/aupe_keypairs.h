/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AUPE_KEYPAIRS_H
#define AUPE_KEYPAIRS_H
#include "aupe_replay.h"

/* A *local* monotonic keypair generation, assigned by the authenticated
 * cryptographic owner, never derived from an unauthenticated packet or IP.
 * The bank is exclusively accessed with the peer mutex held.
 * Previous keypair has a bounded grace period and can be revoked earlier.
 */
#define AUPE_KEYPAIR_GRACE_MS 180000ULL
struct aupe_keypair_slot {
    struct aupe_replay_window replay;
    af_u64 epoch;
    af_u8 valid;
};
struct aupe_keypair_bank {
    struct aupe_keypair_slot active, previous;
    af_u64 high_watermark;
    af_u64 previous_deadline_ms;
    af_u64 last_now_ms;
    af_u8 clock_started;
    af_u8 enabled;
};
/* Return AF_WAIT for a fresh keypair, AF_ALREADY_BOUND for identical event,
 * AF_STALE_EPOCH for rollback, AF_BAD for invalid/time-regressing event.
 * The caller MUST first ensure the corresponding gate and mux can bind.
 */
int aupe_keypairs_install(struct aupe_keypair_bank *b, af_u64 epoch,
                          af_u64 now_ms);
/* No replay acceptance for an expired/retired epoch. Checks trusted now_ms. */
int aupe_keypairs_accept(struct aupe_keypair_bank *b, af_u64 epoch,
                         af_u64 counter, af_u64 now_ms);
/* Retire previous without affecting current. Current may only be retired
 * by closing/unbinding the entire peer. */
int aupe_keypairs_retire_previous(struct aupe_keypair_bank *b,
                                  af_u64 previous_epoch);
void aupe_keypairs_clear(struct aupe_keypair_bank *b);
#endif
