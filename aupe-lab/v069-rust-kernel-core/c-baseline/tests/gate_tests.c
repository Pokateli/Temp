/* SPDX-License-Identifier: GPL-2.0-only */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aupe_gate.h"
#include "aupe_mux.h"

static unsigned checks, failed;
#define T(x) do { checks++; if (!(x)) { \
    fprintf(stderr, "gate:%d failed: %s\n", __LINE__, #x); failed++; \
} } while (0)
static unsigned be16_checksum(const af_u8 *b)
{
    unsigned sum = 0, i;
    for (i = 0; i < 20; i += 2)
        sum += (unsigned)b[i] * 256U + b[i+1];
    while (sum >> 16)
        sum = (sum & 65535U) + (sum >> 16);
    return (~sum) & 65535U;
}
static unsigned fragment(af_u8 *b, unsigned id, unsigned off, int mf, unsigned len)
{
    unsigned i, n = 20 + len, checksum;
    memset(b, 0, n);
    b[0] = 0x45;
    b[1] = 2;
    b[2] = (af_u8)(n >> 8); b[3] = (af_u8)n;
    b[4] = (af_u8)(id >> 8); b[5] = (af_u8)id;
    b[6] = (af_u8)(((off / 8) >> 8) | (mf ? 0x20 : 0));
    b[7] = (af_u8)((off / 8) & 255);
    b[8] = 64; b[9] = 17;
    b[12] = 172; b[13] = 16; b[15] = 2;
    b[16] = 172; b[17] = 16; b[19] = 2; /* identical inner IP */
    for (i = 0; i < len; i++) b[20+i] = (af_u8)((off+i+id)%251);
    checksum = be16_checksum(b);
    b[10] = (af_u8)(checksum >> 8); b[11] = (af_u8)checksum;
    return n;
}
static void simple_contract(void)
{
    struct aupe_rx_gate g = {0};
    T(aupe_gate_check(&g,1) == AF_STALE_EPOCH);
    T(aupe_gate_bind(&g,0) == AF_BAD);
    T(aupe_gate_bind(&g,101) == AF_WAIT);
    T(aupe_gate_check(&g,101) == AF_WAIT);
    T(aupe_gate_bind(&g,101) == AF_ALREADY_BOUND);
    T(aupe_gate_bind(&g,100) == AF_STALE_EPOCH);
    T(aupe_gate_check(&g,102) == AF_STALE_EPOCH);
    T(aupe_gate_bind(&g,102) == AF_WAIT);
    T(aupe_gate_check(&g,101) == AF_STALE_EPOCH);
    T(aupe_gate_check(&g,102) == AF_WAIT);
    T(aupe_gate_bind(&g,101) == AF_STALE_EPOCH);
    aupe_gate_unbind(&g);
    T(g.epoch == 0 && g.state == AUPE_GATE_UNBOUND);
    T(aupe_gate_check(&g,102) == AF_STALE_EPOCH);
    T(aupe_gate_bind(&g,102) == AF_STALE_EPOCH);
    T(aupe_gate_bind(&g,103) == AF_WAIT);
    aupe_gate_close(&g);
    T(g.epoch == 0 && g.state == AUPE_GATE_CLOSED);
    T(aupe_gate_bind(&g,104) == AF_CLOSED);
    T(aupe_gate_check(&g,103) == AF_CLOSED);
    aupe_gate_unbind(&g);
    T(g.state == AUPE_GATE_CLOSED && g.epoch == 0);
    aupe_gate_close(&g);
    T(g.state == AUPE_GATE_CLOSED);
    T(aupe_gate_check(NULL,100) == AF_BAD);
}
static void crypto_epoch_fence(void)
{
    static struct af_state a, b;
    struct af_mux m = {0};
    struct aupe_rx_gate g = {0};
    af_u8 pkt[65535], out[65535];
    unsigned slot = AF_MUX_SLOTS, n;
    T(aupe_gate_bind(&g,31) == AF_WAIT);
    T(af_mux_bind(&m,&a,&b,31) == AF_WAIT);
    n = fragment(pkt,501,0,1,8);
    T(aupe_gate_check(&g,31) == AF_WAIT);
    T(af_mux_feed(&m,31,5,pkt,n,&slot)==AF_WAIT);
    T(aupe_gate_bind(&g,31) == AF_ALREADY_BOUND);
    T(a.active && a.received_bytes == 8);
    T(aupe_gate_bind(&g,30) == AF_STALE_EPOCH);
    /* Owner rotates Noise; previous plaintext is always scrubbed. */
    T(aupe_gate_bind(&g,32) == AF_WAIT);
    T(af_mux_bind(&m,&a,&b,32) == AF_WAIT);
    T(a.payload[0] == 0 && b.payload[0] == 0);
    n = fragment(pkt,501,8,0,3);
    T(aupe_gate_check(&g,31)==AF_STALE_EPOCH);
    T(af_mux_feed(&m,31,6,pkt,n,&slot)==AF_STALE_EPOCH);
    /* A fragment from new key cannot complete prior epoch's datagram. */
    T(aupe_gate_check(&g,32)==AF_WAIT);
    T(af_mux_feed(&m,32,7,pkt,n,&slot)==AF_WAIT);
    n = fragment(pkt,501,0,1,8);
    T(af_mux_feed(&m,32,8,pkt,n,&slot)==AF_DONE);
    T(af_mux_take(&m,32,slot,out,sizeof(out))==31);
    aupe_gate_close(&g);
    af_mux_unbind(&m);
    T(aupe_gate_check(&g,32)==AF_CLOSED);
    T(aupe_gate_bind(&g,33)==AF_CLOSED);
}
struct threaded_gate {
    pthread_mutex_t lock;
    struct aupe_rx_gate gate;
    unsigned accepted, rejected, closed;
};
static void *thread_worker(void *opaque)
{
    struct threaded_gate *ctx = opaque;
    unsigned i;
    for (i=0; i < 12000; ++i) {
        af_u64 incoming = (af_u64)(i % 9U) + 1U;
        int r;
        pthread_mutex_lock(&ctx->lock);
        r = aupe_gate_check(&ctx->gate,incoming);
        if (ctx->gate.state == AUPE_GATE_CLOSED) {
            T(r == AF_CLOSED);
            ctx->closed++;
        } else if (ctx->gate.state == AUPE_GATE_ACTIVE &&
                   incoming == ctx->gate.epoch) {
            T(r == AF_WAIT);
            ctx->accepted++;
        } else {
            T(r == AF_STALE_EPOCH);
            ctx->rejected++;
        }
        pthread_mutex_unlock(&ctx->lock);
    }
    return NULL;
}
static void *thread_rotator(void *opaque)
{
    struct threaded_gate *ctx = opaque;
    unsigned i;
    for (i=0; i < 12000; ++i) {
        pthread_mutex_lock(&ctx->lock);
        if (i == 10000)
            aupe_gate_close(&ctx->gate);
        else if (i < 10000 && i % 17 == 0)
            aupe_gate_unbind(&ctx->gate);
        else if (i < 10000)
            T(aupe_gate_bind(&ctx->gate, (af_u64)i+2U)==AF_WAIT);
        else
            T(aupe_gate_bind(&ctx->gate,(af_u64)i+2U)==AF_CLOSED);
        pthread_mutex_unlock(&ctx->lock);
    }
    return NULL;
}
static void concurrent_gate(void)
{
    struct threaded_gate c = {0};
    pthread_t threads[5];
    unsigned i;
    T(pthread_mutex_init(&c.lock, NULL)==0);
    T(aupe_gate_bind(&c.gate,1)==AF_WAIT);
    for (i=0; i<4; ++i) {
        if (pthread_create(&threads[i],NULL,thread_worker,&c)) {
            fputs("pthread_create failed\n",stderr); exit(2);
        }
    }
    if (pthread_create(&threads[4],NULL,thread_rotator,&c)) {
        fputs("pthread_create rotator failed\n",stderr); exit(2);
    }
    for (i=0;i<5;i++) T(pthread_join(threads[i],NULL)==0);
    T(c.accepted+c.rejected+c.closed==48000);
    T(c.gate.state==AUPE_GATE_CLOSED);
    T(pthread_mutex_destroy(&c.lock)==0);
}
int main(void)
{
    simple_contract(); crypto_epoch_fence(); concurrent_gate();
    printf("AUPE lifecycle gate: %u checks, %u failures\n",checks,failed);
    return failed ? 1 : 0;
}
