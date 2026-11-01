/* SPDX-License-Identifier: GPL-2.0-only */
/* Synthetic post-AEAD integration: NOT WireGuard framing/Noise E2E. */
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aupe_crypto_binding.h"
#include "aupe_dispatch.h"

#define T(cond) do { checks++; if (!(cond)) { fails++; if(fails<8) fprintf(stderr,"FAIL %d: %s\n",__LINE__,#cond); } } while (0)
static unsigned long long checks, fails;
static const af_u8 ipv4[28] = {
 0x45,0x00,0x00,0x1c,0x00,0x01,0x00,0x00,0x40,0x11,
 0x1e,0xac,0xac,0x10,0x02,0x02,0xac,0x10,0x02,0x02,
 1,2,3,4,5,6,7,8
};
static int emit(void *ctx, const af_u8 *packet, af_u32 n)
{
    int *hits=(int *)ctx;
    if (n != sizeof(ipv4) || memcmp(packet,ipv4,n)) return -1;
    (*hits)++;
    return 0;
}
static int encrypt_packet(const af_u8 key[32], const af_u8 nonce[12],
                          af_u8 *ct, af_u8 tag[16])
{
    EVP_CIPHER_CTX *ctx=EVP_CIPHER_CTX_new();
    int written=0, final=0, ok=0;
    if (!ctx) return 0;
    ok = EVP_EncryptInit_ex(ctx, EVP_chacha20_poly1305(), NULL,key,nonce) == 1 &&
         EVP_EncryptUpdate(ctx,ct,&written,ipv4,sizeof(ipv4)) == 1 &&
         written == (int)sizeof(ipv4) &&
         EVP_EncryptFinal_ex(ctx,ct+written,&final) == 1 &&
         EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_AEAD_GET_TAG,16,tag)==1;
    EVP_CIPHER_CTX_free(ctx);
    return ok;
}
static int decrypt_packet(const af_u8 key[32], const af_u8 nonce[12],
                          const af_u8 *ct,const af_u8 tag[16],af_u8 *pt)
{
    EVP_CIPHER_CTX *ctx=EVP_CIPHER_CTX_new();
    int written=0,final=0,ok=0;
    if (!ctx) return 0;
    ok = EVP_DecryptInit_ex(ctx, EVP_chacha20_poly1305(), NULL,key,nonce) == 1 &&
         EVP_DecryptUpdate(ctx,pt,&written,ct,sizeof(ipv4))==1 &&
         EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_AEAD_SET_TAG,16,(void *)tag)==1 &&
         EVP_DecryptFinal_ex(ctx,pt+written,&final)==1 &&
         written==(int)sizeof(ipv4);
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) memset(pt,0,sizeof(ipv4));
    return ok;
}
int main(void)
{
    struct aupe_crypto_binding binding={0};
    struct aupe_rx_gate gate={0};
    struct aupe_keypair_bank bank={0};
    struct aupe_pair_mux mux={0};
    struct af_state *slots[4]={0};
    af_u8 pk[32]={2},key1[32]={7},key2[32]={8};
    af_u8 nonce[12]={0},ct[28],tag[16],plain[28],scratch[AF_MAX_PACKET];
    af_u64 e1=0,e2=0,e=0;
    int ids[2]={0},hits=0,rv,i;
    struct aupe_crypto_ref a={&ids[0],1},b={&ids[1],2};
    for(i=0;i<4;i++) {
        slots[i]=calloc(1,sizeof(*slots[i]));
        if (!slots[i]) {fprintf(stderr,"alloc failed\n"); return 2;}
    }
    T(aupe_pair_mux_init(&mux,slots[0],slots[1],slots[2],slots[3])==AF_WAIT);
    T(aupe_binding_init(&binding,pk)==AF_WAIT);
    T(aupe_binding_install(&binding,pk,a,1000,&e1)==AF_WAIT && e1==1);
    T(aupe_gate_bind(&gate,e1)==AF_WAIT);
    T(aupe_pair_mux_rotate(&mux,e1)==AF_WAIT);
    T(aupe_keypairs_install(&bank,e1,1000)==AF_WAIT);
    T(encrypt_packet(key1,nonce,ct,tag));
    T(decrypt_packet(key1,nonce,ct,tag,plain));
    T(aupe_binding_resolve(&binding,a,1001,&e)==AF_WAIT && e==e1);
    rv=aupe_dispatch_keypair(&gate,&mux,&bank,e,0,1001,plain,sizeof(plain),
                             scratch,sizeof(scratch),emit,&hits);
    T(rv==AF_BYPASS && hits==1);
    rv=aupe_dispatch_keypair(&gate,&mux,&bank,e,0,1002,plain,sizeof(plain),
                             scratch,sizeof(scratch),emit,&hits);
    T(rv==AUPE_REPLAY_DUPLICATE && hits==1);
    tag[0]^=1;
    T(!decrypt_packet(key1,nonce,ct,tag,plain) && hits==1);
    tag[0]^=1;
    T(!decrypt_packet(key2,nonce,ct,tag,plain) && hits==1);
    T(aupe_binding_resolve(&binding,b,1003,&e)==AF_STALE_EPOCH && e==0);
    T(aupe_binding_install(&binding,pk,b,2000,&e2)==AF_WAIT && e2==2);
    T(aupe_gate_bind(&gate,e2)==AF_WAIT);
    T(aupe_pair_mux_rotate(&mux,e2)==AF_WAIT);
    T(aupe_keypairs_install(&bank,e2,2000)==AF_WAIT);
    T(encrypt_packet(key2,nonce,ct,tag));
    T(decrypt_packet(key2,nonce,ct,tag,plain));
    T(aupe_binding_resolve(&binding,b,2001,&e)==AF_WAIT && e==e2);
    T(aupe_dispatch_keypair(&gate,&mux,&bank,e,0,2001,plain,sizeof(plain),
                             scratch,sizeof(scratch),emit,&hits)==AF_BYPASS && hits==2);
    T(aupe_binding_resolve(&binding,a,2002,&e)==AF_WAIT && e==e1);
    nonce[4]=1;
    T(encrypt_packet(key1,nonce,ct,tag));
    T(decrypt_packet(key1,nonce,ct,tag,plain));
    T(aupe_dispatch_keypair(&gate,&mux,&bank,e,1,2002,plain,sizeof(plain),
                             scratch,sizeof(scratch),emit,&hits)==AF_BYPASS && hits==3);
    T(aupe_binding_retire_previous(&binding,a)==AF_WAIT);
    T(aupe_keypairs_retire_previous(&bank,e1)==AF_WAIT);
    aupe_pair_mux_retire_previous(&mux);
    T(aupe_binding_resolve(&binding,a,2003,&e)==AF_STALE_EPOCH && e==0);
    T(hits==3);
    aupe_binding_close(&binding);
    aupe_pair_mux_clear(&mux);
    aupe_keypairs_clear(&bank);
    for(i=0;i<4;i++) {memset(slots[i],0,sizeof(*slots[i]));free(slots[i]);}
    printf("AUPE synthetic AEAD to RX: %llu checks, %llu failures\n",checks,fails);
    return fails ? EXIT_FAILURE : EXIT_SUCCESS;
}
