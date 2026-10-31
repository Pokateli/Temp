#include "aupe_frag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct af_state s; /* Large state must NOT reside on stack. */
static af_u8 packet[65535];
static af_u8 output[65535];
static unsigned count;
static unsigned failures;
#define CHECK(c) do { count++; if (!(c)) { \
    fprintf(stderr, "%s:%d: CHECK failed: %s\n",__FILE__,__LINE__,#c); \
    failures++; } } while (0)
static void put16(af_u8 *p, unsigned v) { p[0] = (af_u8)(v >> 8); p[1] = (af_u8)v; }
static unsigned get16(const af_u8 *p) { return ((unsigned)p[0] << 8) | p[1]; }
static unsigned checksum(const af_u8 *p, unsigned n)
{
    unsigned i, sum = 0;
    for (i = 0; i < n; i += 2)
        sum += ((unsigned)p[i] << 8) | p[i + 1];
    while (sum >> 16)
        sum = (sum & 0xffffU) + (sum >> 16);
    return (~sum) & 0xffffU;
}
static void refresh_sum(af_u8 *p)
{
    put16(p + 10, 0);
    put16(p + 10, checksum(p, 20));
}
static unsigned frag(af_u8 *p, unsigned id, unsigned off, unsigned len,
                     int more, unsigned src, unsigned dst)
{
    unsigned i, flags = ((off / 8) & 0x1fffU) | (more ? 0x2000U : 0U);
    memset(p, 0, len + 20);
    p[0] = 0x45; p[1] = 0; put16(p + 2, len + 20);
    put16(p + 4, id); put16(p + 6, flags); p[8] = 64; p[9] = 17;
    p[12] = 172; p[13] = 16; p[14] = (af_u8)(src >> 8); p[15] = (af_u8)src;
    p[16] = 172; p[17] = 16; p[18] = (af_u8)(dst >> 8); p[19] = (af_u8)dst;
    for (i = 0; i < len; i++)
        p[20 + i] = (af_u8)((off + i) % 251);
    refresh_sum(p);
    return len + 20;
}
static int feed(unsigned id, unsigned off, unsigned len, int more,
                unsigned src, unsigned dst, af_u64 epoch, af_u64 now)
{
    unsigned n = frag(packet, id, off, len, more, src, dst);
    return af_feed_ipv4(&s, epoch, now, packet, n);
}
static void begin(void) { memset(&s, 0, sizeof(s)); af_bind_epoch(&s, 99); }

