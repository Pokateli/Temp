# AUPE 0.5.63 local qualification (2026-11-01)

Environment: isolated local container, GCC, Clang, Debian 13 AMD64 kernel
headers `6.12.107+deb13-amd64`, no GitHub Actions or paid storage.

| Suite | GCC -O2 | Clang -O2 | GCC ASan+UBSan |
|---|---:|---:|---:|
| IPv4 frag safety | 435898 | 435898 | 435898 |
| Dual-slot mux | 100128 | 100128 | 100128 |
| Epoch lifecycle gate | 59459 | 59458 | 59459 |
| Generation-scoped handles | 680313 | 680313 | 680313 |
| Verified RX dispatch | 102119 | 102119 | 102119 |
| New authenticated-counter replay | 181126 | 181126 | 181126 |
| Failed assertions | 0 | 0 | 0 |

Totals: GCC 1,559,043; Clang 1,559,042; ASan/UBSan 1,559,043.
Gate assertion counts are nondeterministic because background rotation races
with test worker threads; zero failures is the actual acceptance criterion.

Kernel build: `make -B kernel KDIR=/lib/modules/6.12.107+deb13-amd64/build`
PASS (`-Werror`). `modinfo` reports version 0.5.63 and vermagic
`6.12.107+deb13-amd64` (SMP preempt mod_unload modversions).
Module is `aupe_frag_lab.ko`: a **lab smoke only**, not operational VPN.

Not tested: module load/insmod, QEMU or physical Debian 13 boot, actual
crypto handshake/AEAD, real WireGuard replay integration, IPv6, VPN routing,
peer-specific routing with duplicate inner addresses, kernel 6.12.113,
KASAN/lockdep, multi-hour soak, ARM64. Missing VM/QEMU and exact 6.12.113
headers prevent claims of live compatibility in this session.
