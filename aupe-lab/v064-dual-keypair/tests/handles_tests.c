/* SPDX-License-Identifier: GPL-2.0-only */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aupe_handles.h"

static unsigned long long checks;
static unsigned errors;
#define T(x) do { __atomic_fetch_add(&checks, 1ULL, __ATOMIC_RELAXED); \
  if (!(x)) { fprintf(stderr,"handles:%d failed: %s\n",__LINE__,#x); \
    __atomic_fetch_add(&errors,1U,__ATOMIC_RELAXED); } } while (0)
static int fake_peers[AUPE_HANDLE_SLOTS+4];

static void simple_lifetime(void)
{
    struct aupe_handle_table t;
    af_u64 h1=0, h2=0, h3=0;
    aupe_handles_init(&t);
    T(aupe_handles_publish(&t, NULL, &h1)==AF_BAD);
    T(aupe_handles_publish(NULL, fake_peers, &h1)==AF_BAD);
    T(aupe_handles_publish(&t, fake_peers, NULL)==AF_BAD);
    T(aupe_handles_lookup(&t, 0)==NULL);
    T(aupe_handles_lookup(&t, 0xffffffffffffffffULL)==NULL);
    T(aupe_handles_remove(&t,0)==NULL);
    T(aupe_handles_publish(&t,&fake_peers[0],&h1)==AF_WAIT);
    T(h1==257ULL);
    T(aupe_handles_lookup(&t,h1)==&fake_peers[0]);
    T(aupe_handles_publish(&t,&fake_peers[1],&h2)==AF_WAIT);
    T(h2==258ULL);
    T(aupe_handles_lookup(&t,h2)==&fake_peers[1]);
    T(aupe_handles_lookup(&t,h1+256ULL)==NULL);
    T(aupe_handles_remove(&t,h1)==&fake_peers[0]);
    T(aupe_handles_lookup(&t,h1)==NULL);
    T(aupe_handles_remove(&t,h1)==NULL);
    T(aupe_handles_publish(&t,&fake_peers[2],&h3)==AF_WAIT);
    T(h3==513ULL);
    T(h3!=h1 && h3!=h2);
    T(aupe_handles_lookup(&t,h1)==NULL);
    T(aupe_handles_lookup(&t,h2)==&fake_peers[1]);
    T(aupe_handles_lookup(&t,h3)==&fake_peers[2]);
    T(aupe_handles_drain(&t,0)==&fake_peers[2]);
    T(aupe_handles_drain(&t,0)==NULL);
    T(aupe_handles_lookup(&t,h3)==NULL);
    T(aupe_handles_drain(&t,AUPE_HANDLE_SLOTS)==NULL);
    T(aupe_handles_drain(NULL,0)==NULL);
}

static void new_registry_incarnation(void)
{
    struct aupe_handle_table old_table, fresh;
    af_u64 old_handle = 0, new_handle = 0;
    aupe_handles_init_seed(&old_table, 0xaaaabbbbccccULL);
    T(aupe_handles_publish(&old_table, &fake_peers[0], &old_handle)==AF_WAIT);
    T(aupe_handles_remove(&old_table, old_handle)==&fake_peers[0]);
    /* A new registry is seeded independently by get_random_u64 in kernel. */
    aupe_handles_init_seed(&fresh, 0xddddeeeeffffULL);
    T(aupe_handles_publish(&fresh, &fake_peers[1], &new_handle)==AF_WAIT);
    T(old_handle!=new_handle);
    T(aupe_handles_lookup(&fresh,old_handle)==NULL);
    T(aupe_handles_lookup(&fresh,new_handle)==&fake_peers[1]);
    T(aupe_handles_remove(&fresh,new_handle)==&fake_peers[1]);
}

