# AUPE v0.5.67 C/Rust verified differential comparison

Source: AUPE 0.5.66 C baseline and Rust 0.5.67 reference (five algorithms).
Toolchain: Rust/Cargo 1.89.0; Ubuntu 24.04 x86_64 hosted GitHub runner.
Successful runs:
- https://github.com/Pokateli/Temp/actions/runs/41269323534
- https://github.com/Pokateli/Temp/actions/runs/41269459113

Each run: 600,000 C/Rust differential state transitions with GCC; the same tests with Clang. ABI sizes and alignments matched. Both completed successfully.

## Replay microbenchmark (2,000,000 sequential counters per sample)

| Run | C compiler | C ns/op | Rust ns/op | Rust vs C |
|---|---|---:|---:|---:|
| 41269323534 | GCC | 3.88 | 3.73 | 3.9% faster |
| 41269323534 | Clang | 3.67 | 3.80 | 3.5% slower |
| 41269459113 | GCC | 2.92 | 3.15 | 7.9% slower |
| 41269459113 | Clang | 3.11 | 2.93 | 5.8% faster |

No consistent performance winner in these noisy single-run samples.
C / Rust layout: replay=1048 B, gate=24 B, handles=1024 B,
keypair bank=2160 B, crypto binding=136 B.
Github Actions artifacts in both runs: 0.

**Limitations:** experimental userspace FFI port, not Rust kernel module. No actual WireGuard/AmneziaWG Noise handshake, no real VPN throughput, no concurrency analysis. Rust FFI uses unsafe raw pointers and does not by itself guarantee memory safety. Fragment reassembly, UDP dataplane, Linux registry and TX not yet ported.
