/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AUPE_FRAG_H
#define AUPE_FRAG_H
#ifdef __KERNEL__
#include <linux/types.h>
typedef u8 af_u8;
typedef u16 af_u16;
typedef u32 af_u32;
typedef u64 af_u64;
#else
#include <stdint.h>
typedef uint8_t af_u8;
typedef uint16_t af_u16;
typedef uint32_t af_u32;
typedef uint64_t af_u64;
#endif

/* No per-fragment allocations. The owner allocates/owns this object outside
 * RX/softirq context, serializes all calls, and releases it on peer teardown.
 * Workspaces are never shared between peers or Noise sessions. */
#define AF_MAX_PAYLOAD 65515U
#define AF_MAX_PACKET (AF_MAX_PAYLOAD + 20U)
#define AF_BITS_BYTES ((AF_MAX_PAYLOAD + 7U) / 8U)
#define AF_TIMEOUT_MS 5000ULL
/* A generous upper bound for ordinary-MTU IPv4; caps tiny-fragment DoS. */
#define AF_MAX_FRAGMENTS 256U
#define AF_IPV4_HLEN 20U

/* Returns: AF_WAIT, AF_DONE, AF_BYPASS; negative errors are reject/drop. */
enum af_result {
    AF_WAIT = 0,
    AF_DONE = 1,
    AF_BYPASS = 2,
    AF_BAD = -1,
    AF_OVERLAP = -2,
    AF_OTHER_DATAGRAM = -3,
    AF_STALE_EPOCH = -4,
    AF_EXPIRED = -5,
    AF_NOT_READY = -6,
    AF_ECN_CONFLICT = -7,
    AF_FRAGMENT_LIMIT = -8,
    AF_CLOSED = -10, /* terminal peer lifecycle, cannot rebind */
    AF_ALREADY_BOUND = 3, /* duplicate active Noise activation: no reset */
};

struct af_state {
    af_u8 payload[AF_MAX_PAYLOAD];
    af_u8 seen[AF_BITS_BYTES];
    af_u8 first_header[AF_IPV4_HLEN];
    af_u64 epoch;
    af_u64 expiry_ms;
    af_u32 src, dst;
    af_u32 received_bytes, furthest_end;
    af_u16 ident, last_end, accepted_frags;
    af_u8 protocol, ecn_mask;
    af_u8 active, has_last, has_first, complete, bound;
};

/* Call bind at secure session creation/rotation, unbind at teardown.
 * Both scrub plaintext; epoch is trusted and nonzero, never from a packet/IP. */
void af_bind_epoch(struct af_state *s, af_u64 epoch);
void af_unbind(struct af_state *s);
/* Discard only the current incomplete datagram, preserving trusted epoch. */
void af_reset(struct af_state *s);
/* Optional owner-driven timer hook for idle/undelivered datagrams. */
int af_expire(struct af_state *s, af_u64 trusted_epoch, af_u64 monotonic_ms);
/* Caller must validate AEAD and anti-replay BEFORE this function. */
int af_feed_ipv4(struct af_state *s, af_u64 trusted_epoch,
                 af_u64 monotonic_ms, const af_u8 *packet,
                 af_u32 packet_bytes);
/* Copy completed IPv4 packet; return >0 packet size, or AF_NOT_READY/AF_BAD.
 * Output must be at least AF_MAX_PACKET bytes, in separate non-overlapping storage. */
int af_take_ipv4(struct af_state *s, af_u8 *out, af_u32 out_capacity);
#endif