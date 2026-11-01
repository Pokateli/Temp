/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AUPE_DISPATCH_H
#define AUPE_DISPATCH_H
#include "aupe_gate.h"
#include "aupe_mux.h"
#include "aupe_replay.h"
#include "aupe_keypairs.h"

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
/* Stronger lab entry: authenticated counter must originate from a verified
 * WG/AWG transport packet (post-AEAD). The window is peer/epoch scoped and
 * caller serialized. Duplicate counters cannot invoke delivery.
 */
int aupe_dispatch_counter(const struct aupe_rx_gate *gate, struct af_mux *mux,
                          struct aupe_replay_window *replay, af_u64 epoch,
                          af_u64 counter, af_u64 now, const af_u8 *pkt,
                          af_u32 n, af_u8 *scratch, af_u32 scratch_capacity,
                          aupe_rx_emit_fn emit, void *ctx);
/* Only for caller-authenticated keypair epochs. The verified transport
 * owner has already completed AEAD. Serializes under peer mutex in kernel. */
int aupe_dispatch_keypair(const struct aupe_rx_gate *gate, struct af_mux *mux,
                          struct aupe_keypair_bank *bank, af_u64 keypair_epoch,
                          af_u64 counter, af_u64 now, const af_u8 *pkt,
                          af_u32 n, af_u8 *scratch, af_u32 scratch_capacity,
                          aupe_rx_emit_fn emit, void *ctx);
#endif
