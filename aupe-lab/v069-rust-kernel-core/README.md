# AUPE 0.5.69 — allocator-free Rust kernel-facing peer core

AUPE 0.5.68's nine post-AEAD algorithm implementations are preserved. `rust/src/kernel_core.rs` now ports the peer/crypto-owner *state machine* to `no_std` Rust with caller-allocated stable workspaces and C ABI. It is designed to be wired into a future Linux adapter, but is **not** a loadable Rust kernel module. A real kernel adapter still needs Rust-for-Linux compatible kernel headers, exported API, locking, RCU, refcounts, `GFP_KERNEL` allocation, Linux module metadata, and verified AEAD/Noise owner integration.

The peer core performs no I/O, no allocation, no locks, no cryptographic AEAD, and no UDP/network functions. The owner **MUST** serialize calls, authenticate and pin a keypair before invoking RX, and enforce unique authenticated public-key identities. Duplicate inner IPs remain supported because inner IPv4 is not an identity or registry key.

**New:** irreversible fail-closed partial rekey behavior, current/previous pinned keypair retirement, hard revocation, no_std ABI functions for peer init/size/align/install/verified_rx/retire/close. The userspace model also closes on any partial install failure.

Test commands (compilers need Rust 1.89):

```
cargo test --offline --locked --manifest-path rust/Cargo.toml --release
cargo build --offline --locked --manifest-path rust/Cargo.toml --release --bin kernel_smoke
rustc --edition=2024 --crate-name aupe_rust_core --crate-type=rlib -C panic=abort rust/src/lib.rs -o /tmp/libaupe_nostd.rlib
CC=gcc bash scripts/run_ffi_diff.sh
```

The VM smoke is intended to execute **locally in ChatGPT's Debian13 QEMU** using a read-only VirtIO FAT share. The GitHub workflow, if used for Rust cross-dependency compilation, MUST NOT start a VM and MUST NOT leave artifacts in Actions storage.
