/* SPDX-License-Identifier: GPL-2.0-only */
#include "aupe_crypto_binding.h"
#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

static int same_ref(struct aupe_crypto_ref a, struct aupe_crypto_ref b)
{
    return a.keypair == b.keypair && a.generation == b.generation;
}
static int static_key_equal(const af_u8 *a, const af_u8 *b)
{
    af_u8 diff = 0;
    unsigned i;
    for (i = 0; i < AUPE_STATIC_PUBLIC_KEY_BYTES; i++)
        diff |= a[i] ^ b[i];
    return diff == 0;
}
static void wipe_slot(struct aupe_binding_slot *s)
{
#ifdef __KERNEL__
    memzero_explicit(s, sizeof(*s));
#else
    volatile af_u8 *p = (volatile af_u8 *)s;
    unsigned i;
    for (i = 0; i < sizeof(*s); ++i) p[i] = 0;
#endif
}
static int valid_ref(struct aupe_crypto_ref ref)
{
    return ref.keypair != NULL && ref.generation != 0;
}
static int clock_ok(struct aupe_crypto_binding *b, af_u64 now)
{
    return !b->clock_started || now >= b->last_clock_ms;
}
static void advance_time(struct aupe_crypto_binding *b, af_u64 now)
{
    b->clock_started = 1;
    b->last_clock_ms = now;
    if (b->previous.valid && now >= b->previous_deadline_ms) {
        wipe_slot(&b->previous);
        b->previous_deadline_ms = 0;
    }
}
int aupe_binding_init(struct aupe_crypto_binding *b, const af_u8 pubkey[32])
{
    af_u8 nz = 0;
    unsigned i;
    if (!b || !pubkey || b->initialized || b->closed) return AF_BAD;
    for (i = 0; i < AUPE_STATIC_PUBLIC_KEY_BYTES; ++i) nz |= pubkey[i];
    if (!nz) return AF_BAD;
    memset(b, 0, sizeof(*b));
    memcpy(b->authenticated_public_key, pubkey, 32);
    b->initialized = 1;
    return AF_WAIT;
}
int aupe_binding_install(struct aupe_crypto_binding *b,
                         const af_u8 authenticated_pubkey[32],
                         struct aupe_crypto_ref ref, af_u64 now,
                         af_u64 *assigned_epoch)
{
    if (assigned_epoch) *assigned_epoch = 0;
    if (!b || !b->initialized || b->closed) return AF_CLOSED;
    if (!assigned_epoch || !authenticated_pubkey || !valid_ref(ref) ||
        !static_key_equal(b->authenticated_public_key, authenticated_pubkey) ||
        !clock_ok(b, now)) return AF_BAD;
    if (b->active.valid && same_ref(ref, b->active.ref)) {
        advance_time(b, now);
        *assigned_epoch = b->active.epoch;
        return AF_ALREADY_BOUND;
    }
    /* A duplicate/retired owner generation can NEVER create a new epoch. */
    if (ref.generation <= b->highest_owner_generation)
        return AF_STALE_EPOCH;
    if ((b->active.valid && b->active.ref.keypair == ref.keypair) ||
        (b->previous.valid && b->previous.ref.keypair == ref.keypair) ||
        b->highest_aupe_epoch == ~(af_u64)0 ||
        now > ~(af_u64)0 - AUPE_KEYPAIR_GRACE_MS) return AF_BAD;
    advance_time(b, now);
    wipe_slot(&b->previous);
    if (b->active.valid) {
        b->previous = b->active;
        b->previous_deadline_ms = now + AUPE_KEYPAIR_GRACE_MS;
    } else {
        b->previous_deadline_ms = 0;
    }
    b->highest_aupe_epoch++;
    b->highest_owner_generation = ref.generation;
    b->active.ref = ref;
    b->active.epoch = b->highest_aupe_epoch;
    b->active.valid = 1;
    *assigned_epoch = b->active.epoch;
    return AF_WAIT;
}
int aupe_binding_resolve(struct aupe_crypto_binding *b,
                         struct aupe_crypto_ref ref, af_u64 now,
                         af_u64 *trusted_epoch)
{
    if (trusted_epoch) *trusted_epoch = 0;
    if (!b || !b->initialized || b->closed) return AF_CLOSED;
    if (!trusted_epoch || !valid_ref(ref) || !clock_ok(b, now)) return AF_BAD;
    advance_time(b, now);
    if (b->active.valid && same_ref(ref, b->active.ref)) {
        *trusted_epoch = b->active.epoch;
        return AF_WAIT;
    }
    if (b->previous.valid && same_ref(ref, b->previous.ref)) {
        *trusted_epoch = b->previous.epoch;
        return AF_WAIT;
    }
    return AF_STALE_EPOCH;
}
int aupe_binding_retire_previous(struct aupe_crypto_binding *b,
                                 struct aupe_crypto_ref ref)
{
    if (!b || !b->initialized || b->closed) return AF_CLOSED;
    if (!valid_ref(ref) || !b->previous.valid ||
        !same_ref(ref, b->previous.ref)) return AF_STALE_EPOCH;
    wipe_slot(&b->previous);
    b->previous_deadline_ms = 0;
    return AF_WAIT;
}
void aupe_binding_close(struct aupe_crypto_binding *b)
{
    if (!b) return;
    wipe_slot(&b->active);
    wipe_slot(&b->previous);
#ifdef __KERNEL__
    memzero_explicit(b->authenticated_public_key, 32);
#else
    {
        volatile af_u8 *p = (volatile af_u8 *)b->authenticated_public_key;
        unsigned i;
        for (i = 0; i < 32; ++i) p[i] = 0;
    }
#endif
    b->highest_owner_generation = 0;
    b->highest_aupe_epoch = 0;
    b->previous_deadline_ms = 0;
    b->last_clock_ms = 0;
    b->initialized = 0;
    b->closed = 1;
}
