/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aupe_dispatch.h"
static unsigned long long checks, failures, deliveries;
#define T(cond) do { checks++; if (!(cond)) { failures++; \
    fprintf(stderr,"keypairs:%d: %s\n", __LINE__,#cond); }} while (0)
static af_u8 packet[64], scratch[AF_MAX_PACKET];
static struct af_state s1, s2;
static af_u16 cksum(const af_u8 *b) {
    af_u32 sum=0; unsigned i;
    for(i=0;i<20;i+=2)sum+=(unsigned)b[i]*256U+b[i+1];
    while(sum>>16)sum=(sum&0xffffU)+(sum>>16);
    return (af_u16)~sum;
}
static af_u32 build_packet(unsigned id, unsigned off, int more, unsigned payload){
    af_u16 frag=(af_u16)(off/8U | (more ? 0x2000U:0U));
    af_u16 cs;unsigned i;
    memset(packet,0,sizeof(packet));
    packet[0]=0x45;packet[2]=0;packet[3]=(af_u8)(20U+payload);
    packet[4]=(af_u8)(id>>8);packet[5]=(af_u8)id;
    packet[6]=(af_u8)(frag>>8);packet[7]=(af_u8)frag;
    packet[8]=64;packet[9]=17;packet[12]=172;packet[13]=16;packet[15]=2;
    packet[16]=172;packet[17]=16;packet[19]=2;
    for(i=0;i<payload;i++)packet[20+i]=(af_u8)(i+off+id);
    cs=cksum(packet);packet[10]=(af_u8)(cs>>8);packet[11]=(af_u8)cs;
    return 20U+payload;
}
static int emitter(void *ctx,const af_u8 *data,af_u32 n){
    (void)ctx;
    if (!data || n<28 || data[0]!=0x45) return -1;
    deliveries++;return 0;
}
static void boundary(void){
    struct aupe_keypair_bank b={0},other={0};
    struct aupe_rx_gate gate={0};
    struct af_mux mux={0};
    af_u32 n=build_packet(77,0,0,8);
    T(aupe_keypairs_accept(&b,1,0,100)==AF_STALE_EPOCH);
    T(aupe_keypairs_install(&b,0,1)==AF_BAD);
    T(aupe_keypairs_install(&b,1,100)==AF_WAIT);
    T(aupe_keypairs_install(&b,1,100)==AF_ALREADY_BOUND);
    T(aupe_keypairs_install(&b,1,99)==AF_BAD);
    T(aupe_keypairs_install(&b,0,100)==AF_BAD);
    T(aupe_keypairs_accept(&b,1,0,100)==AF_WAIT);
    T(aupe_keypairs_accept(&b,1,0,100)==AUPE_REPLAY_DUPLICATE);
    T(aupe_keypairs_install(&b,2,200)==AF_WAIT);
    T(aupe_keypairs_accept(&b,1,1,201)==AF_WAIT); /* previous lives */
    T(aupe_keypairs_accept(&b,2,0,201)==AF_WAIT); /* independent counter */
    T(aupe_keypairs_accept(&b,2,0,201)==AUPE_REPLAY_DUPLICATE);
    T(aupe_keypairs_accept(&b,1,1,201)==AUPE_REPLAY_DUPLICATE);
    T(aupe_keypairs_accept(&b,1,2,200)==AF_BAD); /* no time rollback */
    T(aupe_keypairs_install(&b,2,201)==AF_ALREADY_BOUND);
    T(aupe_keypairs_accept(&b,1,3,180199)==AF_WAIT);
    T(aupe_keypairs_accept(&b,1,4,180200)==AF_STALE_EPOCH);
    T(aupe_keypairs_accept(&b,2,1,180200)==AF_WAIT);
    T(aupe_keypairs_install(&b,3,180300)==AF_WAIT);
    T(aupe_keypairs_accept(&b,1,5,180301)==AF_STALE_EPOCH);
    T(aupe_keypairs_retire_previous(&b,2)==AF_WAIT);
    T(aupe_keypairs_accept(&b,2,2,180302)==AF_STALE_EPOCH);
    T(aupe_keypairs_accept(&b,3,0,180302)==AF_WAIT);
    T(aupe_keypairs_install(&b,2,180302)==AF_STALE_EPOCH);
    T(aupe_keypairs_install(&b,4,~(af_u64)0)==AF_BAD);
    T(aupe_keypairs_install(&other,10,180302)==AF_WAIT);
    T(aupe_keypairs_accept(&other,10,0,180302)==AF_WAIT);
    T(aupe_keypairs_accept(&other,10,0,180302)==AUPE_REPLAY_DUPLICATE);
    T(aupe_keypairs_accept(&b,3,1,180302)==AF_WAIT);
    T(aupe_gate_bind(&gate,100)==AF_WAIT);
    T(af_mux_bind(&mux,&s1,&s2,100)==AF_WAIT);
    aupe_keypairs_clear(&b);
    T(aupe_keypairs_install(&b,100,250)==AF_WAIT);
    T(aupe_dispatch_keypair(&gate,&mux,&b,100,0,250,packet,n,scratch,sizeof(scratch),emitter,NULL)==AF_BYPASS);
    T(aupe_dispatch_keypair(&gate,&mux,&b,100,0,251,packet,n,scratch,sizeof(scratch),emitter,NULL)==AUPE_REPLAY_DUPLICATE);
    T(aupe_gate_bind(&gate,101)==AF_WAIT);
    T(af_mux_bind(&mux,&s1,&s2,101)==AF_WAIT);
    T(aupe_keypairs_install(&b,101,300)==AF_WAIT);
    T(aupe_dispatch_keypair(&gate,&mux,&b,100,1,301,packet,n,scratch,sizeof(scratch),emitter,NULL)==AF_BYPASS);
    T(aupe_dispatch_keypair(&gate,&mux,&b,101,0,302,packet,n,scratch,sizeof(scratch),emitter,NULL)==AF_BYPASS);
    T(aupe_dispatch_keypair(&gate,&mux,&b,99,0,303,packet,n,scratch,sizeof(scratch),emitter,NULL)==AF_STALE_EPOCH);
    T(deliveries==3);
    T(aupe_dispatch_keypair(&gate,&mux,&b,101,1,304,packet,n,packet,sizeof(packet),emitter,NULL)==AF_BAD);
    T(aupe_dispatch_keypair(&gate,&mux,&b,101,1,304,packet,n,scratch,sizeof(scratch),emitter,NULL)==AF_BYPASS);
    T(aupe_keypairs_retire_previous(&b,100)==AF_WAIT);
    T(aupe_dispatch_keypair(&gate,&mux,&b,100,2,305,packet,n,scratch,sizeof(scratch),emitter,NULL)==AF_STALE_EPOCH);
    aupe_gate_close(&gate);
    T(aupe_dispatch_keypair(&gate,&mux,&b,101,2,306,packet,n,scratch,sizeof(scratch),emitter,NULL)==AF_CLOSED);
    af_mux_unbind(&mux);aupe_keypairs_clear(&b);
    T(b.active.valid==0 && b.previous.valid==0 && b.high_watermark==0);
}
static af_u64 rng=0x987654321abULL;
static af_u32 next(void){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return (af_u32)rng;}
static void stress(void){
    struct aupe_keypair_bank b={0};
    unsigned k,j;
    af_u64 now=1000;
    for(k=1;k<=24;k++){
        T(aupe_keypairs_install(&b,k,now)==AF_WAIT);
        T(aupe_keypairs_install(&b,k,now)==AF_ALREADY_BOUND);
        for(j=0;j<12000;j++){
            af_u64 e=(next()%3U==0 && k>1)?(k-1U):(af_u64)k;
            af_u64 c=(af_u64)(next()%4000U);
            int result=aupe_keypairs_accept(&b,e,c,now);
            T(result==AF_WAIT || result==AUPE_REPLAY_DUPLICATE);
            if(result==AF_WAIT)T(aupe_keypairs_accept(&b,e,c,now)==AUPE_REPLAY_DUPLICATE);
            T(aupe_keypairs_accept(&b,0,c,now)==AF_STALE_EPOCH);
            if((j%1000U)==0)now+=1;
        }
        now+=200;
        if(k>2)T(aupe_keypairs_accept(&b,k-2,0,now)==AF_STALE_EPOCH);
    }
    aupe_keypairs_clear(&b);
}
int main(void){boundary();stress();printf("AUPE dual-keypair tests: %llu checks, %llu failures\n",checks,failures);return failures?1:0;}
