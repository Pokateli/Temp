/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aupe_dispatch.h"

static unsigned long long passed, failed;
#define T(e) do { passed++; if (!(e)) { failed++; \
    fprintf(stderr,"dispatch:%d: %s\n",__LINE__,#e); } } while (0)
static struct af_state left_a, left_b, right_a, right_b;
static af_u8 scratch[AF_MAX_PACKET], packet[AF_MAX_PACKET];
static unsigned calls;
static unsigned captured_len;
static int deny_callback;
static af_u8 captured[AF_MAX_PACKET];

static int emit_capture(void *ctx, const af_u8 *p, af_u32 n)
{
    (void)ctx;
    calls++;
    captured_len = n;
    if (n > sizeof(captured)) return -1;
    memcpy(captured, p, n);
    return deny_callback ? -1 : 0;
}
static af_u16 csum(const af_u8 *p)
{
    unsigned sum=0,i;
    for(i=0;i<20;i+=2) sum+=(unsigned)p[i]*256U + p[i+1];
    while(sum>>16) sum=(sum&65535U)+(sum>>16);
    return (af_u16)(~sum);
}
static af_u32 fragment(af_u8 *b,unsigned id,unsigned offset,int more,unsigned len)
{
    unsigned i;
    af_u32 size = 20U+len;
    af_u16 flag=(af_u16)((offset/8U)|(more?0x2000U:0));
    memset(b,0,size);
    b[0]=0x45; b[1]=2;
    b[2]=(af_u8)(size>>8); b[3]=(af_u8)size;
    b[4]=(af_u8)(id>>8); b[5]=(af_u8)id;
    b[6]=(af_u8)(flag>>8); b[7]=(af_u8)flag;
    b[8]=64; b[9]=17;
    b[12]=172; b[13]=16; b[15]=2;
    b[16]=172; b[17]=16; b[19]=2;
    for(i=0;i<len;i++) b[20U+i]=(af_u8)((offset+i+id)%251U);
    { af_u16 chk=csum(b); b[10]=(af_u8)(chk>>8); b[11]=(af_u8)chk; }
    return size;
}
static void reset_calls(void) { calls=0; captured_len=0; deny_callback=0; }
static void test_two_peers(void)
{
    struct aupe_rx_gate ag={0},bg={0};
    struct af_mux am={0},bm={0};
    af_u32 n;
    reset_calls();
    T(aupe_gate_bind(&ag,700)==AF_WAIT);
    T(aupe_gate_bind(&bg,700)==AF_WAIT);
    T(af_mux_bind(&am,&left_a,&left_b,700)==AF_WAIT);
    T(af_mux_bind(&bm,&right_a,&right_b,700)==AF_WAIT);
    n=fragment(packet,17,0,1,8);
    T(aupe_dispatch_verified(&ag,&am,700,10,packet,n,scratch,sizeof(scratch),emit_capture,NULL)==AF_WAIT);
    T(aupe_dispatch_verified(&bg,&bm,700,10,packet,n,scratch,sizeof(scratch),emit_capture,NULL)==AF_WAIT);
    T(calls==0);
    T(left_a.active && right_a.active);
    T(aupe_gate_bind(&ag,700)==AF_ALREADY_BOUND);
    T(aupe_gate_bind(&ag,699)==AF_STALE_EPOCH);
    T(left_a.active && left_a.received_bytes==8);
    n=fragment(packet,17,8,0,6);
    T(aupe_dispatch_verified(&ag,&am,700,12,packet,n,scratch,sizeof(scratch),emit_capture,NULL)==AF_DONE);
    T(calls==1 && captured_len==34);
    T(captured[12]==172 && captured[15]==2);
    T(left_a.active==0 && right_a.active==1);
    T(aupe_dispatch_verified(&bg,&bm,700,12,packet,n,scratch,sizeof(scratch),emit_capture,NULL)==AF_DONE);
    T(calls==2 && captured_len==34);
    /* Wipe *only* reassembled plaintext output, not caller's packet. */
    for(n=0;n<34;n++) T(scratch[n]==0);
    T(packet[20]!=0);
    aupe_gate_close(&ag); af_mux_unbind(&am);
    T(aupe_dispatch_verified(&ag,&am,700,20,packet,26,scratch,sizeof(scratch),emit_capture,NULL)==AF_CLOSED);
    T(calls==2);
    n=fragment(packet,18,0,0,8);
    T(aupe_dispatch_verified(&bg,&bm,700,20,packet,n,scratch,sizeof(scratch),emit_capture,NULL)==AF_BYPASS);
    T(calls==3 && captured_len==n);
    aupe_gate_close(&bg); af_mux_unbind(&bm);
}
static void test_fencing_and_failures(void)
{
    struct aupe_rx_gate gate={0};
    struct af_mux mux={0};
    af_u32 n;
    reset_calls();
    T(aupe_gate_bind(&gate,1001)==AF_WAIT);
    T(af_mux_bind(&mux,&left_a,&left_b,1001)==AF_WAIT);
    n=fragment(packet,19,0,1,8);
    T(aupe_dispatch_verified(&gate,&mux,1001,100,packet,n,scratch,sizeof(scratch),emit_capture,NULL)==AF_WAIT);
    T(aupe_gate_bind(&gate,1001)==AF_ALREADY_BOUND);
    T(left_a.active && left_a.received_bytes==8);
    T(aupe_gate_bind(&gate,1002)==AF_WAIT);
    T(af_mux_bind(&mux,&left_a,&left_b,1002)==AF_WAIT);
    T(!left_a.active && left_a.payload[0]==0);
    n=fragment(packet,19,8,0,4);
    T(aupe_dispatch_verified(&gate,&mux,1001,101,packet,n,scratch,sizeof(scratch),emit_capture,NULL)==AF_STALE_EPOCH);
    T(calls==0);
    T(aupe_dispatch_verified(&gate,&mux,1002,101,packet,n,scratch,sizeof(scratch),emit_capture,NULL)==AF_WAIT);
    n=fragment(packet,19,0,1,8);
    T(aupe_dispatch_verified(&gate,&mux,1002,102,packet,n,scratch,sizeof(scratch),emit_capture,NULL)==AF_DONE);
    T(calls==1);
    n=fragment(packet,20,0,0,8);
    deny_callback=1;
    T(aupe_dispatch_verified(&gate,&mux,1002,103,packet,n,scratch,sizeof(scratch),emit_capture,NULL)==AF_BAD);
    T(calls==2);
    T(aupe_dispatch_verified(&gate,&mux,1002,103,packet,n,scratch,4,emit_capture,NULL)==AF_BAD);
    T(aupe_dispatch_verified(&gate,&mux,1002,103,packet,n,packet,sizeof(packet),emit_capture,NULL)==AF_BAD);
    T(aupe_dispatch_verified(&gate,&mux,1002,103,packet,n,packet+8,sizeof(packet),emit_capture,NULL)==AF_BAD);
    T(aupe_dispatch_verified(&gate,&mux,1002,103,packet,n,scratch,sizeof(scratch),NULL,NULL)==AF_BAD);
    T(calls==2);
    n=fragment(packet,21,0,1,8);
    T(aupe_dispatch_verified(&gate,&mux,1002,103,packet,n,scratch,sizeof(scratch),emit_capture,NULL)==AF_WAIT);
    n=fragment(packet,21,8,0,4);
    T(aupe_dispatch_verified(&gate,&mux,1002,104,packet,n,scratch,sizeof(scratch),emit_capture,NULL)==AF_BAD);
    T(calls==3);
    for(n=0;n<32;n++) T(scratch[n]==0);
    aupe_gate_unbind(&gate); af_mux_unbind(&mux);
    T(aupe_gate_bind(&gate,1002)==AF_STALE_EPOCH);
    T(aupe_gate_bind(&gate,999)==AF_STALE_EPOCH);
    T(aupe_gate_bind(&gate,1003)==AF_WAIT);
    T(af_mux_bind(&mux,&left_a,&left_b,1003)==AF_WAIT);
    T(aupe_dispatch_verified(&gate,&mux,1002,104,packet,n,scratch,sizeof(scratch),emit_capture,NULL)==AF_STALE_EPOCH);
    aupe_gate_close(&gate); af_mux_unbind(&mux);
}
static void test_churn(void)
{
    struct aupe_rx_gate gate={0};
    struct af_mux mux={0};
    unsigned i;
    af_u32 n;
    T(aupe_gate_bind(&gate,1)==AF_WAIT);
    T(af_mux_bind(&mux,&left_a,&left_b,1)==AF_WAIT);
    reset_calls();
    for(i=0;i<30000;i++) {
        if(i%5U==0) {
            T(aupe_gate_bind(&gate,(af_u64)(i/5U+2U))==AF_WAIT);
            T(af_mux_bind(&mux,&left_a,&left_b,gate.epoch)==AF_WAIT);
        }
        n=fragment(packet,i%65535U,0,0,16);
        T(aupe_dispatch_verified(&gate,&mux,gate.epoch,i,packet,n,scratch,sizeof(scratch),emit_capture,NULL)==AF_BYPASS);
        T(aupe_gate_bind(&gate,gate.epoch)==AF_ALREADY_BOUND);
        T(aupe_gate_bind(&gate,gate.epoch-1U)==AF_STALE_EPOCH);
    }
    T(calls==30000);
    aupe_gate_close(&gate); af_mux_unbind(&mux);
}
int main(void)
{
    test_two_peers();test_fencing_and_failures();test_churn();
    printf("AUPE dispatch bridge tests: %llu checks, %llu failures\n",passed,failed);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
