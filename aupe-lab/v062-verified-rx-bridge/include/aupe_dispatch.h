/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AUPE_DISPATCH_H
#define AUPE_DISPATCH_H
#include "aupe_gate.h"
#include "aupe_mux.h"

/* Caller has already validated Noise/AEAD and replay protection, has pinned
 * the correct peer, and holds its serialization lock. A handle and IP address
 * are NOT crypto proof. Must only be called from trusted process-context RX.
 * Callback is synchronous, must not reenter peer/registry or retain pointer.
 * Scratch must not overlap input packet; at least AF_MAX_PACKET bytes.
 */
typedef int (*aupe_rx_emit_fn)(void *context, const af_u8 *ipv4, af_u32 size);
int aupe_dispatch_verified(const struct aupe_rx_gate *gate, struct af_mux *mux,
                           af_u64 trusted_epoch, af_u64 monotonic_ms,
                           const af_u8 *inner_ipv4, af_u32 bytes,
                           af_u8 *scratch, af_u32 scratch_capacity,
                           aupe_rx_emit_fn emit, void *emit_context);
#endif
