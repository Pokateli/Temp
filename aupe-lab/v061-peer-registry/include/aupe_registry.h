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
/* Terminal revocation: removes handle first, closes peer, drops owner ref.
 * Previously acquired references remain valid memory but reject new RX. */
int aupe_registry_revoke(struct aupe_rx_registry *registry, af_u64 handle);
/* Owner-only shutdown, after all registry call-sites quiesced. Outstanding
 * pinned peers may outlive this registry and are freed by their final put. */
void aupe_registry_destroy(struct aupe_rx_registry *registry);
#endif
#endif
