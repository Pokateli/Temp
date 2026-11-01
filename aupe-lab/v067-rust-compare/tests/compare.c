/* SPDX-License-Identifier: GPL-2.0-only */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stddef.h>
#include "ffi.h"
#define MUST(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d (%s)\n",__FILE__,__LINE__,#x);exit(1); } } while(0)
static uint64_t rng = 0xa5c4c3e291452201ULL;
static uint64_t rand64(void) { rng^=rng<<13; rng^=rng>>7; rng^=rng<<17; return rng; }
static volatile uint64_t blackhole;
static uint64_t ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC_RAW,&t); return (uint64_t)t.tv_sec*1000000000ULL+(uint64_t)t.tv_nsec; }
#ifndef AUPE_C_ONLY
static int replay_equal(const struct aupe_replay_window *a,const struct aupe_replay_window *b) {
 return memcmp(a->seen,b->seen,sizeof(a->seen))==0 && a->epoch==b->epoch && a->highest==b->highest && a->valid==b->valid && a->initialized==b->initialized;
}
static int bank_equal(const struct aupe_keypair_bank *a,const struct aupe_keypair_bank *b) {
 return replay_equal(&a->active.replay,&b->active.replay) && replay_equal(&a->previous.replay,&b->previous.replay) &&
 a->active.epoch==b->active.epoch && a->active.valid==b->active.valid &&
 a->previous.epoch==b->previous.epoch && a->previous.valid==b->previous.valid &&
 a->high_watermark==b->high_watermark && a->previous_deadline_ms==b->previous_deadline_ms &&
 a->last_now_ms==b->last_now_ms && a->clock_started==b->clock_started && a->enabled==b->enabled;
}
static int binding_equal(const struct aupe_crypto_binding *a,const struct aupe_crypto_binding *b) {
 return memcmp(a->authenticated_public_key,b->authenticated_public_key,32)==0 &&
 a->active.ref.keypair==b->active.ref.keypair && a->active.ref.generation==b->active.ref.generation &&
 a->previous.ref.keypair==b->previous.ref.keypair && a->previous.ref.generation==b->previous.ref.generation &&
 a->active.epoch==b->active.epoch && a->active.valid==b->active.valid &&
 a->previous.epoch==b->previous.epoch && a->previous.valid==b->previous.valid &&
 a->highest_owner_generation==b->highest_owner_generation && a->highest_aupe_epoch==b->highest_aupe_epoch &&
 a->previous_deadline_ms==b->previous_deadline_ms && a->last_clock_ms==b->last_clock_ms &&
 a->initialized==b->initialized && a->closed==b->closed && a->clock_started==b->clock_started;
}
#endif
static void layout(void) {
 printf("C sizes: replay=%zu gate=%zu handles=%zu keypairs=%zu binding=%zu\n",sizeof(struct aupe_replay_window),sizeof(struct aupe_rx_gate),sizeof(struct aupe_handle_table),sizeof(struct aupe_keypair_bank),sizeof(struct aupe_crypto_binding));
#ifndef AUPE_C_ONLY
 MUST(aupe_rust_sizeof_replay()==sizeof(struct aupe_replay_window));
 MUST(aupe_rust_sizeof_gate()==sizeof(struct aupe_rx_gate));
 MUST(aupe_rust_sizeof_handles()==sizeof(struct aupe_handle_table));
 MUST(aupe_rust_sizeof_keypairs()==sizeof(struct aupe_keypair_bank));
 MUST(aupe_rust_sizeof_binding()==sizeof(struct aupe_crypto_binding));
 MUST(aupe_rust_alignof_keypairs()==_Alignof(struct aupe_keypair_bank));
 MUST(aupe_rust_alignof_binding()==_Alignof(struct aupe_crypto_binding));
 puts("ABI sizes/alignments: MATCH");
#endif
}
static void test_replay(void) {
 struct aupe_replay_window a={0};
#ifndef AUPE_C_ONLY
 struct aupe_replay_window b={0};
#endif
 uint64_t epoch=1;
 aupe_replay_bind(&a,epoch);
#ifndef AUPE_C_ONLY
 aupe_replay_bind_rs(&b,epoch);
#endif
 for(unsigned i=0;i<300000;++i){
   uint64_t c=rand64()%10000u;
   if(i%17011==0){ epoch++; aupe_replay_bind(&a,epoch);
#ifndef AUPE_C_ONLY
     aupe_replay_bind_rs(&b,epoch);
#endif
   }
   if(i%907==0)c=UINT64_MAX;
   int x=aupe_replay_accept(&a,epoch,c);
#ifndef AUPE_C_ONLY
   int y=aupe_replay_accept_rs(&b,epoch,c);
   MUST(x==y); MUST(replay_equal(&a,&b));
#else
   blackhole+=(unsigned)x;
#endif
 }
 puts("replay: 300000 state transitions PASS");
}
static void test_gate(void) {
 struct aupe_rx_gate a={0};
#ifndef AUPE_C_ONLY
 struct aupe_rx_gate b={0};
#endif
 for(unsigned i=0;i<30000;++i){
  uint64_t e=(rand64()%10000)+1; int x;
  switch(i%7){
   case 0: aupe_gate_unbind(&a);
#ifndef AUPE_C_ONLY
   aupe_gate_unbind_rs(&b);
#endif
   break;
   case 1: x=aupe_gate_check(&a,e);
#ifndef AUPE_C_ONLY
   MUST(x==aupe_gate_check_rs(&b,e));
#else
   blackhole+=(unsigned)x;
#endif
   break;
   case 2: if(i%101==0){aupe_gate_close(&a);
#ifndef AUPE_C_ONLY
   aupe_gate_close_rs(&b);
#endif
   } break;
   default: x=aupe_gate_bind(&a,e);
#ifndef AUPE_C_ONLY
   MUST(x==aupe_gate_bind_rs(&b,e));
#else
   blackhole+=(unsigned)x;
#endif
  }
#ifndef AUPE_C_ONLY
  MUST(a.state==b.state && a.epoch==b.epoch && a.high_watermark==b.high_watermark);
#endif
 }
 puts("gate: 30000 transitions PASS");
}
static void test_handles(void) {
 struct aupe_handle_table a={0};
#ifndef AUPE_C_ONLY
 struct aupe_handle_table b={0};
#endif
 int peers[100]; uint64_t handles[100]={0};
 aupe_handles_init_seed(&a,0x1122334455ULL);
#ifndef AUPE_C_ONLY
 aupe_handles_init_seed_rs(&b,0x1122334455ULL);
#endif
 for(unsigned i=0;i<50000;++i){
  unsigned j=(unsigned)(rand64()%100u);
  if((i%3)!=0 && handles[j]!=0){
   void *x=aupe_handles_lookup(&a,handles[j]);
#ifdef AUPE_C_ONLY
   (void)x;
#endif
#ifndef AUPE_C_ONLY
   MUST(x==aupe_handles_lookup_rs(&b,handles[j]));
#endif
   if(i%9==0){x=aupe_handles_remove(&a,handles[j]);
#ifndef AUPE_C_ONLY
    MUST(x==aupe_handles_remove_rs(&b,handles[j]));
#endif
    handles[j]=0;
   }
  } else if(handles[j]==0){
   uint64_t h=0;
   int x=aupe_handles_publish(&a,&peers[j],&h);
#ifndef AUPE_C_ONLY
   uint64_t hr=0; int y=aupe_handles_publish_rs(&b,&peers[j],&hr);
   MUST(x==y); MUST(h==hr);
#endif
   if(x==AF_WAIT)handles[j]=h;
  }
#ifndef AUPE_C_ONLY
  for(unsigned k=0;k<AUPE_HANDLE_SLOTS;++k){
   MUST(a.slots[k].peer==b.slots[k].peer && a.slots[k].generation==b.slots[k].generation);
  }
#endif
 }
 puts("handles: 50000 transitions PASS");
}
static void test_keypairs(void) {
 struct aupe_keypair_bank a={0};
#ifndef AUPE_C_ONLY
 struct aupe_keypair_bank b={0};
#endif
 uint64_t now=0,epoch=1;
 for(unsigned i=0;i<120000;++i){
  now+=2;
  int x;
  if(i%1103==0){epoch++;x=aupe_keypairs_install(&a,epoch,now);
#ifndef AUPE_C_ONLY
   MUST(x==aupe_keypairs_install_rs(&b,epoch,now));
#endif
  }else if(i%1301==0){x=aupe_keypairs_retire_previous(&a,epoch-1);
#ifndef AUPE_C_ONLY
   MUST(x==aupe_keypairs_retire_previous_rs(&b,epoch-1));
#endif
  }else{
   uint64_t chosen=(i%10==0)?epoch-1:epoch;
   uint64_t counter=(i%701==0)?0:(rand64()%12000u);
   x=aupe_keypairs_accept(&a,chosen,counter,now);
#ifndef AUPE_C_ONLY
   MUST(x==aupe_keypairs_accept_rs(&b,chosen,counter,now));
#endif
  }
#ifndef AUPE_C_ONLY
  MUST(bank_equal(&a,&b));
#else
  blackhole+=(unsigned)x;
#endif
 }
 puts("keypairs: 120000 state transitions PASS");
}
static void test_binding(void) {
 struct aupe_crypto_binding a={0};
#ifndef AUPE_C_ONLY
 struct aupe_crypto_binding b={0};
#endif
 int ids[110]; af_u8 pubkey[32]={4,8,15,16,23,42};
 MUST(aupe_binding_init(&a,pubkey)==AF_WAIT);
#ifndef AUPE_C_ONLY
 MUST(aupe_binding_init_rs(&b,pubkey)==AF_WAIT);
#endif
 uint64_t epoch=0,generation=1,now=0;
 for(unsigned i=0;i<100000;i++){
  now+=5;
  struct aupe_crypto_ref r={.keypair=&ids[generation%110u],.generation=generation};
  af_u64 out=99;
#ifndef AUPE_C_ONLY
  af_u64 other=99;
#endif
  int x;
  if(i%1237==0){generation++;r.generation=generation;r.keypair=&ids[generation%110u];x=aupe_binding_install(&a,pubkey,r,now,&out);
#ifndef AUPE_C_ONLY
    int y=aupe_binding_install_rs(&b,pubkey,r,now,&other);
    MUST(x==y);MUST(out==other);
#endif
    if(x==AF_WAIT)epoch=out;
  }else if(i%2381==0){x=aupe_binding_retire_previous(&a,r);
#ifndef AUPE_C_ONLY
    MUST(x==aupe_binding_retire_previous_rs(&b,r));
#endif
  }else{x=aupe_binding_resolve(&a,r,now,&out);
#ifndef AUPE_C_ONLY
    int y=aupe_binding_resolve_rs(&b,r,now,&other);
    MUST(x==y); MUST(out==other);
#endif
  }
#ifndef AUPE_C_ONLY
  MUST(binding_equal(&a,&b));
#else
  blackhole+=(unsigned)x;
#endif
 }
 (void)epoch;
 aupe_binding_close(&a);
#ifndef AUPE_C_ONLY
 aupe_binding_close_rs(&b); MUST(binding_equal(&a,&b));
#endif
 puts("binding: 100000 transitions PASS");
}
static void bench(void) {
 const int N=2000000;
 struct aupe_replay_window a={0};
#ifndef AUPE_C_ONLY
 struct aupe_replay_window b={0};
#endif
 aupe_replay_bind(&a,100);
#ifndef AUPE_C_ONLY
 aupe_replay_bind_rs(&b,100);
#endif
 uint64_t t=ns();
 for(int i=0;i<N;++i) blackhole+=(unsigned)aupe_replay_accept(&a,100,(uint64_t)i);
 uint64_t delta=ns()-t;
 printf("BENCH,replay,C,%d,%llu,%.2f\n",N,(unsigned long long)delta,(double)delta/N);
#ifndef AUPE_C_ONLY
 t=ns();
 for(int i=0;i<N;++i) blackhole+=(unsigned)aupe_replay_accept_rs(&b,100,(uint64_t)i);
 delta=ns()-t;
 printf("BENCH,replay,Rust,%d,%llu,%.2f\n",N,(unsigned long long)delta,(double)delta/N);
#endif
}
int main(void){
 layout();test_replay();test_gate();test_handles();test_keypairs();test_binding();bench();
#ifdef AUPE_C_ONLY
 puts("C-only transition tests complete; Rust equivalence NOT tested.");
#else
 puts("C/Rust differential checks PASS.");
#endif
 return 0;
}
