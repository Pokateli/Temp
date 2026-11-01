# AUPE v0.5.70 — Linux owner bridge / Rust core

**Stage:** genuine Linux C kernel-object compilation, genuine userspace C→Rust staticlib ABI tests; **not** a loadable `aupe.ko`, not a deployed VPN. All VM operation is LOCAL to the ChatGPT sandbox. GitHub Actions is permitted only for Rust compilation and temporary source/artifact transfer; never starts a VM.

## What is new

- `kernel-bridge/aupe_rs_linux_owner.c`: Linux-specific C lifecycle adapter for `no_std` AUPE Rust kernel core; uses `mutex`, `refcount_t`, quota, `kvzalloc`, `kvfree_sensitive` and owner final revocation, no memory allocation during RX. It is intentionally compiled as a standalone object because the stock Debian 6.12.107 kernel build has no `CONFIG_RUST` support.
- `include/aupe_rs_linux_owner.h`: trusted process-context-only kernel API; caller must pin the already-verified Noise/AEAD keypair, and serialize initial get/unpublish under a registry lock.
- `include/aupe_rs_core.h`: portable C API corrected to include Linux `<linux/stddef.h>` under Kbuild.
- Rust ABI introspection for size and alignment of the pinned crypto reference, exact ABI cross-check with original C structures.
- `tests/bridge_abi_tests.c`: real C binary linked with Rust staticlib that checks independent peer sessions with identical IPv4, replay, rekey, previous-key retirement, 4,000 transport counters and close. This is *after*-AEAD validation only; AEAD is not performed by this test.
- `scripts/kernel-object-check.sh`: compile C bridge with local Kbuild `6.12.107` under `-Werror` (object only), print unresolved Rust symbols to avoid pretending the module can be loaded.
- `scripts/test-abi.sh`: GCC/Clang C/Rust interoperability tests.

## Host-side commands

```bash
# Host with Debian 6.12.107 header package:
bash scripts/kernel-object-check.sh
# Host with Rust 1.89: compile staticlib (no VM):
cd rust && cargo build --offline --locked --release --lib && cd ..
bash scripts/test-abi.sh rust/target/release/libaupe_rust_core.a
```

**Security boundaries:** The `verified_rx` API does not perform Noise/AEAD, input pointers and keypair generation must come from trusted crypto owner; user-supplied IP, handle, or epoch is not proof. The adapter must be published under external owner synchronization. A true production Rust .ko needs exact CONFIG_RUST kernel/toolchain, Rust-for-Linux allocation/sync support, authenticated WG/AWG handshake, UDP socket TX/RX, per-netns registry and routing.
