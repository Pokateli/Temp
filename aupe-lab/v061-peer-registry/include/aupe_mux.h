/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AUPE_MUX_H
#define AUPE_MUX_H
#include "aupe_frag.h"

/* Both workspace objects MUST be provisioned at peer creation, outside RX.
 * Per-peer cap: exactly two slots, each sizeof(struct af_state). No RX alloc.
 * Each peer owns its own mux and slots, never shared by inner IP/subnet.
 * All entry points require peer-owned serialization and peer lifetime pinning.
 * A lock is not embedded here because the kernel caller may be in softirq.
 * Memory wiping is O(sizeof(struct af_state)) and may require deferring RX.
 */
#define AF_MUX_SLOTS 2U
#define AF_MUX_BUSY (-9)

struct af_mux {
    struct af_state *slot[AF_MUX_SLOTS];
    af_u64 epoch;
    af_u8 bound;
};

/* bind takes ownership of NO memory. All pointers must outlive mux lifetime;
 * successful (re)bind destroys both existing datagrams, even on same epoch. */
int af_mux_bind(struct af_mux *m, struct af_state *a,
                struct af_state *b, af_u64 trusted_epoch);
/* Destroy datagrams and revoke session epoch, before freeing slots. */
void af_mux_unbind(struct af_mux *m);
/* On each already authenticated and anti-replay-checked inner IPv4 packet.
 * If AF_DONE, *ready_slot identifies ONLY the complete packet's storage.
 * AF_BYPASS means deliver original validated unfragmented packet unchanged.
 * AF_MUX_BUSY never evicts existing in-flight/completed packets. */
int af_mux_feed(struct af_mux *m, af_u64 trusted_epoch, af_u64 monotonic_ms,
                const af_u8 *packet, af_u32 packet_bytes, unsigned *ready_slot);
/* On AF_DONE, after ref/lifetime validation and while owner holds peer lock. */
int af_mux_take(struct af_mux *m, af_u64 trusted_epoch, unsigned ready_slot,
                af_u8 *out, af_u32 capacity);
/* Maintenance timer; returns number of expired (incomplete or completed).
 * Returns AF_STALE_EPOCH on invalid binding. */
int af_mux_expire(struct af_mux *m, af_u64 trusted_epoch,
                  af_u64 monotonic_ms);
#endif
