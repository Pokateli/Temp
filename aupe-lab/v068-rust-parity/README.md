# AUPE 0.5.68 — Rust parity port to C 0.5.66 laboratory capability

- C 0.5.66 baseline included unchanged.
- Rust `no_std` algorithm implementations for nine C files, preserving C ABI with `_rs` exports.
- Userspace Rust peer/registry/crypto-owner lifecycle model, explicitly **not** a kernel module.
- All existing C test sources linked against Rust library without linking C implementation.
- Original OpenSSL AEAD→RX test executes against Rust post-AEAD path.
- Debian VM test (`rust/src/bin/vm_smoke.rs`) handles identity, post-AEAD sample data, replay, key rotation, and revocation, **not** real WG/AWG handshake.

## Build/test

```
cargo test --offline --manifest-path rust/Cargo.toml --release
CC=gcc bash scripts/run_ffi_diff.sh
CC=clang bash scripts/run_ffi_diff.sh
```

Toolchain dependency Rust 1.89.0 downloaded by GitHub `Pokateli/Temp` runner. No paid storage or GitHub Actions Artifacts required. Source branch includes isolated workflow, VM-Fabric profile for offline Debian testing.

Security boundary: only AEAD-verified payloads and counters from a matching pinned keypair may reach Rust dispatch. No crypto validation occurs inside this library. Current Rust port is not a drop-in production VPN module.
