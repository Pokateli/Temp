/* SPDX-License-Identifier: GPL-2.0-only */
#include "aupe_handles.h"
#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

static unsigned decode_slot(af_u64 handle)
{
    unsigned low = (unsigned)(handle & 255ULL);
    if (low == 0 || low > AUPE_HANDLE_SLOTS || (handle >> 8) == 0)
        return AUPE_HANDLE_SLOTS;
    return low - 1;
}

void aupe_handles_init(struct aupe_handle_table *table)
{
    if (table)
        memset(table, 0, sizeof(*table));
}

void aupe_handles_init_seed(struct aupe_handle_table *table, af_u64 seed)
{
    unsigned i;
    if (!table)
        return;
    aupe_handles_init(table);
    seed &= AUPE_HANDLE_SEED_MASK;
    for (i = 0; i < AUPE_HANDLE_SLOTS; ++i)
        table->slots[i].generation = seed;
}

int aupe_handles_publish(struct aupe_handle_table *table, void *peer,
                         af_u64 *new_handle)
{
    unsigned i;
    if (!table || !peer || !new_handle)
        return AF_BAD;
    *new_handle = 0;
    for (i = 0; i < AUPE_HANDLE_SLOTS; ++i) {
        struct aupe_handle_slot *s = &table->slots[i];
        if (s->peer || s->generation == AUPE_HANDLE_GEN_MAX)
            continue;
        /* Fail closed on exhaustion: never wrap a live or historical handle. */
        ++s->generation;
        s->peer = peer;
        *new_handle = (s->generation << 8) | (af_u64)(i + 1);
        return AF_WAIT;
    }
    return AUPE_HANDLE_FULL;
}

void *aupe_handles_lookup(const struct aupe_handle_table *table,
                          af_u64 handle)
{
    unsigned idx = decode_slot(handle);
    const struct aupe_handle_slot *s;
    if (!table || idx == AUPE_HANDLE_SLOTS)
        return NULL;
    s = &table->slots[idx];
    if (s->generation != (handle >> 8))
        return NULL;
    return s->peer;
}

void *aupe_handles_remove(struct aupe_handle_table *table, af_u64 handle)
{
    unsigned idx = decode_slot(handle);
    void *peer;
    if (!table || idx == AUPE_HANDLE_SLOTS ||
        !aupe_handles_lookup(table, handle))
        return NULL;
    peer = table->slots[idx].peer;
    table->slots[idx].peer = NULL;
    return peer;
}

void *aupe_handles_drain(struct aupe_handle_table *table, unsigned index)
{
    void *peer;
    if (!table || index >= AUPE_HANDLE_SLOTS)
        return NULL;
    peer = table->slots[index].peer;
    table->slots[index].peer = NULL;
    return peer;
}