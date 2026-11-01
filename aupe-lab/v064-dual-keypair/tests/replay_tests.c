/* SPDX-License-Identifier: GPL-2.0-only */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "aupe_replay.h"
#include "aupe_dispatch.h"

static unsigned long long tests, errors;
#define C(e) do { ++tests; if (!(e)) { ++errors; if (errors < 8) \
    fprintf(stderr, "replay:%d: %s\n", __LINE__, #e); } } while (0)
static unsigned rnd_state=0x51f3a892U;
static unsigned rnd(void)
{
    unsigned x=rnd_state;
    x^=x<<13; x^=x>>17; x^=x<<5;
    return rnd_state=x;
}
/* An independent reference oracle: each ring entry remembers the *actual*
 * accepted counter, not a bitmap. This is intentionally different from the
 * bit clearing implementation used by the production candidate. */
static af_u64 ref_seen[AUPE_REPLAY_WINDOW];
static af_u64 ref_hi;
static int ref_has;
static void ref_reset(void) { memset(ref_seen,0xff,sizeof(ref_seen)); ref_has=0; ref_hi=0; }
static int ref_accept(af_u64 n)
{
    unsigned slot=(unsigned)(n % AUPE_REPLAY_WINDOW);
    if (n >= AUPE_REPLAY_MAX_COUNTER) return AUPE_REPLAY_LIMIT;
    if (!ref_has) { ref_has=1; ref_hi=n; ref_seen[slot]=n; return AF_WAIT; }
    if (n <= ref_hi && (ref_hi-n >= AUPE_REPLAY_WINDOW || ref_seen[slot]==n))
        return AUPE_REPLAY_DUPLICATE;
    if (n > ref_hi) ref_hi=n;
    ref_seen[slot]=n;
    return AF_WAIT;
}
static void replay_edges(void)
{
    struct aupe_replay_window a={0},b={0};
    unsigned i;
    C(aupe_replay_accept(&a,5,0)==AF_STALE_EPOCH);
    aupe_replay_bind(&a,5); aupe_replay_bind(&b,5);
    C(aupe_replay_accept(&a,5,0)==AF_WAIT);
    C(aupe_replay_accept(&a,5,0)==AUPE_REPLAY_DUPLICATE);
    C(aupe_replay_accept(&b,5,0)==AF_WAIT); /* independent peers */
    C(aupe_replay_accept(&a,6,1)==AF_STALE_EPOCH);
    C(aupe_replay_accept(&a,5,8191)==AF_WAIT);
    C(aupe_replay_accept(&a,5,8192)==AF_WAIT);
    C(aupe_replay_accept(&a,5,0)==AUPE_REPLAY_DUPLICATE);
    C(aupe_replay_accept(&a,5,1)==AF_WAIT);
    C(aupe_replay_accept(&a,5,1)==AUPE_REPLAY_DUPLICATE);
    C(aupe_replay_accept(&a,5,8192+AUPE_REPLAY_WINDOW)==AF_WAIT);
    C(aupe_replay_accept(&a,5,8192)==AUPE_REPLAY_DUPLICATE);
    C(aupe_replay_accept(&a,5,AUPE_REPLAY_MAX_COUNTER)==AUPE_REPLAY_LIMIT);
    aupe_replay_bind(&a,6);
    C(aupe_replay_accept(&a,5,2)==AF_STALE_EPOCH);
    C(aupe_replay_accept(&a,6,0)==AF_WAIT);
    C(aupe_replay_accept(&a,6,0)==AUPE_REPLAY_DUPLICATE);
    aupe_replay_clear(&a);
    C(aupe_replay_accept(&a,6,1)==AF_STALE_EPOCH);
    for(i=0;i<sizeof(a);i++) C(((const af_u8 *)&a)[i]==0);
}
static void replay_random(void)
{
    static struct aupe_replay_window w;
    af_u64 base=0;
    unsigned i;
    aupe_replay_bind(&w,199);
    ref_reset();
    for (i=0;i<180000;i++) {
        af_u64 n;
        unsigned t=rnd()%8U;
        if (t==0) base+=rnd()%20000U;
        else if (t==1) base+=rnd()%40U;
        if (t==2) n=0;
        else if(t==3) n=base + rnd()%2U;
        else if(t==4) n=base>20000U?base-rnd()%20000U:0;
        else n=base>12000U?base-rnd()%12000U:0;
        C(aupe_replay_accept(&w,199,n)==ref_accept(n));
        if (i%5000U==0) {
            aupe_replay_bind(&w,200U+i);
            ref_reset();
            C(aupe_replay_accept(&w,199,base)==AF_STALE_EPOCH);
            /* Test oracle starts a completely new keypair epoch. */
            aupe_replay_bind(&w,199);
            ref_reset();
        }
    }
}
static af_u16 checksum(const af_u8 *p)
{
    unsigned i,sum=0;
    for(i=0;i<20;i+=2) sum+=(unsigned)p[i]*256U+p[i+1];
    while(sum>>16) sum=(sum&65535U)+(sum>>16);
    return (af_u16)~sum;
}
static af_u32 ipv4(af_u8 *b, unsigned id, unsigned off, int more, unsigned len)
{
    unsigned i; af_u16 f=(af_u16)((off/8U)|(more?0x2000U:0));
    af_u32 n=20U+len;
    memset(b,0,n); b[0]=0x45; b[2]=(af_u8)(n>>8); b[3]=(af_u8)n;
    b[4]=(af_u8)(id>>8); b[5]=(af_u8)id;
    b[6]=(af_u8)(f>>8); b[7]=(af_u8)f;
    b[8]=64;b[9]=17;b[12]=172;b[13]=16;b[15]=2;
    b[16]=172;b[17]=16;b[19]=2;
    for(i=0;i<len;i++) b[20U+i]=(af_u8)((id+off+i)%251U);
    {af_u16 s=checksum(b);b[10]=(af_u8)(s>>8);b[11]=(af_u8)s;}
    return n;
}
static unsigned delivered;
static int fail_emit;
static int capture(void *ctx,const af_u8 *p,af_u32 n)
{
    (void)ctx;
    if(n<28 || (p[0]&0xf0)!=0x40) return -1;
    delivered++;
    return fail_emit?-1:0;
}
static void test_bound_bridge(void)
{
    static struct af_state first,second;
    static af_u8 pkt[AF_MAX_PACKET],scratch[AF_MAX_PACKET];
    struct aupe_rx_gate gate={0};
    struct af_mux mux={0};
    struct aupe_replay_window win={0};
    af_u32 n;
    C(aupe_gate_bind(&gate,100)==AF_WAIT);
    C(af_mux_bind(&mux,&first,&second,100)==AF_WAIT);
    aupe_replay_bind(&win,100);
    n=ipv4(pkt,11,0,0,8);
    C(aupe_dispatch_counter(&gate,&mux,&win,100,0,1,pkt,n,scratch,sizeof(scratch),capture,NULL)==AF_BYPASS);
    C(delivered==1);
    C(aupe_dispatch_counter(&gate,&mux,&win,100,0,2,pkt,n,scratch,sizeof(scratch),capture,NULL)==AUPE_REPLAY_DUPLICATE);
    C(delivered==1);
    C(aupe_dispatch_counter(&gate,&mux,&win,100,1,2,pkt,n,scratch,4,capture,NULL)==AF_BAD);
    C(aupe_dispatch_counter(&gate,&mux,&win,100,1,2,pkt,n,scratch,sizeof(scratch),capture,NULL)==AF_BYPASS);
    C(delivered==2);
    fail_emit=1;
    C(aupe_dispatch_counter(&gate,&mux,&win,100,2,3,pkt,n,scratch,sizeof(scratch),capture,NULL)==AF_BAD);
    fail_emit=0;
    C(aupe_dispatch_counter(&gate,&mux,&win,100,2,4,pkt,n,scratch,sizeof(scratch),capture,NULL)==AUPE_REPLAY_DUPLICATE);
    C(delivered==3);
    n=ipv4(pkt,12,0,1,8);
    C(aupe_dispatch_counter(&gate,&mux,&win,100,3,5,pkt,n,scratch,sizeof(scratch),capture,NULL)==AF_WAIT);
    C(delivered==3);
    n=ipv4(pkt,12,8,0,8);
    C(aupe_dispatch_counter(&gate,&mux,&win,100,3,6,pkt,n,scratch,sizeof(scratch),capture,NULL)==AUPE_REPLAY_DUPLICATE);
    C(aupe_dispatch_counter(&gate,&mux,&win,100,4,7,pkt,n,scratch,sizeof(scratch),capture,NULL)==AF_DONE);
    C(delivered==4);
    C(aupe_dispatch_counter(&gate,&mux,&win,99,5,8,pkt,n,scratch,sizeof(scratch),capture,NULL)==AF_STALE_EPOCH);
    C(aupe_gate_bind(&gate,101)==AF_WAIT);
    C(af_mux_bind(&mux,&first,&second,101)==AF_WAIT);
    aupe_replay_bind(&win,101);
    n=ipv4(pkt,13,0,0,8);
    C(aupe_dispatch_counter(&gate,&mux,&win,101,0,8,pkt,n,scratch,sizeof(scratch),capture,NULL)==AF_BYPASS);
    C(delivered==5);
    C(aupe_dispatch_counter(&gate,&mux,&win,100,7,9,pkt,n,scratch,sizeof(scratch),capture,NULL)==AF_STALE_EPOCH);
    C(delivered==5);
    aupe_gate_close(&gate);af_mux_unbind(&mux);aupe_replay_clear(&win);
    C(aupe_dispatch_counter(&gate,&mux,&win,101,1,10,pkt,n,scratch,sizeof(scratch),capture,NULL)==AF_CLOSED);
}
int main(void)
{
    replay_edges();replay_random();test_bound_bridge();
    printf("AUPE transport-counter replay tests: %llu checks, %llu failures\n",tests,errors);
    return errors?EXIT_FAILURE:EXIT_SUCCESS;
}