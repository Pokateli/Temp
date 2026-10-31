/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AUPE_RX_GATE_H
#define AUPE_RX_GATE_H
#include "aupe_frag.h"

/* Only the authenticated peer owner may bind a trusted Noise epoch.
 * These simple state transitions MUST be serialized by the owner. They
 * do not perform authentication, decrypt transport, or check replay.
 */
enum aupe_rx_gate_state {
    AUPE_GATE_UNBOUND = 0,
    AUPE_GATE_ACTIVE = 1,
    AUPE_GATE_CLOSED = 2,
};
struct aupe_rx_gate {
    af_u64 epoch;
    af_u8 state;
};
int aupe_gate_bind(struct aupe_rx_gate *g, af_u64 confirmed_epoch);
void aupe_gate_unbind(struct aupe_rx_gate *g);
int aupe_gate_check(const struct aupe_rx_gate *g, af_u64 packet_epoch);
void aupe_gate_close(struct aupe_rx_gate *g);
#endif
