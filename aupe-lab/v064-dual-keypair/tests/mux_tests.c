/* SPDX-License-Identifier: GPL-2.0-only */
#include "aupe_mux.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static unsigned checks, failed;
#define T(expr) do { checks++; if (!(expr)) { \
    fprintf(stderr,"mux:%d: failed: %s\n",__LINE__,#expr); failed++; \
} } while (0)
static struct af_state slots[2], alien_slots[2];
static struct af_mux m, other;
static af_u8 packet[65535], out[65535];
static void put16(af_u8 *p, unsigned v) {p[0]=(af_u8)(v>>8);p[1]=(af_u8)v;}
static unsigned checksum(const af_u8 *p)
{
    unsigned i, sum=0;
    for (i=0;i<20;i+=2) sum += ((unsigned)p[i]<<8)|p[i+1];
    while(sum>>16) sum=(sum&65535U)+(sum>>16);
    return (~sum)&65535U;
}
static void refresh(af_u8 *p) {put16(p+10,0);put16(p+10,checksum(p));}
static unsigned make(af_u8 *p, unsigned id, unsigned off, unsigned len,
                     int mf, unsigned src, unsigned dst)
{
    unsigned i;
    memset(p,0,len+20);
    p[0]=0x45; p[1]=2; put16(p+2,len+20); put16(p+4,id);
    put16(p+6,((off/8)&8191U)|(mf?8192U:0U));
    p[8]=64; p[9]=17;
    p[12]=172; p[13]=16; p[14]=(af_u8)(src>>8);p[15]=(af_u8)src;
    p[16]=172; p[17]=16; p[18]=(af_u8)(dst>>8);p[19]=(af_u8)dst;
    for(i=0;i<len;i++) p[20+i]=(af_u8)((off+i+id)%251U);
    refresh(p);
    return len+20;
}
static int feed(struct af_mux *mux,unsigned id,unsigned off,unsigned len,
                int mf,af_u64 epoch,af_u64 now,unsigned *slot)
{
    unsigned n=make(packet,id,off,len,mf,3,7);
    return af_mux_feed(mux,epoch,now,packet,n,slot);
}
static void setup(void)
{
    memset(slots,0,sizeof(slots));
    memset(alien_slots,0,sizeof(alien_slots));
    memset(&m,0,sizeof(m));
    memset(&other,0,sizeof(other));
    T(af_mux_bind(&m,&slots[0],&slots[1],101)==AF_WAIT);
    T(af_mux_bind(&other,&alien_slots[0],&alien_slots[1],101)==AF_WAIT);
}
static void interleaved(void)
{
    unsigned a,b,idx;
    setup();
    T(feed(&m,1,0,16,1,101,0,&idx)==AF_WAIT);
    T(feed(&m,2,0,8,1,101,1,&idx)==AF_WAIT);
    T(feed(&m,3,0,8,1,101,2,&idx)==AF_MUX_BUSY);
    T(slots[0].active && slots[1].active);
    T(feed(&m,1,16,5,0,101,3,&a)==AF_DONE);
    T(a==0);
    T(feed(&m,2,8,4,0,101,4,&b)==AF_DONE);
    T(b==1);
    T(feed(&m,3,0,8,1,101,5,&idx)==AF_MUX_BUSY);
    T(af_mux_take(&m,101,b,out,sizeof(out))==32);
    T(checksum(out)==0);
    T(out[20]==(af_u8)(2%251U));
    T(af_mux_take(&m,101,a,out,sizeof(out))==41);
    T(out[20]==(af_u8)(1%251U));
    T(feed(&m,3,0,8,1,101,10,&idx)==AF_WAIT);
    T(feed(&m,3,8,3,0,101,11,&a)==AF_DONE);
    T(af_mux_take(&m,101,a,out,sizeof(out))==31);
    af_mux_unbind(&m);
    af_mux_unbind(&other);
}
static void unfrag_not_blocked(void)
{
    unsigned idx=99,n;
    setup();
    T(feed(&m,21,0,8,1,101,10,&idx)==AF_WAIT);
    T(feed(&m,22,0,8,1,101,11,&idx)==AF_WAIT);
    n=make(packet,42,0,17,0,3,7);
    T(af_mux_feed(&m,101,12,packet,n,&idx)==AF_BYPASS);
    T(idx==AF_MUX_SLOTS);
    T(slots[0].active && slots[1].active);
    T(feed(&m,21,8,3,0,101,13,&idx)==AF_DONE);
    T(af_mux_take(&m,101,idx,out,sizeof(out))==31);
    af_mux_unbind(&m);af_mux_unbind(&other);
}
static void isolation(void)
{
    unsigned i, j;
    setup();
    T(feed(&m,42,0,8,1,101,0,&i)==AF_WAIT);
    T(feed(&other,42,0,8,1,101,0,&j)==AF_WAIT);
    T(feed(&m,42,8,7,0,101,1,&i)==AF_DONE);
    T(other.slot[0]->active && !other.slot[0]->complete);
    T(af_mux_take(&m,101,i,out,sizeof(out))==35);
    T(feed(&other,42,8,7,0,101,2,&j)==AF_DONE);
    T(af_mux_take(&other,101,j,out,sizeof(out))==35);
    af_mux_unbind(&m);af_mux_unbind(&other);
}
static void epochs_and_timeout(void)
{
    unsigned idx=99;
    setup();
    T(feed(&m,1,0,8,1,101,100,&idx)==AF_WAIT);
    T(feed(&m,2,0,8,1,101,110,&idx)==AF_WAIT);
    T(af_mux_expire(&m,101,5099)==0);
    T(af_mux_expire(&m,101,5100)==1);
    T(slots[0].active==0 && slots[1].active==1);
    T(feed(&m,3,0,8,1,101,5101,&idx)==AF_WAIT);
    T(af_mux_expire(&m,101,5110)==1);
    T(af_mux_expire(&m,101,10101)==1);
    T(af_mux_expire(&m,101,11000)==0);
    T(af_mux_expire(&m,100,11000)==AF_STALE_EPOCH);
    T(feed(&m,4,0,8,1,101,11001,&idx)==AF_WAIT);
    T(af_mux_bind(&m,&slots[0],&slots[1],102)==AF_WAIT);
    T(!slots[0].active && !slots[1].active);
    T(slots[0].payload[2]==0U && slots[1].payload[2]==0U);
    T(feed(&m,4,8,4,0,101,11002,&idx)==AF_STALE_EPOCH);
    T(feed(&m,4,0,8,1,102,11002,&idx)==AF_WAIT);
    T(af_mux_bind(&m,&slots[0],&alien_slots[0],103)==AF_BAD);
    T(slots[0].active);
    af_mux_unbind(&m);
    T(!slots[0].bound && !slots[1].bound);
    T(feed(&m,4,0,8,1,102,11003,&idx)==AF_STALE_EPOCH);
    af_mux_unbind(&other);
}
static void malformed_and_overlaps(void)
{
    unsigned idx,n;
    setup();
    T(feed(&m,1,0,16,1,101,100,&idx)==AF_WAIT);
    T(feed(&m,2,0,16,1,101,101,&idx)==AF_WAIT);
    n=make(packet,2,0,8,1,3,7);
    packet[10]^=12;
    T(af_mux_feed(&m,101,102,packet,n,&idx)==AF_BAD);
    T(slots[0].active && slots[1].active);
    T(feed(&m,1,8,8,1,101,103,&idx)==AF_OVERLAP);
    T(!slots[0].active && slots[1].active);
    T(feed(&m,2,16,3,0,101,104,&idx)==AF_DONE);
    T(af_mux_take(&m,101,idx,out,sizeof(out))==39);
    T(af_mux_take(&m,101,AF_MUX_SLOTS,out,sizeof(out))==AF_BAD);
    T(af_mux_take(&m,101,0,out,sizeof(out))==AF_NOT_READY);
    af_mux_unbind(&m);af_mux_unbind(&other);
}
static void stress(void)
{
    unsigned i,idx,k;
    setup();
    for (i=0;i<5000;i++) {
        unsigned id=100+(i%500);
        T(feed(&m,id,0,8,1,101,i*4U,&idx)==AF_WAIT);
        T(feed(&m,id+500,0,16,1,101,i*4U+1U,&idx)==AF_WAIT);
        T(feed(&m,id,8,9,0,101,i*4U+2U,&idx)==AF_DONE);
        T(af_mux_take(&m,101,idx,out,sizeof(out))==37);
        T(feed(&m,id+500,16,1,0,101,i*4U+3U,&idx)==AF_DONE);
        T(af_mux_take(&m,101,idx,out,sizeof(out))==37);
        T(!slots[0].active&&!slots[1].active);
        for (k=0;k<9;k++) T(out[20+k]==(af_u8)((k+id+500)%251U));
    }
    af_mux_unbind(&m);af_mux_unbind(&other);
}

