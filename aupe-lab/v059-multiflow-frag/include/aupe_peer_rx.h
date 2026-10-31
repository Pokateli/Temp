/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AUPE_PEER_RX_H
#define AUPE_PEER_RX_H
#ifdef __KERNEL__
#include <linux/gfp.h>
#include "aupe_mux.h"

/* A provisional adapter for an authenticated AUPE peer's inner IPv4 RX.
 * These functions MAY SLEEP: invoke only from a process-context worker,
 * never a UDP/Netfilter softirq, NAPI poll, or atomic/RCU-only context.
 * Caller MUST ensure the peer and its memory remain alive through each call,
 * and that ciphertext AEAD + anti-replay succeeded before feeding plaintext.
 */
struct aupe_rx_peer;
struct aupe_rx_peer *aupe_rx_peer_create(gfp_t gfp);
/* Caller must stop/drain all RX workers and expiry callbacks first. */
void aupe_rx_peer_destroy(struct aupe_rx_peer *peer);
/* Called only after trusted Noise session promotion. Zero is invalid. */
int aupe_rx_peer_bind(struct aupe_rx_peer *peer, af_u64 epoch);
/* Stop all new RX, scrub and revoke epoch prior to destruction. */
void aupe_rx_peer_unbind(struct aupe_rx_peer *peer);
/* Low-level process-context API: AF_DONE returns a ready slot in *ready_slot.
 * AF_BYPASS indicates caller should route original inner IPv4 packet. */
int aupe_rx_peer_feed(struct aupe_rx_peer *peer, af_u64 trusted_epoch,
                      af_u64 monotonic_ms, const af_u8 *inner_ipv4,
                      af_u32 packet_bytes, unsigned *ready_slot);
int aupe_rx_peer_take(struct aupe_rx_peer *peer, af_u64 trusted_epoch,
                      unsigned slot, af_u8 *out, af_u32 out_capacity);
/* Preferred production-facing path: feed + take hold the SAME peer lock.
 * On AF_DONE, *rebuilt_bytes tells the exact new IPv4 packet length in out;
 * for AF_WAIT, AF_BYPASS, or a negative drop, *rebuilt_bytes is zero.
 * On AF_BYPASS, caller forwards the ORIGINAL already authenticated packet.
 * out must be disjoint from inner_ipv4 and sufficiently large (65535 bytes).
 * Do not infer a peer from the inner packet src/dst addresses. */
int aupe_rx_peer_ingest(struct aupe_rx_peer *peer, af_u64 trusted_epoch,
                        af_u64 monotonic_ms, const af_u8 *inner_ipv4,
                        af_u32 packet_bytes, af_u8 *out,
                        af_u32 out_capacity, af_u32 *rebuilt_bytes);
int aupe_rx_peer_expire(struct aupe_rx_peer *peer, af_u64 trusted_epoch,
                        af_u64 monotonic_ms);
#endif
#endif
