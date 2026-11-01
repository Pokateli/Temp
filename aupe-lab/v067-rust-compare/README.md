# AUPE 0.5.67 — Rust vs C reference comparison

This is an **experimental userspace port**, not a functioning Rust-for-Linux VPN module. The C baseline is copied from AUPE 0.5.66, preserving the original API and algorithm state. The Rust `no_std`-capable reference exposes `extern "C"` entrypoints suffixed `_rs` and C-compatible `#[repr(C)]` state layouts.

## Ported to Rust

- Anti-replay window (8192 transport counters)
- Current and previous keypair replay windows with 180-second overlap
- Noise epoch gate
- Generation-safe handle table (64 slots)
- Public-key / owner-keypair binding and locally assigned epochs

**Not ported:** `aupe_frag`, `aupe_mux`, `aupe_pair_mux`, `aupe_dispatch`, kernel spinlocks, mutexes, Netlink, UDP RX/TX and actual WG/AWG handshake. These need a separate correctness and Rust-for-Linux integration assessment. No crypto implementation is ported (AEAD remains an OpenSSL test in 0.5.66).

## Testing

`bash tools/compare.sh` compiles and tests the C baseline, then compiles Rust and runs differential C/Rust checks if Rust is installed. Set `AUPE_REQUIRE_RUST=1` to refuse a C-only partial result. The integration harness compares return codes, state transitions, sizes and alignments for the five ported components and runs a limited replay microbenchmark. The inputs are deterministic; results are not hardware-independent.

**GitHub Temp transport:** `.github/workflows/aupe-v067-rust-compare.yml` installs a pinned Rust compiler on a GitHub runner and runs C/Rust comparisons, printing measurements to job logs. It does not store or upload Actions Artifacts, use Actions/cache or modify the default branch. Runner minutes and logs may still count toward GitHub usage; use only within your free allocation.

**Important:** Rust FFI is `unsafe` and these tests do not prove freedom from data races, malformed pointers or protocol vulnerabilities. C ABI compatibility MUST be tested for every field before swapping implementations. Rust output is a reference to compare; not yet a drop-in production replacement for a kernel C module.
