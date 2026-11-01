/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AUPE_CRYPTO_OWNER_H
#define AUPE_CRYPTO_OWNER_H
#ifdef __KERNEL__
#include "aupe_registry.h"
#include "aupe_crypto_binding.h"

/* Adapter for a kernel cryptographic OWNER, never a network-facing API.
 * The owner provides a pinned keypair pointer + locally assigned generation
 * only AFTER Noise handshake and its authenticated static public key.
 * The owner must verify AEAD with that SAME pinned keypair before verified_rx.
 * No UDP index, tunnel address, or wire packet may select an epoch.
 * All functions may sleep. Caller owns pointer lifetime, must quiesce users
 * before destroy and before destroying the RX registry.
 */
struct aupe_crypto_peer;
struct aupe_crypto_peer *aupe_crypto_peer_create(
    struct aupe_rx_registry *registry, af_u64 peer_handle,
    const af_u8 confirmed_public_key[32], gfp_t gfp);
/* Idempotent for exactly the same object + owner generation. A fresh
 * generation maps to an AUPE-assigned epoch (never chosen by the wire).
 */
int aupe_crypto_peer_install(struct aupe_crypto_peer *peer,
                             const af_u8 authenticated_public_key[32],
                             struct aupe_crypto_ref pinned_keypair,
                             af_u64 now_ms);
/* Invoked ONLY after successful decrypt+authentication; transport_counter
 * is from that authenticated transport message. The owner keeps keypair
 * pinned until this function returns. Passing fake/unverified data violates
 * the caller security contract; the bridge has no private decryption keys.
 */
int aupe_crypto_verified_rx(struct aupe_crypto_peer *peer,
                             struct aupe_crypto_ref decrypting_keypair,
                             af_u64 authenticated_counter, af_u64 now_ms,
                             const af_u8 *plaintext_ipv4, af_u32 packet_size,
                             af_u8 *scratch, af_u32 scratch_capacity,
                             aupe_rx_emit_fn emit, void *ctx);
int aupe_crypto_peer_retire(struct aupe_crypto_peer *peer,
                            struct aupe_crypto_ref previous);
/* Terminal. Blocks new calls; calls registry_revoke to tear down peer.
 * The owner MUST quiesce in-flight calls before destroy. */
void aupe_crypto_peer_destroy(struct aupe_crypto_peer *peer);
#endif
#endif
