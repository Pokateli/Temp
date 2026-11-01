/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AUPE_PEER_RX_H
#define AUPE_PEER_RX_H
#ifdef __KERNEL__
#include <linux/gfp.h>
#include "aupe_dispatch.h"

/* Process-context API. Every operation, including get/put, MAY SLEEP.
 * The owner MUST synchronize publishing/unpublishing the peer pointer with
 * new reference acquisitions. No get-after-free is safe without an owner lock.
 * The peer RX worker MUST hold a reference for every outstanding task.
 * AEAD authentication and replay-window validation MUST occur beforehand.
 */
struct aupe_rx_peer;
struct aupe_rx_stats {
    af_u64 rx_wait, rx_bypass, rx_reassembled, rx_drop;
    af_u64 rx_stale_epoch, rx_closed, rx_expired_slots, rx_replay_drop;
};
struct aupe_rx_peer *aupe_rx_peer_create(gfp_t gfp);
/* Returns a new reference or ERR_PTR(-ESHUTDOWN), -EINVAL; caller still
 * needs external synchronization during the initial pointer lookup. */
struct aupe_rx_peer *aupe_rx_peer_get(struct aupe_rx_peer *peer);
/* Release one reference. The final put frees and scrubs workspaces. */
void aupe_rx_peer_put(struct aupe_rx_peer *peer);
/* Terminal, idempotent revocation: new get/bind/ingest/take will fail.
 * Existing pinned tasks remain safe to finish or fail closed. */
void aupe_rx_peer_close(struct aupe_rx_peer *peer);
/* Compatibility with 0.5.59: close + release OWNER's reference once.
 * Call only after the owner's pointer is unpublished; never twice. */
void aupe_rx_peer_destroy(struct aupe_rx_peer *peer);
int aupe_rx_peer_bind(struct aupe_rx_peer *peer, af_u64 trusted_epoch);
void aupe_rx_peer_unbind(struct aupe_rx_peer *peer);
int aupe_rx_peer_feed(struct aupe_rx_peer *peer, af_u64 trusted_epoch,
                      af_u64 monotonic_ms, const af_u8 *inner_ipv4,
                      af_u32 packet_bytes, unsigned *ready_slot);
int aupe_rx_peer_take(struct aupe_rx_peer *peer, af_u64 trusted_epoch,
                      unsigned slot, af_u8 *out, af_u32 out_capacity);
/* Preferred path: one lock around feed + take, forbids rotation mid-delivery.
 * AF_DONE => rebuilt_bytes >20; AF_BYPASS => caller forwards *original*
 * authenticated IPv4. On other outcomes output is unused. Buffers disjoint. */
int aupe_rx_peer_ingest(struct aupe_rx_peer *peer, af_u64 trusted_epoch,
                        af_u64 monotonic_ms, const af_u8 *inner_ipv4,
                        af_u32 packet_bytes, af_u8 *out,
                        af_u32 out_capacity, af_u32 *rebuilt_bytes);
/* Preferred authenticated-dataplane boundary. Only trusted post-AEAD and
 * post-replay callers may call it. Emits only validated BYPASS or assembled
 * packets while holding the peer mutex: teardown/rotation cannot interleave.
 * The callback must NOT re-enter peer/registry APIs or retain input pointers.
 * It may sleep; use a process-context worker, never softirq.
 * For reassembled packets, scratch is wiped immediately after callback. */
int aupe_rx_peer_dispatch(struct aupe_rx_peer *peer, af_u64 trusted_epoch,
                          af_u64 monotonic_ms, const af_u8 *inner_ipv4,
                          af_u32 packet_bytes, af_u8 *scratch,
                          af_u32 scratch_capacity, aupe_rx_emit_fn emit,
                          void *emit_context);
/* Preferred laboratory boundary for authenticated transport counters.
 * The real cryptographic engine still verifies AEAD and binds the keypair.
 */
int aupe_rx_peer_dispatch_counter(struct aupe_rx_peer *peer,
                                  af_u64 trusted_epoch, af_u64 counter,
                                  af_u64 monotonic_ms, const af_u8 *inner_ipv4,
                                  af_u32 packet_bytes, af_u8 *scratch,
                                  af_u32 scratch_capacity, aupe_rx_emit_fn emit,
                                  void *emit_context);
int aupe_rx_peer_expire(struct aupe_rx_peer *peer, af_u64 trusted_epoch,
                        af_u64 monotonic_ms);
/* Privacy-preserving per-peer counters, captured under owner lock. */
int aupe_rx_peer_snapshot(struct aupe_rx_peer *peer,
                          struct aupe_rx_stats *out);
#endif
#endif
