/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AUPE_RS_LINUX_OWNER_H
#define AUPE_RS_LINUX_OWNER_H
#ifdef __KERNEL__
#include <linux/gfp_types.h>
#include <linux/types.h>
#include "aupe_rs_core.h"

/* Proof-sensitive entry points. The AUTHENTICATED public key must come from
 * a completed Noise handshake. A successfully AEAD-decrypted and pinned
 * keypair is required for verified_rx; this API DOES NOT VERIFY AEAD.
 * Every call is process-context and may sleep. Never call from softirq.
 * The owner protects the initial published pointer during acquire() and
 * unpublishes it before dropping the last reference. These functions are NOT
 * safe as a public network boundary; only the trusted crypto engine calls.
 * For callbacks: same-mutex non-reentrant, synchronous, no retained buffers.
 * This adapter is a Kbuild-compiled LINKAGE CANDIDATE ONLY until a compatible
 * Rust-for-Linux object is available; no module is shipped from this file.
 */
struct aupe_rs_linux_owner;
struct aupe_rs_linux_owner *aupe_rs_owner_create(gfp_t gfp, const af_u8 confirmed_pubkey[32]);
/* Only while publication lock/RCU guarantees pointer lifetime. */
struct aupe_rs_linux_owner *aupe_rs_owner_acquire(struct aupe_rs_linux_owner *owner);
/* Irreversible revoke. Existing pinned callers complete or fail closed. */
void aupe_rs_owner_close(struct aupe_rs_linux_owner *owner);
void aupe_rs_owner_put(struct aupe_rs_linux_owner *owner);
int aupe_rs_owner_install(struct aupe_rs_linux_owner *owner,
                          const af_u8 authenticated_pubkey[32],
                          struct aupe_crypto_ref pinned_ref, af_u64 now,
                          af_u64 *epoch);
int aupe_rs_owner_verified_rx(struct aupe_rs_linux_owner *owner,
                               struct aupe_crypto_ref aeadd_verified_ref,
                               af_u64 counter, af_u64 now,
                               const af_u8 *plaintext, af_u32 length,
                               af_u8 *scratch, af_u32 capacity,
                               aupe_rx_emit_fn emit, void *context);
int aupe_rs_owner_retire_previous(struct aupe_rs_linux_owner *owner,
                                  struct aupe_crypto_ref previous_ref);
#endif
#endif
