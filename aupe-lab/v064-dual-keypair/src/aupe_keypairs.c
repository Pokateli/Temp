/* SPDX-License-Identifier: GPL-2.0-only */
#include "aupe_keypairs.h"
#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

static void scrub(void *ptr, af_u32 n)
{
#ifdef __KERNEL__
    memzero_explicit(ptr, n);
#else
    volatile af_u8 *p = (volatile af_u8 *)ptr;
    af_u32 i;
    for (i = 0; i < n; ++i) p[i] = 0;
#endif
}
static void slot_clear(struct aupe_keypair_slot *s)
{
    aupe_replay_clear(&s->replay);
    scrub(s, (af_u32)sizeof(*s));
}
void aupe_keypairs_clear(struct aupe_keypair_bank *b)
{
    if (b) {
        slot_clear(&b->active);
        slot_clear(&b->previous);
        scrub(b, (af_u32)sizeof(*b));
    }
}

int aupe_keypairs_install(struct aupe_keypair_bank *b, af_u64 epoch,
                          af_u64 now_ms)
{
    if (!b || !epoch || now_ms > (~(af_u64)0 - AUPE_KEYPAIR_GRACE_MS))
        return AF_BAD;
    if (b->clock_started && now_ms < b->last_now_ms)
        return AF_BAD;
    if (b->active.valid && b->active.epoch == epoch)
        return AF_ALREADY_BOUND;
    if (epoch <= b->high_watermark)
        return AF_STALE_EPOCH;

    /* Only one previous slot; evict any older key before publication. */
    slot_clear(&b->previous);
    if (b->active.valid) {
        b->previous = b->active;
        b->previous_deadline_ms = now_ms + AUPE_KEYPAIR_GRACE_MS;
    } else {
        b->previous_deadline_ms = 0;
    }
    /* No transient reuse: current replay window always starts empty. */
    scrub(&b->active, (af_u32)sizeof(b->active));
    aupe_replay_bind(&b->active.replay, epoch);
    b->active.epoch = epoch;
    b->active.valid = 1;
    b->high_watermark = epoch;
    b->last_now_ms = now_ms;
    b->clock_started = 1;
    b->enabled = 1;
    return AF_WAIT;
}

int aupe_keypairs_accept(struct aupe_keypair_bank *b, af_u64 epoch,
                         af_u64 counter, af_u64 now_ms)
{
    struct aupe_keypair_slot *s;
    if (!b || !b->enabled || !epoch)
        return AF_STALE_EPOCH;
    if (b->clock_started && now_ms < b->last_now_ms)
        return AF_BAD;
    b->last_now_ms = now_ms;
    if (b->previous.valid && now_ms >= b->previous_deadline_ms) {
        slot_clear(&b->previous);
        b->previous_deadline_ms = 0;
    }
    if (b->active.valid && b->active.epoch == epoch)
        s = &b->active;
    else if (b->previous.valid && b->previous.epoch == epoch)
        s = &b->previous;
    else
        return AF_STALE_EPOCH;
    return aupe_replay_accept(&s->replay, epoch, counter);
}

int aupe_keypairs_retire_previous(struct aupe_keypair_bank *b,
                                  af_u64 previous_epoch)
{
    if (!b || !b->enabled || !previous_epoch)
        return AF_BAD;
    if (!b->previous.valid || b->previous.epoch != previous_epoch)
        return AF_STALE_EPOCH;
    slot_clear(&b->previous);
    b->previous_deadline_ms = 0;
    return AF_WAIT;
}
