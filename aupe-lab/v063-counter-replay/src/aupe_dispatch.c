/* SPDX-License-Identifier: GPL-2.0-only */
#include "aupe_dispatch.h"
#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <stdint.h>
#include <string.h>
#endif

static void clear_scratch(af_u8 *p, af_u32 n)
{
#ifdef __KERNEL__
    memzero_explicit(p, n);
#else
    volatile af_u8 *dest = p;
    af_u32 i;
    for (i = 0; i < n; ++i)
        dest[i] = 0;
#endif
}

/* Require non-overlapping memory without undefined pointer relational
 * comparisons; code is used only on Linux where unsigned long can hold a
 * pointer in kernel, uintptr_t in userspace. */
static int buffers_overlap(const af_u8 *a, af_u32 an,
                           const af_u8 *b, af_u32 bn)
{
#ifdef __KERNEL__
    unsigned long pa = (unsigned long)a, pb = (unsigned long)b;
#else
    uintptr_t pa = (uintptr_t)a, pb = (uintptr_t)b;
#endif
    if (pa <= pb)
        return pb - pa < an;
    return pa - pb < bn;
}

static int dispatch_core(const struct aupe_rx_gate *gate, struct af_mux *mux,
                         struct aupe_replay_window *replay, af_u64 epoch,
                         af_u64 counter, af_u64 now, const af_u8 *pkt,
                         af_u32 n, af_u8 *scratch, af_u32 scratch_capacity,
                         aupe_rx_emit_fn emit, void *ctx)
{
    unsigned slot = AF_MUX_SLOTS;
    int r, size;
    if (!gate || !mux || !pkt || !scratch || !emit ||
        !n || n > AF_MAX_PACKET || scratch_capacity < AF_MAX_PACKET ||
        buffers_overlap(pkt, n, scratch, scratch_capacity))
        return AF_BAD;
    r = aupe_gate_check(gate, epoch);
    if (r != AF_WAIT)
        return r;
    if (replay) {
        r = aupe_replay_accept(replay, epoch, counter);
        if (r != AF_WAIT)
            return r;
    }
    r = af_mux_feed(mux, epoch, now, pkt, n, &slot);
    if (r == AF_BYPASS) {
        if (emit(ctx, pkt, n))
            return AF_BAD;
        return AF_BYPASS;
    }
    if (r != AF_DONE)
        return r;
    size = af_mux_take(mux, epoch, slot, scratch, scratch_capacity);
    if (size <= (int)AF_IPV4_HLEN)
        return size < 0 ? size : AF_BAD;
    r = emit(ctx, scratch, (af_u32)size) ? AF_BAD : AF_DONE;
    clear_scratch(scratch, (af_u32)size);
    return r;
}

int aupe_dispatch_verified(const struct aupe_rx_gate *gate, struct af_mux *mux,
                           af_u64 epoch, af_u64 now, const af_u8 *pkt,
                           af_u32 n, af_u8 *scratch, af_u32 scratch_capacity,
                           aupe_rx_emit_fn emit, void *ctx)
{
    /* Legacy laboratory helper: does not enforce replay; NEVER use as the
     * externally reachable RX entry point in a real VPN dataplane. */
    return dispatch_core(gate, mux, NULL, epoch, 0, now, pkt, n,
                         scratch, scratch_capacity, emit, ctx);
}
int aupe_dispatch_counter(const struct aupe_rx_gate *gate, struct af_mux *mux,
                          struct aupe_replay_window *replay, af_u64 epoch,
                          af_u64 counter, af_u64 now, const af_u8 *pkt,
                          af_u32 n, af_u8 *scratch, af_u32 scratch_capacity,
                          aupe_rx_emit_fn emit, void *ctx)
{
    if (!replay) return AF_BAD;
    return dispatch_core(gate, mux, replay, epoch, counter, now,
                         pkt, n, scratch, scratch_capacity, emit, ctx);
}
