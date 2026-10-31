/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AUPE_PEER_REGISTRY_H
#define AUPE_PEER_REGISTRY_H
#ifdef __KERNEL__
#include <linux/gfp.h>
#include "aupe_peer_rx.h"
#include "aupe_handles.h"

/* Process-context only; all entry points may sleep. Registry identity is
 * scoped to one AUPE instance/netns. It never selects peers by tunnel IP.
 * External owner MUST block new registry calls and wait for in-progress
 * calls BEFORE destroy: registry pointer itself is not RCU-protected.
 * A handle is a routing reference, NOT proof of AEAD or anti-replay.
 */
struct aupe_rx_registry;
struct aupe_rx_registry *aupe_registry_create(gfp_t gfp);
/* Creates a peer with one registry-owned reference. No peer pointer escapes. */
int aupe_registry_add(struct aupe_rx_registry *registry, gfp_t gfp,
                      af_u64 *new_handle);
/* Obtains a pinned reference under registry mutex; caller must call put(). */
struct aupe_rx_peer *aupe_registry_acquire(struct aupe_rx_registry *registry,
                                         af_u64 handle);
/* AEAD + anti-replay verified by the *trusted owner* before this call.
 * handle/epoch are not authentication proof. The registry pins the peer under
 * mutex; callback is invoked under peer lock, before revoke/rotation can run.
 * Scratch must be disjoint from plaintext and large enough for reassembly.
 * Only process context. Missing or revoked handles fail closed. */
int aupe_registry_dispatch(struct aupe_rx_registry *registry, af_u64 handle,
                            af_u64 trusted_epoch, af_u64 monotonic_ms,
                            const af_u8 *inner_ipv4, af_u32 packet_bytes,
                            af_u8 *scratch, af_u32 scratch_capacity,
                            aupe_rx_emit_fn emit, void *emit_context);
/* Confirmed Noise rotation only, not per packet. Duplicate is idempotent;
 * epoch rollback is rejected even after unbind. */
int aupe_registry_bind(struct aupe_rx_registry *registry, af_u64 handle,
                       af_u64 trusted_epoch);
/* Terminal revocation: removes handle first, closes peer, drops owner ref.
 * Previously acquired references remain valid memory but reject new RX. */
int aupe_registry_revoke(struct aupe_rx_registry *registry, af_u64 handle);
/* Owner-only shutdown, after all registry call-sites quiesced. Outstanding
 * pinned peers may outlive this registry and are freed by their final put. */
void aupe_registry_destroy(struct aupe_rx_registry *registry);
#endif
#endif
