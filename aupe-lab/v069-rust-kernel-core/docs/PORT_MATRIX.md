# AUPE 0.5.68 — Rust port status

Source baseline: **AUPE 0.5.66**, which is a laboratory kernel module, not a functioning WireGuard/AmneziaWG server. C 0.5.66 is kept *verbatim* for differential testing.

| C part | Rust 0.5.68 | Validation |
|---|---|---|
| `aupe_frag.c` | `rust/src/fragment.rs` | original `frag_tests.c` through Rust FFI |
| `aupe_mux.c` | `fragment.rs` | original `mux_tests.c` |
| `aupe_pair_mux.c` | `fragment.rs` | original `pair_mux_tests.c` |
| `aupe_gate.c` | `lib.rs` | original `gate_tests.c` |
| `aupe_handles.c` | `lib.rs` | original `handles_tests.c` |
| `aupe_replay.c` | `lib.rs` | original `replay_tests.c` |
| `aupe_keypairs.c` | `lib.rs` | original `keypairs_tests.c` |
| `aupe_crypto_binding.c` | `lib.rs` | original `crypto_binding_tests.c` |
| `aupe_dispatch.c` | `dispatch.rs` | original `dispatch_tests.c` and synthetic AEAD harness |
| `kernel/aupe_peer_rx.c` | `peer_model.rs` | userspace lifetime/locking analogue and VM smoke, **not** kernel replacement |
| `kernel/aupe_registry.c` | `peer_model.rs` | userspace Arc/Mutex/handle registry; not ABI-compatible with kernel C adapter |
| `kernel/aupe_crypto_owner.c` | `peer_model.rs` | userspace post-AEAD binding model; not kernel replacement |
| `kernel/lab.c` | **not ported as `.ko`** | requires target `CONFIG_RUST=y` and Rust-for-Linux build prerequisites |

**Warning**: Production `aupe.ko`, native Rust-for-Linux driver, WG/AWG Noise handshake, UDP socket binding, TX, routing, Netfilter/conntrack/Netlink are *not present* in the C baseline and therefore cannot be honestly called 'ported'. This remains a laboratory dataplane component.

Rust core FFI (`_rs`) uses unsafe raw pointers at the C interface. The userspace peer model owns buffers and serializes access via Arc/Mutex. All no_std algorithms avoid external Cargo dependencies.

Rust compilation on Linux userspace does not establish loadability as a Rust kernel module. Check `CONFIG_RUST` in the exact Debian 13 guest and exact kernel headers; both are required for native Rust `.ko`.
