# Build and verification — v0.5.59

Date: 2026-11-01
Host: x86_64 userland; active kernel is 6.18.57 (not target)

- gcc -O2 -Wall -Wextra -Werror -pedantic: regression 435,898 PASS;
  mux 100,128 PASS.
- clang -O2 -Wall -Wextra -Werror -pedantic: same 536,026 PASS.
- gcc -fsanitize=address,undefined -fno-omit-frame-pointer:
  same 536,026 PASS, no sanitizer reports.
- make -C /usr/src/linux-headers-6.12.107+deb13-amd64 M=... modules:
  PASS, module metadata version 0.5.59, vermagic 6.12.107+deb13-amd64.
- Linux 6.12.63 headers found but incomplete (no `modules` make target);
  attempted build STOPPED with no rule to make target `modules`.
- Linux 6.12.113 headers not present locally; no target-kernel compilation.
- qemu-system-x86_64 unavailable; no VM boot, module load or WG/AWG E2E test.
- No GitHub Actions workflow launched and no new Actions artifact storage used.

A kernel module build is a *link qualification only*; requires load and live
packet/crypto tests in the specified 6.12.113 Debian 13 environment.