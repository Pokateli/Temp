/* SPDX-License-Identifier: GPL-2.0-only */
#include "aupe_replay.h"
#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

void aupe_replay_clear(struct aupe_replay_window *w)
{
    if (!w) return;
#ifdef __KERNEL__
    memzero_explicit(w, sizeof(*w));
#else
    /* Prevent optimizer eliding plaintext-adjacent session state scrubbing. */
    volatile af_u8 *p = (volatile af_u8 *)w;
    af_u32 i;
    for (i = 0; i < sizeof(*w); ++i) p[i] = 0;
#endif
}
void aupe_replay_bind(struct aupe_replay_window *w, af_u64 epoch)
{
    if (!w) return;
    aupe_replay_clear(w);
    if (epoch) { w->epoch = epoch; w->valid = 1; }
}
static void mark(struct aupe_replay_window *w, af_u64 counter)
{
    const unsigned slot = (unsigned)(counter % AUPE_REPLAY_WINDOW);
    w->seen[slot / 64U] |= 1ULL << (slot % 64U);
}
static int marked(const struct aupe_replay_window *w, af_u64 counter)
{
    const unsigned slot = (unsigned)(counter % AUPE_REPLAY_WINDOW);
    return (w->seen[slot / 64U] & (1ULL << (slot % 64U))) != 0;
}
int aupe_replay_accept(struct aupe_replay_window *w, af_u64 epoch,
                       af_u64 counter)
{
    af_u64 delta;
    if (!w || !epoch || !w->valid || w->epoch != epoch)
        return AF_STALE_EPOCH;
    /* Never allow counter arithmetic to approach wrap-around. WG engines
     * must independently enforce their own REJECT_AFTER_MESSAGES limit. */
    if (counter >= AUPE_REPLAY_MAX_COUNTER)
        return AUPE_REPLAY_LIMIT;
    if (!w->initialized) {
        w->initialized = 1;
        w->highest = counter;
        mark(w, counter);
        return AF_WAIT;
    }
    if (counter <= w->highest) {
        if (w->highest - counter >= AUPE_REPLAY_WINDOW || marked(w, counter))
            return AUPE_REPLAY_DUPLICATE;
        mark(w, counter);
        return AF_WAIT;
    }
    delta = counter - w->highest;
    if (delta >= AUPE_REPLAY_WINDOW)
        memset(w->seen, 0, sizeof(w->seen));
    else {
        af_u64 k;
        /* Ring slots for the newly opened range are cleared; counters
         * remaining within the window retain their accepted bits. */
        for (k = 1; k <= delta; ++k) {
            unsigned slot = (unsigned)((w->highest + k) % AUPE_REPLAY_WINDOW);
            w->seen[slot / 64U] &= ~(1ULL << (slot % 64U));
        }
    }
    w->highest = counter;
    mark(w, counter);
    return AF_WAIT;
}
