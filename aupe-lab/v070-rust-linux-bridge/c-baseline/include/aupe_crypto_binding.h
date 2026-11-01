/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AUPE_CRYPTO_BINDING_H
#define AUPE_CRYPTO_BINDING_H
#include "aupe_keypairs.h"

/* The real cryptographic owner supplies these values from its private,
 * authenticated keypair object. No wire-provided index, address, handshake
 * field or user supplied handle is a valid keypair identity. The pointer MUST
 * remain pinned throughout a call and be retired before freeing its object.
 * generation is an owner-managed monotonic keypair serial (not a WG counter).
 * AUPE assigns its OWN epoch, so packets cannot select epochs themselves.
 */
#define AUPE_STATIC_PUBLIC_KEY_BYTES 32U
struct aupe_crypto_ref {
    const void *keypair;
    af_u64 generation;
};
struct aupe_binding_slot {
    struct aupe_crypto_ref ref;
    af_u64 epoch;
    af_u8 valid;
};
struct aupe_crypto_binding {
    af_u8 authenticated_public_key[AUPE_STATIC_PUBLIC_KEY_BYTES];
    struct aupe_binding_slot active, previous;
    af_u64 highest_owner_generation;
    af_u64 highest_aupe_epoch;
    af_u64 previous_deadline_ms;
    af_u64 last_clock_ms;
    af_u8 initialized, closed, clock_started;
};
/* Must be called exactly once on a zeroed, newly allocated object. The
 * authenticated_public_key must be copied from a completed Noise handshake.
 */
int aupe_binding_init(struct aupe_crypto_binding *b, const af_u8 pubkey[32]);
/* Public key compare is constant-time. Caller MUST have authenticated it. */
int aupe_binding_install(struct aupe_crypto_binding *b,
                         const af_u8 authenticated_pubkey[32],
                         struct aupe_crypto_ref ref, af_u64 monotonic_ms,
                         af_u64 *assigned_epoch);
/* Only call after successful AEAD with the pinned decrypting keypair.
 * Return epoch to the owner-internal RX adapter; NEVER to an untrusted caller.
 */
int aupe_binding_resolve(struct aupe_crypto_binding *b,
                         struct aupe_crypto_ref decrypting_keypair,
                         af_u64 monotonic_ms, af_u64 *trusted_epoch);
int aupe_binding_retire_previous(struct aupe_crypto_binding *b,
                                 struct aupe_crypto_ref previous);
/* Irreversible terminal close; wipes identifiers and key-related metadata. */
void aupe_binding_close(struct aupe_crypto_binding *b);
#endif
