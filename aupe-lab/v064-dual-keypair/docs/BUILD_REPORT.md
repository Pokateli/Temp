# AUPE 0.5.64 local build report

Validated on **2026-11-01**, using available host headers for Debian 13
`6.12.107+deb13-amd64` (not the requested target `6.12.113`).

| Suite | Total assertions | Outcome |
|---|---:|---|
| TEST_GCC.log | 2,226,232 | PASS, 0 failures |
| TEST_CLANG.log | 2,226,231 | PASS, 0 failures |
| TEST_SANITIZERS.log | 2,226,232 | PASS, 0 failures |

Suite order: IPv4 fragment safety, two-flow reassembly, epoch gate,
peer handles, authenticated callback, single replay window, dual-keypair bank.
Minor total count variation between runs results from thread scheduling in the
preexisting gate concurrency test, not from skipped test suites.

External kernel module: `aupe_frag_lab.ko` successfully compiled with
`-Werror`, module version `0.5.64`, vermagic
`6.12.107+deb13-amd64 SMP preempt mod_unload modversions`.
The module was **not loaded** and no WG/AWG E2E tunnel was exercised.

Missing: exact 6.12.113 build, QEMU Debian 13 VM boot/load/smoke,
authenticated WG/AWG Noise handshake, encrypted UDP TX/RX, production
routing/netns stress and performance testing.
