# AUPE 0.5.66 — reproduced local build/test evidence

- Source: actual 0.5.65 DevKit archive with source additions; architecture: amd64.
- GCC: **3,146,301 checks**, 9 suites, 0 failures (`TEST_GCC.log`).
- Clang: **3,146,301 checks**, 9 suites, 0 failures (`TEST_CLANG.log`).
- GCC ASan+UBSan: seven legacy suites fully completed (**2,226,233 checks**); full suite timed out during pair-mux stress; new `crypto_binding_tests` was then explicitly run under ASan+UBSan (600,029 checks, 0 failures, `TEST_BINDING_ASAN.log`). Do not classify the incomplete full sanitizer run as all-PASS.
- Optional OpenSSL ChaCha20-Poly1305 synthetic post-AEAD→RX integration: **30 checks, 0 failures** (`TEST_AEAD.log`). Not a Noise/WG handshake test.
- Kernel build with `-Werror`: **PASS** using exact available `/lib/modules/6.12.107+deb13-amd64/build` (see `KERNEL_BUILD_LOG.txt`). `modinfo`: version 0.5.66, vermagic 6.12.107+deb13-amd64.
- Kernel smoke *compiled* but **not executed** via insmod; guest QEMU and exact 6.12.113 headers not available in the runtime.
- GitHub Actions: not run by this local test. No paid or metered Actions Artifacts required.

## Missing qualification

A real kernel WG/AWG handshake and transport engine, real AEAD owner integration and keypair lifetime under RX load, guest Debian 13 boot/module load, wire protocol interoperability, IPv4 TX, routes, Netfilter/NAT and continuous stress testing are not performed. This is still `aupe_frag_lab.ko`, not the target `aupe.ko`.
