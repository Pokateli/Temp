/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "aupe_crypto_binding.h"
#include "aupe_handles.h"
#include "aupe_gate.h"
/* Rust FFI: unique suffix permits side-by-side differential testing. */
void aupe_replay_clear_rs(struct aupe_replay_window *);
void aupe_replay_bind_rs(struct aupe_replay_window *, af_u64);
int aupe_replay_accept_rs(struct aupe_replay_window *, af_u64, af_u64);
int aupe_gate_bind_rs(struct aupe_rx_gate *, af_u64);
void aupe_gate_unbind_rs(struct aupe_rx_gate *);
int aupe_gate_check_rs(const struct aupe_rx_gate *, af_u64);
void aupe_gate_close_rs(struct aupe_rx_gate *);
void aupe_handles_init_rs(struct aupe_handle_table *);
void aupe_handles_init_seed_rs(struct aupe_handle_table *, af_u64);
int aupe_handles_publish_rs(struct aupe_handle_table *, void *, af_u64 *);
void *aupe_handles_lookup_rs(const struct aupe_handle_table *, af_u64);
void *aupe_handles_remove_rs(struct aupe_handle_table *, af_u64);
void *aupe_handles_drain_rs(struct aupe_handle_table *, unsigned);
void aupe_keypairs_clear_rs(struct aupe_keypair_bank *);
int aupe_keypairs_install_rs(struct aupe_keypair_bank *, af_u64, af_u64);
int aupe_keypairs_accept_rs(struct aupe_keypair_bank *, af_u64, af_u64, af_u64);
int aupe_keypairs_retire_previous_rs(struct aupe_keypair_bank *, af_u64);
int aupe_binding_init_rs(struct aupe_crypto_binding *, const af_u8 *);
int aupe_binding_install_rs(struct aupe_crypto_binding *, const af_u8 *, struct aupe_crypto_ref, af_u64, af_u64 *);
int aupe_binding_resolve_rs(struct aupe_crypto_binding *, struct aupe_crypto_ref, af_u64, af_u64 *);
int aupe_binding_retire_previous_rs(struct aupe_crypto_binding *, struct aupe_crypto_ref);
void aupe_binding_close_rs(struct aupe_crypto_binding *);
size_t aupe_rust_sizeof_replay(void);
size_t aupe_rust_sizeof_gate(void);
size_t aupe_rust_sizeof_handles(void);
size_t aupe_rust_sizeof_keypairs(void);
size_t aupe_rust_sizeof_binding(void);
size_t aupe_rust_alignof_binding(void);
size_t aupe_rust_alignof_keypairs(void);
