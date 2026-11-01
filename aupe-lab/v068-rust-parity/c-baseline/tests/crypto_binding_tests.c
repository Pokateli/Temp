/* SPDX-License-Identifier: GPL-2.0-only */
#include "aupe_crypto_binding.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#define T(x) do { checks++; if (!(x)) { failures++; if (failures < 10) fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); } } while(0)
static unsigned long long checks,failures;
static void cases(void)
{
    struct aupe_crypto_binding b = {0}, other = {0};
    af_u8 pub[32]={3}, wrong[32]={4}, zero[32]={0};
    int ids[4]={0};
    struct aupe_crypto_ref a={&ids[0],1}, c={&ids[1],2}, d={&ids[2],3}, e={&ids[3],4};
    struct aupe_crypto_ref invalid={NULL,0};
    af_u64 ep=9;
    T(aupe_binding_init(&b,zero)==AF_BAD);
    T(aupe_binding_init(&b,pub)==AF_WAIT);
    T(aupe_binding_init(&b,pub)==AF_BAD);
    T(aupe_binding_install(&b,wrong,a,1000,&ep)==AF_BAD && ep==0);
    T(aupe_binding_install(&b,pub,invalid,1000,&ep)==AF_BAD && ep==0);
    T(aupe_binding_install(&b,pub,a,1000,&ep)==AF_WAIT && ep==1);
    T(aupe_binding_resolve(&b,a,1001,&ep)==AF_WAIT && ep==1);
    T(aupe_binding_install(&b,pub,a,1002,&ep)==AF_ALREADY_BOUND && ep==1);
    T(aupe_binding_install(&b,pub,(struct aupe_crypto_ref){&ids[0],2},1003,&ep)==AF_BAD);
    T(aupe_binding_install(&b,pub,c,1004,&ep)==AF_WAIT && ep==2);
    T(aupe_binding_resolve(&b,a,1005,&ep)==AF_WAIT && ep==1);
    T(aupe_binding_resolve(&b,c,1006,&ep)==AF_WAIT && ep==2);
    T(aupe_binding_install(&b,pub,d,1007,&ep)==AF_WAIT && ep==3);
    T(aupe_binding_resolve(&b,a,1008,&ep)==AF_STALE_EPOCH && ep==0);
    T(aupe_binding_resolve(&b,c,1009,&ep)==AF_WAIT && ep==2);
    T(aupe_binding_install(&b,pub,a,1010,&ep)==AF_STALE_EPOCH && ep==0);
    T(aupe_binding_retire_previous(&b,c)==AF_WAIT);
    T(aupe_binding_resolve(&b,c,1011,&ep)==AF_STALE_EPOCH && ep==0);
    T(aupe_binding_install(&b,pub,e,1012,&ep)==AF_WAIT && ep==4);
    T(aupe_binding_resolve(&b,d,1013,&ep)==AF_WAIT && ep==3);
    T(aupe_binding_resolve(&b,d,181011,&ep)==AF_WAIT && ep==3);
    T(aupe_binding_resolve(&b,d,181012,&ep)==AF_STALE_EPOCH && ep==0);
    T(aupe_binding_resolve(&b,e,181014,&ep)==AF_WAIT && ep==4);
    T(aupe_binding_resolve(&b,e,181013,&ep)==AF_BAD && ep==0);
    T(aupe_binding_init(&other,wrong)==AF_WAIT);
    T(aupe_binding_install(&other,pub,a,1,&ep)==AF_BAD && ep==0);
    T(aupe_binding_install(&other,wrong,a,1,&ep)==AF_WAIT && ep==1);
    T(aupe_binding_resolve(&other,a,2,&ep)==AF_WAIT && ep==1);
    aupe_binding_close(&b);
    T(aupe_binding_resolve(&b,e,181015,&ep)==AF_CLOSED && ep==0);
    T(aupe_binding_install(&b,pub,e,181015,&ep)==AF_CLOSED && ep==0);
    T(aupe_binding_init(&b,pub)==AF_BAD);
    T(!memcmp(b.authenticated_public_key,zero,32));
    aupe_binding_close(&other);
}
static void stress(void)
{
    unsigned i;
    af_u8 key[32]={5};
    struct aupe_crypto_binding b={0};
    int ptrs[2]={0};
    af_u64 now=1,epoch=0;
    T(aupe_binding_init(&b,key)==AF_WAIT);
    for (i=1;i<=100000;i++) {
        struct aupe_crypto_ref a={&ptrs[i%2],i};
        struct aupe_crypto_ref prev={&ptrs[i%2],i-2};
        /* Source pointer may be re-used AFTER rotation retires 2-old. */
        if (i>2) { /* two-keypair overlap forbids aliased address to prior. */
            int ret=aupe_binding_install(&b,key,a,now,&epoch);
            T(ret==AF_BAD && epoch==0);
            /* Explicitly retire the previous object first. */
            T(aupe_binding_retire_previous(&b,prev)==AF_WAIT);
        }
        T(aupe_binding_install(&b,key,a,now,&epoch)==AF_WAIT && epoch==i);
        T(aupe_binding_resolve(&b,a,now,&epoch)==AF_WAIT && epoch==i);
        T(aupe_binding_resolve(&b,(struct aupe_crypto_ref){a.keypair,i+1},now,&epoch)==AF_STALE_EPOCH);
        T(aupe_binding_install(&b,key,a,now,&epoch)==AF_ALREADY_BOUND && epoch==i);
        now+=1;
    }
    aupe_binding_close(&b);
}
int main(void){cases();stress();printf("AUPE authenticated owner binding: %llu checks, %llu failures\n",checks,failures);return failures?EXIT_FAILURE:EXIT_SUCCESS;}