static void basic(void)
{
    unsigned i;
    begin();
    CHECK(feed(12,0,16,1,2,3,99,100)==AF_WAIT);
    CHECK(feed(12,16,7,0,2,3,99,110)==AF_DONE);
    CHECK(af_take_ipv4(&s,output,sizeof(output))==43);
    CHECK(get16(output+2)==43);
    CHECK(get16(output+6)==0);
    CHECK(checksum(output,20)==0);
    for(i=0;i<23;i++) CHECK(output[20+i] == (af_u8)(i%251));
    CHECK(!s.active);
    /* Complete packet with same identification may start again. */
    CHECK(feed(12,0,8,1,2,3,99,120)==AF_WAIT);
    CHECK(feed(12,8,2,0,2,3,99,121)==AF_DONE);
    CHECK(af_take_ipv4(&s,output,sizeof(output))==30);
}
static void reverse(void)
{
    begin();
    CHECK(feed(41,16,9,0,2,3,99,100)==AF_WAIT);
    CHECK(feed(41,0,16,1,2,3,99,110)==AF_DONE);
    CHECK(af_take_ipv4(&s,output,sizeof(output))==45);
}
static void overlap_and_duplicate(void)
{
    begin();
    CHECK(feed(3,0,16,1,1,1,99,0)==AF_WAIT);
    CHECK(feed(3,8,8,1,1,1,99,1)==AF_OVERLAP);
    CHECK(!s.active);
    CHECK(feed(3,0,16,1,1,1,99,2)==AF_WAIT);
    CHECK(feed(3,0,16,1,1,1,99,3)==AF_OVERLAP);
    CHECK(!s.active);
}
static void epoch_and_id(void)
{
    begin();
    CHECK(feed(8,0,8,1,2,2,99,0)==AF_WAIT);
    CHECK(feed(8,8,4,0,2,2,98,1)==AF_STALE_EPOCH);
    CHECK(s.active);
    CHECK(feed(9,8,4,0,2,2,99,1)==AF_OTHER_DATAGRAM);
    CHECK(feed(8,8,4,0,3,2,99,1)==AF_OTHER_DATAGRAM);
    CHECK(feed(8,8,4,0,2,4,99,1)==AF_OTHER_DATAGRAM);
    CHECK(feed(8,8,4,0,2,2,99,1)==AF_DONE);
    CHECK(af_take_ipv4(&s,output,sizeof(output))==32);
    af_bind_epoch(&s,100);
    CHECK(feed(8,0,8,1,2,2,99,2)==AF_STALE_EPOCH);
    CHECK(feed(8,0,8,1,2,2,100,2)==AF_WAIT);
    af_unbind(&s);
    CHECK(feed(8,8,4,0,2,2,100,3)==AF_STALE_EPOCH);
    af_bind_epoch(&s,101);
    CHECK(feed(8,0,8,1,2,2,101,4)==AF_WAIT);
}
static void timeouts(void)
{
    begin();
    CHECK(feed(5,0,8,1,2,2,99,0)==AF_WAIT);
    CHECK(feed(5,8,8,1,2,2,99,4999)==AF_WAIT);
    CHECK(feed(5,16,1,0,2,2,99,5000)==AF_EXPIRED);
    CHECK(!s.active);
    CHECK(feed(5,0,8,1,2,2,99,6000)==AF_WAIT);
    CHECK(feed(5,8,1,0,2,2,99,6001)==AF_DONE);
    CHECK(af_take_ipv4(&s,output,sizeof(output))==29);
}
static void invalid(void)
{
    unsigned n;
    begin();
    n=frag(packet,1,0,9,1,2,2);
    CHECK(af_feed_ipv4(&s,99,1,packet,n)==AF_BAD); /* non-final not 8 aligned */
    n=frag(packet,1,0,8,1,2,2);
    packet[10]^=1;
    CHECK(af_feed_ipv4(&s,99,1,packet,n)==AF_BAD); /* checksum */
    n=frag(packet,1,0,8,1,2,2); packet[0]=0x46; refresh_sum(packet);
    CHECK(af_feed_ipv4(&s,99,1,packet,n)==AF_BAD); /* IPv4 options */
    n=frag(packet,1,0,8,1,2,2); put16(packet+6,0xa000); refresh_sum(packet);
    CHECK(af_feed_ipv4(&s,99,1,packet,n)==AF_BAD); /* reserved flag */
    n=frag(packet,1,0,8,1,2,2); put16(packet+6,0x6000); refresh_sum(packet);
    CHECK(af_feed_ipv4(&s,99,1,packet,n)==AF_BAD); /* DF and MF */
    n=frag(packet,1,0,8,1,2,2);
    CHECK(af_feed_ipv4(&s,99,1,packet,n-1)==AF_BAD); /* truncated */
    CHECK(af_feed_ipv4(&s,99,1,packet,1)==AF_BAD);
    CHECK(af_feed_ipv4(&s,99,1,NULL,n)==AF_BAD);
    CHECK(af_take_ipv4(&s,output,sizeof(output))==AF_NOT_READY);
}
static void final_bounds(void)
{
    begin();
    CHECK(feed(1,32,8,1,1,1,99,0)==AF_WAIT);
    CHECK(feed(1,16,4,0,1,1,99,1)==AF_BAD); /* last end < prior high water */
    CHECK(!s.active);
    CHECK(feed(1,16,3,0,1,1,99,2)==AF_WAIT);
    CHECK(feed(1,24,8,1,1,1,99,3)==AF_BAD); /* beyond last */
    CHECK(!s.active);
    CHECK(feed(1,16,3,0,1,1,99,4)==AF_WAIT);
    CHECK(feed(1,24,1,0,1,1,99,5)==AF_BAD); /* conflicting last */
    CHECK(!s.active);
}
static void complete_not_ready(void)
{
    begin();
    CHECK(feed(1,0,8,1,2,2,99,0)==AF_WAIT);
    CHECK(feed(1,8,3,0,2,2,99,1)==AF_DONE);
    CHECK(feed(2,0,8,1,2,2,99,2)==AF_NOT_READY);
    CHECK(af_take_ipv4(&s,output,30)==AF_BAD);
    CHECK(s.complete);
    CHECK(af_take_ipv4(&s,output,sizeof(output))==31);
}
static void large(void)
{
    unsigned i, offset=0, n;
    begin();
    /* Maximum IPv4 packet length (65535) / minimum header (20). */
    while (offset + 1480 < AF_MAX_PAYLOAD) {
        CHECK(feed(777,offset,1480,1,7,7,99,(offset/1480U)*100U)==AF_WAIT);
        offset+=1480;
    }
    CHECK(feed(777,offset,AF_MAX_PAYLOAD-offset,0,7,7,99,(offset/1480U)*100U)==AF_DONE);
    CHECK(af_take_ipv4(&s,output,sizeof(output))==65535);
    for(i=0;i<AF_MAX_PAYLOAD;i+=97) CHECK(output[20+i]==(af_u8)(i%251));
    CHECK(checksum(output,20)==0);
    begin();
    /* offset=65528, final length=8 -> strictly exceeds max IPv4 payload. */
    n=frag(packet,1,65528,8,0,2,2);
    CHECK(af_feed_ipv4(&s,99,0,packet,n)==AF_BAD);
}
static void bypass(void)
{
    unsigned n;
    begin();
    n=frag(packet,1,0,3,0,2,2);
    CHECK(af_feed_ipv4(&s,99,1,packet,n)==AF_BYPASS);
    n=frag(packet,1,0,3,0,2,2); put16(packet+6,0x4000);refresh_sum(packet);
    CHECK(af_feed_ipv4(&s,99,1,packet,n)==AF_BYPASS);
}
/* Deterministic generated cases: sizes, order, IDs, peer-owned addresses. */
static af_u32 prng_state = 0x8a532b17U;
static af_u32 rand32(void)
{
    af_u32 x = prng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return prng_state = x;
}
static void generated(void)
{
    unsigned k;
    for (k=0;k<4000;k++) {
        af_u32 r=rand32();
        unsigned first = 8U * ((r % 90U) + 1U);
        unsigned tail = ((r >> 9) % 1200U) + 1U;
        unsigned ident = (r >> 17) & 0xffffU;
        unsigned src = r & 0xffffU, dst = (r >> 16) & 0xffffU;
        unsigned i;
        begin();
        if (r & 0x80U) {
            CHECK(feed(ident, first, tail, 0, src, dst,99,100)==AF_WAIT);
            CHECK(feed(ident, 0, first, 1, src, dst,99,110)==AF_DONE);
        } else {
            CHECK(feed(ident, 0, first, 1, src, dst,99,100)==AF_WAIT);
            CHECK(feed(ident, first, tail, 0, src, dst,99,110)==AF_DONE);
        }
        CHECK(af_take_ipv4(&s, output, sizeof(output)) == (int)(20U+first+tail));
        CHECK(checksum(output,20)==0U);
        for (i=0;i<first+tail;i+=31U)
            CHECK(output[20+i] == (af_u8)(i%251U));
    }
}
int main(void)
{
    basic(); reverse(); overlap_and_duplicate(); epoch_and_id();
    timeouts(); invalid(); final_bounds(); complete_not_ready(); large(); bypass(); generated();
    printf("AUPE frag safety tests: %u checks, %u failures\n",count,failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
