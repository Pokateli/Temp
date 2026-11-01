/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AUPE_HANDLES_H
#define AUPE_HANDLES_H
#include "aupe_frag.h"

/* Handle identifies an authenticated owner-bound peer slot. NOT a credential.
 * The owner MUST serialize every table operation; this pure core does not lock.
 * A 56-bit non-wrapping generation plus a 1-based slot identifier prevents
 * accidental reuse after peer replacement, including duplicate inner IPs.
 */
#define AUPE_HANDLE_SLOTS 64U
#define AUPE_HANDLE_GEN_MAX ((~(af_u64)0) >> 8)
#define AUPE_HANDLE_SEED_MASK ((af_u64)0xffffffffffffULL)
#define AUPE_HANDLE_FULL (-11)
#define AUPE_HANDLE_MISSING (-12)
struct aupe_handle_slot {
    void *peer;
    af_u64 generation;
};
struct aupe_handle_table {
    struct aupe_handle_slot slots[AUPE_HANDLE_SLOTS];
};
void aupe_handles_init(struct aupe_handle_table *table);
/* Owner may seed a new registry from a trusted local CSPRNG. Never a client.
 * 48-bit seed leaves >= 2^56-2^48 generations before exhaustion. */
void aupe_handles_init_seed(struct aupe_handle_table *table, af_u64 seed);
int aupe_handles_publish(struct aupe_handle_table *table, void *peer,
                         af_u64 *new_handle);
void *aupe_handles_lookup(const struct aupe_handle_table *table,
                          af_u64 handle);
void *aupe_handles_remove(struct aupe_handle_table *table, af_u64 handle);
/* For owner-controlled table teardown; must exclude all concurrent users. */
void *aupe_handles_drain(struct aupe_handle_table *table, unsigned index);
#endif
