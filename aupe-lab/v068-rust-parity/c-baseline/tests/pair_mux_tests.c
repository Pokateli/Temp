/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aupe_dispatch.h"

static unsigned long long count, fail;
#define CHECK(x) do { ++count; if (!(x)) { ++fail; fprintf(stderr, "pair mux:%d: %s\n", __LINE__, #x); } } while (0)
static struct af_state a, b, c, d;
static af_u8 scratch[AF_MAX_PACKET], pkt[64], delivered[AF_MAX_PACKET];
static unsigned delivered_count;
static af_u32 delivered_len;
static af_u16 checksum(const af_u8 *p)
{
    unsigned sum = 0, i;
    for (i = 0; i < 20; i += 2) sum += (unsigned)p[i] * 256U + p[i + 1];
    while (sum >> 16) sum = (sum & 65535U) + (sum >> 16);
    return (af_u16)~sum;
}
static af_u32 frag(unsigned id, unsigned off, int more, af_u8 fill)
{
    af_u16 flags = (af_u16)((off / 8U) | (more ? 0x2000U : 0U));
    af_u16 crc;
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = 0x45; pkt[2] = 0; pkt[3] = 28;
    pkt[4] = (af_u8)(id >> 8); pkt[5] = (af_u8)id;
    pkt[6] = (af_u8)(flags >> 8); pkt[7] = (af_u8)flags;
    pkt[8] = 64; pkt[9] = 17;
    pkt[12] = 172; pkt[13] = 16; pkt[15] = 2;
    pkt[16] = 172; pkt[17] = 16; pkt[19] = 2;
    memset(pkt + 20, fill, 8);
    crc = checksum(pkt); pkt[10] = (af_u8)(crc >> 8); pkt[11] = (af_u8)crc;
    return 28;
}
static int emit(void *ctx, const af_u8 *ipv4, af_u32 n)
{
    (void)ctx;
    if (!ipv4 || n > sizeof(delivered)) return -1;
    memcpy(delivered, ipv4, n);
    delivered_len = n;
    ++delivered_count;
    return 0;
}
static void test_isolation(void)
{
    struct aupe_pair_mux pair = {0};
    struct aupe_keypair_bank keys = {0};
    struct aupe_rx_gate gate = {0};
    af_u32 n;
    CHECK(aupe_pair_mux_init(&pair, &a, &b, &c, &d) == AF_WAIT);
    CHECK(aupe_gate_bind(&gate, 100) == AF_WAIT);
    CHECK(aupe_keypairs_install(&keys, 100, 100) == AF_WAIT);
    CHECK(aupe_pair_mux_rotate(&pair, 100) == AF_WAIT);
    n = frag(99, 0, 1, 0x11);
    CHECK(aupe_dispatch_keypair(&gate, &pair, &keys, 100, 0, 101,
          pkt, n, scratch, sizeof(scratch), emit, NULL) == AF_WAIT);
    CHECK(aupe_gate_bind(&gate, 101) == AF_WAIT);
    CHECK(aupe_keypairs_install(&keys, 101, 102) == AF_WAIT);
    CHECK(aupe_pair_mux_rotate(&pair, 101) == AF_WAIT);
    CHECK(pair.active.slot[0] != pair.previous.slot[0]);
    CHECK(pair.previous.slot[0]->payload[0] == 0x11);
    /* Identical src/dst/protocol/IP ID, different authenticated keypair. */
    n = frag(99, 0, 1, 0x22);
    CHECK(aupe_dispatch_keypair(&gate, &pair, &keys, 101, 0, 103,
          pkt, n, scratch, sizeof(scratch), emit, NULL) == AF_WAIT);
    CHECK(delivered_count == 0);
    CHECK(pair.previous.slot[0]->payload[0] == 0x11);
    CHECK(pair.active.slot[0]->payload[0] == 0x22);
    n = frag(99, 8, 0, 0x33);
    CHECK(aupe_dispatch_keypair(&gate, &pair, &keys, 100, 1, 104,
          pkt, n, scratch, sizeof(scratch), emit, NULL) == AF_DONE);
    CHECK(delivered_count == 1 && delivered_len == 36);
    CHECK(delivered[20] == 0x11 && delivered[28] == 0x33);
    CHECK(pair.active.slot[0]->payload[0] == 0x22);
    n = frag(99, 8, 0, 0x44);
    CHECK(aupe_dispatch_keypair(&gate, &pair, &keys, 101, 1, 105,
          pkt, n, scratch, sizeof(scratch), emit, NULL) == AF_DONE);
    CHECK(delivered_count == 2 && delivered_len == 36);
    CHECK(delivered[20] == 0x22 && delivered[28] == 0x44);
    CHECK(aupe_dispatch_keypair(&gate, &pair, &keys, 100, 1, 106,
          pkt, n, scratch, sizeof(scratch), emit, NULL) == AUPE_REPLAY_DUPLICATE);
    CHECK(delivered_count == 2);
    CHECK(aupe_keypairs_retire_previous(&keys, 100) == AF_WAIT);
    aupe_pair_mux_retire_previous(&pair);
    CHECK(aupe_dispatch_keypair(&gate, &pair, &keys, 100, 2, 107,
          pkt, n, scratch, sizeof(scratch), emit, NULL) == AF_STALE_EPOCH);
    CHECK(pair.previous.bound == 0);
    CHECK(aupe_pair_mux_rotate(&pair, 101) == AF_STALE_EPOCH);
    CHECK(aupe_gate_bind(&gate, 102) == AF_WAIT);
    CHECK(aupe_keypairs_install(&keys, 102, 108) == AF_WAIT);
    CHECK(aupe_pair_mux_rotate(&pair, 102) == AF_WAIT);
    CHECK(aupe_pair_mux_select(&pair, 101) == &pair.previous);
    CHECK(aupe_pair_mux_select(&pair, 102) == &pair.active);
    CHECK(aupe_pair_mux_select(&pair, 100) == NULL);
    CHECK(aupe_gate_bind(&gate, 103) == AF_WAIT);
    CHECK(aupe_keypairs_install(&keys, 103, 180109) == AF_WAIT);
    CHECK(aupe_pair_mux_rotate(&pair, 103) == AF_WAIT);
    CHECK(aupe_keypairs_accept(&keys, 102, 0, 360109) == AF_STALE_EPOCH);
    aupe_pair_mux_retire_previous(&pair);
    CHECK(pair.previous.bound == 0);
    aupe_pair_mux_clear(&pair);
    aupe_keypairs_clear(&keys);
    CHECK(pair.active.bound == 0 && pair.previous.bound == 0);
    CHECK(a.payload[0] == 0 && b.payload[0] == 0 &&
          c.payload[0] == 0 && d.payload[0] == 0);
}
static void test_many_rotations(void)
{
    struct aupe_pair_mux pair = {0};
    struct aupe_keypair_bank bank = {0};
    struct aupe_rx_gate gate = {0};
    unsigned i;
    CHECK(aupe_pair_mux_init(&pair, &a, &b, &c, &c) == AF_BAD);
    CHECK(aupe_pair_mux_init(&pair, &a, &b, &c, &d) == AF_WAIT);
    for (i = 1; i <= 40000; ++i) {
        af_u64 epoch = (af_u64)i;
        CHECK(aupe_gate_bind(&gate, epoch) == AF_WAIT);
        CHECK(aupe_keypairs_install(&bank, epoch, epoch) == AF_WAIT);
        CHECK(aupe_pair_mux_rotate(&pair, epoch) == AF_WAIT);
        CHECK(pair.active.epoch == epoch);
        if (i > 1) {
            CHECK(pair.previous.epoch == epoch - 1);
            CHECK(aupe_pair_mux_select(&pair, epoch - 1) == &pair.previous);
            CHECK(pair.active.slot[0] != pair.previous.slot[0]);
        }
        CHECK(aupe_pair_mux_select(&pair, epoch) == &pair.active);
    }
    aupe_pair_mux_clear(&pair); aupe_keypairs_clear(&bank);
}
int main(void)
{
    test_isolation();
    test_many_rotations();
    printf("AUPE keypair-scoped IPv4 fragment isolation: %llu checks, %llu failures\n", count, fail);
    return fail ? EXIT_FAILURE : EXIT_SUCCESS;
}
