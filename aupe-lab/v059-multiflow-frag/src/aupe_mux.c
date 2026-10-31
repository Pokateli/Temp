/* SPDX-License-Identifier: GPL-2.0-only */
#include "aupe_mux.h"
#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

static af_u16 mux_be16(const af_u8 *p)
{
    return (af_u16)((af_u16)p[0] << 8 | p[1]);
}
static af_u32 mux_be32(const af_u8 *p)
{
    return (af_u32)p[0] << 24 | (af_u32)p[1] << 16 |
           (af_u32)p[2] << 8 | p[3];
}
static int mux_valid(const struct af_mux *m, af_u64 epoch)
{
    return m && m->bound && epoch && epoch == m->epoch &&
           m->slot[0] && m->slot[1] && m->slot[0] != m->slot[1];
}
static int mux_valid_ipv4(const af_u8 *p, af_u32 n)
{
    af_u32 i, sum = 0;
    af_u16 frag;
    if (!p || n < AF_IPV4_HLEN || n > 65535U || p[0] != 0x45 ||
        mux_be16(p + 2) != n)
        return AF_BAD;
    for (i = 0; i < AF_IPV4_HLEN; i += 2)
        sum += mux_be16(p + i);
    while (sum >> 16)
        sum = (sum & 0xffffU) + (sum >> 16);
    if (sum != 0xffffU)
        return AF_BAD;
    frag = mux_be16(p + 6);
    if ((frag & 0x8000U) || ((frag & 0x4000U) && (frag & 0x3fffU)))
        return AF_BAD;
    return (frag & 0x3fffU) ? AF_WAIT : AF_BYPASS;
}

int af_mux_bind(struct af_mux *m, struct af_state *a,
                struct af_state *b, af_u64 epoch)
{
    if (!m || !a || !b || a == b || !epoch)
        return AF_BAD;
    /* Caller cannot change slot pointers while bound: prevents abandoned
     * buffers keeping plaintext or surviving a key epoch replacement. */
    if (m->bound && (m->slot[0] != a || m->slot[1] != b))
        return AF_BAD;
    af_bind_epoch(a, epoch);
    af_bind_epoch(b, epoch);
    m->slot[0] = a;
    m->slot[1] = b;
    m->epoch = epoch;
    m->bound = 1;
    return AF_WAIT;
}
void af_mux_unbind(struct af_mux *m)
{
    if (!m)
        return;
    if (m->slot[0])
        af_unbind(m->slot[0]);
    if (m->slot[1] && m->slot[1] != m->slot[0])
        af_unbind(m->slot[1]);
    m->epoch = 0;
    m->bound = 0;
    m->slot[0] = 0;
    m->slot[1] = 0;
}
int af_mux_expire(struct af_mux *m, af_u64 epoch, af_u64 now)
{
    unsigned i;
    int expired = 0;
    if (!mux_valid(m, epoch))
        return AF_STALE_EPOCH;
    for (i = 0; i < AF_MUX_SLOTS; i++)
        if (af_expire(m->slot[i], epoch, now) == AF_EXPIRED)
            expired++;
    return expired;
}
int af_mux_feed(struct af_mux *m, af_u64 epoch, af_u64 now,
                const af_u8 *pkt, af_u32 n, unsigned *ready_slot)
{
    int kind, target = -1, free_slot = -1, rv;
    unsigned i;
    af_u16 id;
    af_u32 src, dst;
    af_u8 proto;
    if (!mux_valid(m, epoch))
        return AF_STALE_EPOCH;
    if (!ready_slot)
        return AF_BAD;
    *ready_slot = AF_MUX_SLOTS;
    kind = mux_valid_ipv4(pkt, n);
    if (kind != AF_WAIT)
        return kind; /* AF_BYPASS preserves both in-flight datagrams. */
    if (af_mux_expire(m, epoch, now) == AF_STALE_EPOCH)
        return AF_STALE_EPOCH;
    id = mux_be16(pkt + 4);
    src = mux_be32(pkt + 12);
    dst = mux_be32(pkt + 16);
    proto = pkt[9];
    for (i = 0; i < AF_MUX_SLOTS; i++) {
        struct af_state *s = m->slot[i];
        if (s->active && s->src == src && s->dst == dst &&
            s->ident == id && s->protocol == proto) {
            target = (int)i;
            break;
        }
        if (!s->active && free_slot < 0)
            free_slot = (int)i;
    }
    if (target < 0)
        target = free_slot;
    if (target < 0)
        return AF_MUX_BUSY;
    rv = af_feed_ipv4(m->slot[target], epoch, now, pkt, n);
    if (rv == AF_DONE)
        *ready_slot = (unsigned)target;
    return rv;
}
int af_mux_take(struct af_mux *m, af_u64 epoch, unsigned slot,
                af_u8 *out, af_u32 capacity)
{
    if (!mux_valid(m, epoch))
        return AF_STALE_EPOCH;
    if (slot >= AF_MUX_SLOTS)
        return AF_BAD;
    return af_take_ipv4(m->slot[slot], out, capacity);
}