static void exhaustion_and_no_wrap(void)
{
    struct aupe_handle_table t;
    af_u64 h[AUPE_HANDLE_SLOTS], empty=123;
    unsigned i;
    aupe_handles_init(&t);
    for (i=0;i<AUPE_HANDLE_SLOTS;i++) {
        T(aupe_handles_publish(&t,&fake_peers[i],&h[i])==AF_WAIT);
        T((h[i]&255ULL)==i+1);
    }
    T(aupe_handles_publish(&t,&fake_peers[0],&empty)==AUPE_HANDLE_FULL);
    T(empty==0);
    for(i=0;i<AUPE_HANDLE_SLOTS;i++) {
        T(aupe_handles_lookup(&t,h[i])==&fake_peers[i]);
        T(aupe_handles_remove(&t,h[i])==&fake_peers[i]);
        T(aupe_handles_lookup(&t,h[i])==NULL);
    }
    /* Exhausted generation never wraps to a previously issued token. */
    t.slots[0].generation=AUPE_HANDLE_GEN_MAX;
    T(aupe_handles_publish(&t,&fake_peers[0],&empty)==AF_WAIT);
    T((empty&255ULL)==2ULL);  /* Permanently retired slot 0. */
    T(aupe_handles_lookup(&t,((af_u64)1<<8)|1)==NULL);
    T(aupe_handles_remove(&t,((af_u64)1<<8)|1)==NULL);
    T(aupe_handles_remove(&t,empty)==&fake_peers[0]);
    for(i=1;i<AUPE_HANDLE_SLOTS;i++) t.slots[i].generation=AUPE_HANDLE_GEN_MAX;
    T(aupe_handles_publish(&t,&fake_peers[0],&empty)==AUPE_HANDLE_FULL);
    T(empty==0);
}

/* Real core with owner-provided pthread mutex, mirroring kernel registry lock.
 * Publication/unpublication + reference acquisition are one critical section. */
struct stress_ctx {
    pthread_mutex_t lock;
    struct aupe_handle_table table;
    af_u64 handle;
    int *peer;
};
static void *reader(void *opaque)
{
    struct stress_ctx *c=opaque;
    unsigned i;
    for(i=0;i<60000;i++) {
        af_u64 h;
        void *p;
        pthread_mutex_lock(&c->lock);
        h=c->handle;
        p=aupe_handles_lookup(&c->table,h);
        T((h==0 && p==NULL) || (h!=0 && p==c->peer));
        pthread_mutex_unlock(&c->lock);
    }
    return NULL;
}
static void *rotator(void *opaque)
{
    struct stress_ctx *c=opaque;
    unsigned i;
    for(i=0;i<10000;i++) {
        af_u64 old, fresh=0;
        void *removed;
        pthread_mutex_lock(&c->lock);
        old=c->handle;
        removed=aupe_handles_remove(&c->table, old);
        T(removed==c->peer);
        T(aupe_handles_lookup(&c->table,old)==NULL);
        c->handle=0;
        T(aupe_handles_publish(&c->table,c->peer,&fresh)==AF_WAIT);
        c->handle=fresh;
        T(aupe_handles_lookup(&c->table,old)==NULL);
        pthread_mutex_unlock(&c->lock);
    }
    return NULL;
}
static void concurrent_owner(void)
{
    struct stress_ctx c;
    pthread_t r[5];
    unsigned i;
    memset(&c,0,sizeof(c));
    c.peer=&fake_peers[0];
    T(pthread_mutex_init(&c.lock,NULL)==0);
    aupe_handles_init(&c.table);
    T(aupe_handles_publish(&c.table,c.peer,&c.handle)==AF_WAIT);
    for(i=0;i<4;i++) T(pthread_create(&r[i],NULL,reader,&c)==0);
    T(pthread_create(&r[4],NULL,rotator,&c)==0);
    for(i=0;i<5;i++) T(pthread_join(r[i],NULL)==0);
    T(aupe_handles_lookup(&c.table,c.handle)==c.peer);
    T(pthread_mutex_destroy(&c.lock)==0);
}
static void churn_many_peers(void)
{
    struct aupe_handle_table t;
    af_u64 handles[AUPE_HANDLE_SLOTS]={0};
    unsigned i;
    aupe_handles_init(&t);
    for(i=0;i<100000;i++) {
        unsigned slot=(i*37U)%AUPE_HANDLE_SLOTS;
        af_u64 replacement;
        void *prior;
        /* Stable per-peer slot ownership, despite the same notional inner IP. */
        if(handles[slot]) {
            prior=aupe_handles_remove(&t,handles[slot]);
            T(prior==&fake_peers[slot]);
            T(aupe_handles_lookup(&t,handles[slot])==NULL);
        }
        T(aupe_handles_publish(&t,&fake_peers[slot],&replacement)==AF_WAIT);
        handles[slot]=replacement;
        T(aupe_handles_lookup(&t,replacement)==&fake_peers[slot]);
    }
    for(i=0;i<AUPE_HANDLE_SLOTS;i++) T(aupe_handles_remove(&t,handles[i])==&fake_peers[i]);
}
int main(void)
{
    simple_lifetime();
    new_registry_incarnation();
    exhaustion_and_no_wrap();
    concurrent_owner();
    churn_many_peers();
    printf("AUPE peer handle tests: %llu checks, %u failures\n",checks,errors);
    return errors ? EXIT_FAILURE : EXIT_SUCCESS;
}