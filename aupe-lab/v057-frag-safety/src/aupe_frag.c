/* SPDX-License-Identifier: GPL-2.0-only */
#include "aupe_frag.h"
#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

static af_u16 be16(const af_u8 *p)
{
    return (af_u16)(((af_u16)p[0] << 8) | p[1]);
}
static af_u32 be32(const af_u8 *p)
{
    return ((af_u32)p[0] << 24) | ((af_u32)p[1] << 16) |
           ((af_u32)p[2] << 8) | p[3];
}
static void set16(af_u8 *p, af_u16 v)
{
    p[0] = (af_u8)(v >> 8);
    p[1] = (af_u8)v;
}
static af_u16 ip_checksum(const af_u8 *p, af_u32 len)
{
    af_u32 sum = 0;
    af_u32 i;
    for (i = 0; i + 1U < len; i += 2U)
        sum += be16(p + i);
    if (i < len)
        sum += (af_u32)p[i] << 8;
    while (sum >> 16)
        sum = (sum & 0xffffU) + (sum >> 16);
    return (af_u16)~sum;
}
static int present(const struct af_state *s, af_u32 p)
{
    return !!(s->seen[p >> 3] & (af_u8)(1U << (p & 7U)));
}
static void mark(struct af_state *s, af_u32 p)
{
    s->seen[p >> 3] |= (af_u8)(1U << (p & 7U));
}

void af_reset(struct af_state *s)
{
    if (!s)
        return;
    /* Keep epoch, clear only state/bitmap, not uninitialized payload bytes. */
    memset(s->seen, 0, sizeof(s->seen));
    memset(s->first_header, 0, sizeof(s->first_header));
    s->expiry_ms = 0;
    s->src = 0;
    s->dst = 0;
    s->received_bytes = 0;
    s->furthest_end = 0;
    s->ident = 0;
    s->last_end = 0;
    s->protocol = 0;
    s->active = 0;
    s->has_last = 0;
    s->has_first = 0;
    s->complete = 0;
}
static void af_wipe_payload(struct af_state *s)
{
#ifdef __KERNEL__
    memzero_explicit(s->payload, sizeof(s->payload));
#else
    volatile af_u8 *dst = s->payload;
    af_u32 i;
    for (i = 0; i < sizeof(s->payload); ++i)
        dst[i] = 0;
#endif
}
void af_bind_epoch(struct af_state *s, af_u64 epoch)
{
    if (!s)
        return;
    af_reset(s);
    af_wipe_payload(s);
    s->epoch = epoch;
    s->bound = !!epoch;
}
void af_unbind(struct af_state *s)
{
    if (!s)
        return;
    af_reset(s);
    af_wipe_payload(s);
    s->epoch = 0;
    s->bound = 0;
}

int af_feed_ipv4(struct af_state *s, af_u64 trusted_epoch,
                 af_u64 now, const af_u8 *pkt, af_u32 n)
{
    af_u16 total, f, off8, id;
    af_u32 src, dst, start, len, end, i;
    af_u8 proto;
    int more;
    if (!s || !pkt || n < AF_IPV4_HLEN)
        return AF_BAD;
    if (!s->bound || !trusted_epoch || s->epoch != trusted_epoch)
        return AF_STALE_EPOCH;
    if (s->complete)
        return AF_NOT_READY; /* Owner must take completed result first. */
    if (s->active && (af_u64)(now - s->expiry_ms) < (1ULL << 63)) {
        af_reset(s);
        return AF_EXPIRED;
    }
    /* Version, IHL: deliberately reject IPv4 options in this lab candidate. */
    if (pkt[0] != 0x45U)
        return AF_BAD;
    total = be16(pkt + 2);
    if (total < AF_IPV4_HLEN || total != n || ip_checksum(pkt, AF_IPV4_HLEN))
        return AF_BAD;
    f = be16(pkt + 6);
    if (f & 0x8000U) /* Reserved flag must be zero. */
        return AF_BAD;
    if (f & 0x4000U) { /* DF and fragments together are contradictory. */
        if ((f & 0x3fffU) != 0U)
            return AF_BAD;
        return AF_BYPASS;
    }
    off8 = (af_u16)(f & 0x1fffU);
    more = !!(f & 0x2000U);
    if (!more && !off8)
        return AF_BYPASS;
    len = total - AF_IPV4_HLEN;
    if (!len || (more && (len & 7U)))
        return AF_BAD;
    start = (af_u32)off8 * 8U;
    if (start > AF_MAX_PAYLOAD || len > AF_MAX_PAYLOAD - start)
        return AF_BAD;
    end = start + len;
    id = be16(pkt + 4);
    src = be32(pkt + 12);
    dst = be32(pkt + 16);
    proto = pkt[9];
    if (s->active) {
        /* IP identity never authenticates a peer. Bind state to trusted epoch. */
        if (s->src != src || s->dst != dst || s->ident != id ||
            s->protocol != proto)
            return AF_OTHER_DATAGRAM; /* Keep original in-flight packet. */
    } else {
        s->src = src;
        s->dst = dst;
        s->ident = id;
        s->protocol = proto;
        s->expiry_ms = now + AF_TIMEOUT_MS;
        s->active = 1;
    }
    if (s->has_last && end > s->last_end) {
        af_reset(s);
        return AF_BAD;
    }
    if (!more) {
        if (s->has_last && s->last_end != end) {
            af_reset(s);
            return AF_BAD;
        }
        /* An earlier fragment might already extend beyond this final end. */
        if (s->furthest_end > end) {
            af_reset(s);
            return AF_BAD;
        }
        s->last_end = (af_u16)end;
        s->has_last = 1;
    }
    /* Conservative IPv4 policy: drop on ANY overlap, even exact duplicates. */
    for (i = start; i < end; i++) {
        if (present(s, i)) {
            af_reset(s);
            return AF_OVERLAP;
        }
    }
    for (i = start; i < end; i++)
        mark(s, i);
    memcpy(s->payload + start, pkt + AF_IPV4_HLEN, len);
    s->received_bytes += len;
    if (end > s->furthest_end)
        s->furthest_end = end;
    if (!start) {
        memcpy(s->first_header, pkt, AF_IPV4_HLEN);
        s->has_first = 1;
    }
    if (s->has_last && s->has_first && s->received_bytes == s->last_end) {
        s->complete = 1;
        return AF_DONE;
    }
    return AF_WAIT;
}

int af_take_ipv4(struct af_state *s, af_u8 *out, af_u32 capacity)
{
    af_u32 size;
    if (!s || !out || !s->complete || !s->has_first || !s->has_last)
        return AF_NOT_READY;
    size = AF_IPV4_HLEN + s->last_end;
    if (size > capacity || size > 65535U)
        return AF_BAD;
    memcpy(out, s->first_header, AF_IPV4_HLEN);
    set16(out + 2, (af_u16)size);
    set16(out + 6, 0); /* Clear MF + offset; no DF on a fragmented packet. */
    set16(out + 10, 0);
    set16(out + 10, ip_checksum(out, AF_IPV4_HLEN));
    memcpy(out + AF_IPV4_HLEN, s->payload, s->last_end);
    af_reset(s);
    return (int)size;
}
