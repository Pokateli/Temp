/* SPDX-License-Identifier: GPL-2.0-only */
#include "aupe_gate.h"

int aupe_gate_bind(struct aupe_rx_gate *g, af_u64 epoch)
{
    if (!g || !epoch)
        return AF_BAD;
    if (g->state == AUPE_GATE_CLOSED)
        return AF_CLOSED;
    if (g->state == AUPE_GATE_ACTIVE && g->epoch == epoch)
        return AF_ALREADY_BOUND;
    /* Reject replayed/delayed activation, even following unbind. */
    if (epoch <= g->high_watermark)
        return AF_STALE_EPOCH;
    g->high_watermark = epoch;
    g->epoch = epoch;
    g->state = AUPE_GATE_ACTIVE;
    return AF_WAIT;
}
void aupe_gate_unbind(struct aupe_rx_gate *g)
{
    if (g && g->state != AUPE_GATE_CLOSED) {
        g->epoch = 0;
        g->state = AUPE_GATE_UNBOUND;
    }
}
int aupe_gate_check(const struct aupe_rx_gate *g, af_u64 epoch)
{
    if (!g)
        return AF_BAD;
    if (g->state == AUPE_GATE_CLOSED)
        return AF_CLOSED;
    return g->state == AUPE_GATE_ACTIVE && epoch && epoch == g->epoch
           ? AF_WAIT : AF_STALE_EPOCH;
}
void aupe_gate_close(struct aupe_rx_gate *g)
{
    if (g) {
        g->epoch = 0;
        g->high_watermark = 0;
        g->state = AUPE_GATE_CLOSED;
    }
}