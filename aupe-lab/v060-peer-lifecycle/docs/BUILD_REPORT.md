# Build and test report — AUPE 0.5.60

Date: 2026-11-01. Host: Linux 6.18.57, x86_64. Not a Debian VM guest.

- GCC, `-O2 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread`:
  435,898 IPv4 tests + 100,128 mux tests + 59,451 gate/concurrency checks;
  all passed in latest run.
- Clang with equivalent flags: same three test suites, all passed.
- GCC ASan+UBSan, `-O1 -g -fsanitize=address,undefined`:
  same suites passed, no reports in latest run.
- Linux module build against `6.12.107+deb13-amd64` headers:
  PASS; `aupe_frag_lab.ko` links peer adapter and gate, no data-plane hooks.
- Kernel `6.12.113+deb13-amd64`: headers not present, NOT tested.
- QEMU: no `qemu-system-x86_64` executable, VM not started.
- Real WG/AWG crypto, replay, packet routing: NOT tested.
- KASAN, KCSAN, module load/unload: NOT tested.
- No Actions runs or additional artifact storage required by this build.

The threaded userspace test exercises the **portable** gate contract with
external mutex serialization, NOT Linux `refcount_t` runtime behavior.