/* Corrupt headers must not claim a slot or tear down other secure flows. */
static void corrupt_flood(void)
{
    unsigned idx, i, n;
    setup();
    T(feed(&m,700,0,8,1,101,100,&idx)==AF_WAIT);
    T(feed(&m,701,0,8,1,101,101,&idx)==AF_WAIT);
    for (i = 0; i < 20000; i++) {
        n = make(packet, 700 + (i % 2U), 8, 4, 0, 3, 7);
        switch (i % 5U) {
        case 0: packet[10] ^= 1U; break;   /* header checksum */
        case 1: packet[0] = 0x46U; refresh(packet); break;
        case 2: packet[6] |= 0x80U; refresh(packet); break;
        case 3: put16(packet+2,n+1U); refresh(packet); break;
        default: put16(packet+6,0x4001U); refresh(packet); break;
        }
        T(af_mux_feed(&m,101,200+i,packet,n,&idx)==AF_BAD);
        /* The fixture's epoch-bound slots must remain isolated.  Renew
         * the timebase to avoid expiry during the malformed packet flood. */
        if (i % 400U == 399U) {
            T(slots[0].bound && slots[1].bound);
        }
    }
    /* The malformed input must not corrupt the epoch, even if a datagram
     * has been independently expired by the trusted monotonic clock. */
    T(m.bound && m.epoch==101);
    af_mux_unbind(&m);af_mux_unbind(&other);
}
int main(void)
{
    interleaved();unfrag_not_blocked();isolation();
    epochs_and_timeout();malformed_and_overlaps();stress();corrupt_flood();
    printf("AUPE mux tests: %u checks, %u failures\n",checks,failed);
    return failed ? 1 : 0;
}