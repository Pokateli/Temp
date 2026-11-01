/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AUPE_RUST_KERNEL_CORE_ABI_H
#define AUPE_RUST_KERNEL_CORE_ABI_H
/* Kernel-facing C ABI to AUPE Rust no_std state machine; NOT a Linux .ko.
 * Functions are available ONLY once a Rust-for-Linux-compatible build
 * supplies the object file and confirms symbol/layout/ABI compatibility.
 * The owner must serialize calls, pin keypair references and authenticate
 * Noise/AEAD; the dispatch API does not perform AEAD or a WG handshake.
 */
#ifdef __KERNEL__
#include <linux/stddef.h>
#else
#include <stddef.h>
#endif
#include "aupe_crypto_binding.h"
#include "aupe_dispatch.h"
struct aupe_rs_kernel_peer;
size_t aupe_rs_peer_size(void);
size_t aupe_rs_peer_align(void);
size_t aupe_rs_workspace_size(void);
size_t aupe_rs_workspace_align(void);
size_t aupe_rs_ref_size(void);
size_t aupe_rs_ref_align(void);
int aupe_rs_peer_init(struct aupe_rs_kernel_peer *peer, const af_u8 key[32],
                      struct af_state *a, struct af_state *b,
                      struct af_state *c, struct af_state *d);
int aupe_rs_peer_install(struct aupe_rs_kernel_peer *peer,
                         const af_u8 confirmed_public_key[32],
                         struct aupe_crypto_ref reference, af_u64 now,
                         af_u64 *epoch);
int aupe_rs_peer_verified_rx(struct aupe_rs_kernel_peer *peer,
                              struct aupe_crypto_ref authenticated_ref,
                              af_u64 authenticated_counter, af_u64 now,
                              const af_u8 *plaintext, af_u32 bytes,
                              af_u8 *scratch, af_u32 capacity,
                              aupe_rx_emit_fn emit, void *emit_context);
int aupe_rs_peer_retire_previous(struct aupe_rs_kernel_peer *peer,
                                  struct aupe_crypto_ref previous);
void aupe_rs_peer_close(struct aupe_rs_kernel_peer *peer);
#endif
