/* SPDX-License-Identifier: GPL-2.0-only */
/* Real C binary linked against Rust staticlib. Not a crypto verification.
 * This is a trusted post-AEAD C ABI validation in userspace. */
#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aupe_rs_core.h"
#include "aupe_handles.h"
#include "aupe_replay.h"

extern size_t aupe_rust_sizeof_replay(void);
extern size_t aupe_rust_sizeof_gate(void);
extern size_t aupe_rust_sizeof_handles(void);
extern size_t aupe_rust_sizeof_keypairs(void);
extern size_t aupe_rust_sizeof_binding(void);
extern size_t aupe_rust_alignof_binding(void);
extern size_t aupe_rust_alignof_keypairs(void);

typedef struct {
    struct aupe_rs_kernel_peer *state;
    struct af_state *ws[4];
    unsigned emitted;
} owner;

static void *zero_aligned(size_t n, size_t a) {
    void *p = NULL;
    if (a < sizeof(void *)) a = sizeof(void *);
    assert((a & (a - 1)) == 0 && n > 0);
    assert(posix_memalign(&p, a, n) == 0 && p);
    memset(p, 0, n);
    return p;
}
static owner new_owner(const uint8_t key[32]) {
    owner o = {0};
    assert(aupe_rs_peer_align() <= 16);
    assert(aupe_rs_workspace_align() <= 16);
    o.state = zero_aligned(aupe_rs_peer_size(), aupe_rs_peer_align());
    for (unsigned i = 0; i < 4; i++)
        o.ws[i] = zero_aligned(aupe_rs_workspace_size(), aupe_rs_workspace_align());
    assert(aupe_rs_peer_init(o.state,key,o.ws[0],o.ws[1],o.ws[2],o.ws[3]) == 0);
    return o;
}
static void del_owner(owner *o) {
    aupe_rs_peer_close(o->state);
    for (unsigned i=0;i<4;i++) {memset(o->ws[i],0,aupe_rs_workspace_size());free(o->ws[i]);}
    memset(o->state,0,aupe_rs_peer_size());free(o->state);
}
static uint8_t ipv4[28];
static void build_ipv4(void) {
    uint32_t sum=0;
    memset(ipv4,0,sizeof(ipv4));
    ipv4[0]=0x45;ipv4[3]=28;ipv4[8]=64;ipv4[9]=17;
    ipv4[12]=172;ipv4[13]=16;ipv4[14]=2;ipv4[15]=2;
    ipv4[16]=172;ipv4[17]=16;ipv4[18]=2;ipv4[19]=2;
    memcpy(ipv4+20,"AUPE070!",8);
    for(unsigned i=0;i<20;i+=2)sum+=((uint16_t)ipv4[i]<<8)|ipv4[i+1];
    while(sum>>16)sum=(sum&0xffff)+(sum>>16);
    uint16_t cs=~(uint16_t)sum;
    ipv4[10]=cs>>8;ipv4[11]=cs&0xff;
}
static int sink(void *ctx,const af_u8 *pkt,af_u32 n) {
    unsigned *count=ctx;
    assert(n==sizeof(ipv4));assert(memcmp(pkt,ipv4,sizeof(ipv4))==0);
    ++*count;return 0;
}
static int accept(owner *o, struct aupe_crypto_ref r, uint64_t ctr, uint64_t now) {
    uint8_t scratch[65535]={0};
    return aupe_rs_peer_verified_rx(o->state,r,ctr,now,ipv4,28,scratch,
                                      sizeof(scratch),sink,&o->emitted);
}
int main(void) {
    /* Explicit C/Rust layout comparisons before any FFI calls. */
    assert(aupe_rs_ref_size()==sizeof(struct aupe_crypto_ref));
    assert(aupe_rs_ref_align()==_Alignof(struct aupe_crypto_ref));
    assert(aupe_rs_workspace_size()==sizeof(struct af_state));
    assert(aupe_rs_workspace_align()==_Alignof(struct af_state));
    assert(aupe_rust_sizeof_binding()==sizeof(struct aupe_crypto_binding));
    assert(aupe_rust_alignof_binding()==_Alignof(struct aupe_crypto_binding));
    assert(aupe_rust_sizeof_replay()==sizeof(struct aupe_replay_window));
    assert(aupe_rust_sizeof_handles()==sizeof(struct aupe_handle_table));
    assert(aupe_rust_sizeof_keypairs()==sizeof(struct aupe_keypair_bank));
    assert(aupe_rust_alignof_keypairs()==_Alignof(struct aupe_keypair_bank));
    assert(aupe_rust_sizeof_gate()==sizeof(struct aupe_rx_gate));
    uint8_t k1[32]={1},k2[32]={2},wrong[32]={3};
    owner a=new_owner(k1), b=new_owner(k2);
    uint8_t id_a=1,id_ar=2,id_b=3;
    struct aupe_crypto_ref ra={&id_a,10},rar={&id_ar,11},rb={&id_b,10};
    uint64_t ea=0,eb=0;
    assert(aupe_rs_peer_install(a.state,wrong,ra,100,&ea)==AF_BAD);
    assert(ea==0);
    assert(aupe_rs_peer_install(a.state,k1,ra,100,&ea)==AF_WAIT);
    assert(aupe_rs_peer_install(b.state,k2,rb,100,&eb)==AF_WAIT);
    assert(ea==1&&eb==1);
    build_ipv4();
    assert(accept(&a,ra,100,101)==AF_BYPASS);
    assert(accept(&a,ra,100,102)==-13); /* anti replay */
    assert(accept(&b,rb,100,101)==AF_BYPASS); /* same IPv4 independent peer */
    assert(a.emitted==1 && b.emitted==1);
    assert(aupe_rs_peer_install(a.state,k1,rar,200,&ea)==AF_WAIT);
    assert(ea==2);
    assert(accept(&a,ra,101,201)==AF_BYPASS); /* previous is valid */
    assert(aupe_rs_peer_retire_previous(a.state,ra)==AF_WAIT);
    assert(accept(&a,ra,102,202)==AF_STALE_EPOCH);
    assert(accept(&a,rar,101,203)==AF_BYPASS);
    /* 4000 distinct authenticated counters: no false replay rejection. */
    for (uint64_t i=102;i<4102;i++)assert(accept(&a,rar,i,204+i)==AF_BYPASS);
    assert(a.emitted==4003);
    assert(accept(&a,rar,4101,4306)==-13);
    aupe_rs_peer_close(a.state);
    assert(accept(&a,rar,4102,4307)==AF_CLOSED);
    assert(accept(&b,rb,101,4307)==AF_BYPASS);
    del_owner(&a);del_owner(&b);
    puts("AUPE070_C_RUST_ABI_PASS: layout, peer isolation with identical IP, 4000 counters, anti-replay, rekey and revoke");
    return 0;
}
