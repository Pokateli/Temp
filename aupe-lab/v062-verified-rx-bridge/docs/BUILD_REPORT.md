# AUPE 0.5.62 reproducibility report

Target: AMD64 Debian 13 headers `/lib/modules/6.12.107+deb13-amd64/build`.
Kernel module filename: `aupe_frag_lab.ko`, version `0.5.62`.

The following tests were run in the local container (not GitHub Actions):

| Suite | GCC -O2 | Clang -O2 | GCC ASan+UBSan |
|---|---:|---:|---:|
| IPv4 fragmentation | 435898 | 435898 | 435898 |
| Dual slot mux | 100128 | 100128 | 100128 |
| Noise gate / threads | 59459 | 59458 | 59459 |
| Peer handle registry | 680313 | 680313 | 680313 |
| Verified dispatch bridge | 102119 | 102119 | 102119 |
| Failures | 0 | 0 | 0 |

Gate test count can vary slightly because its threaded worker scheduling
alters the number of rotation assertions; all test outcomes must remain 0
failures. New tests cover duplicate Noise binds, rollback after unbind,
session separation, callback delivery rejection, buffer overlap and wipe.

`make -B kernel KDIR=/lib/modules/6.12.107+deb13-amd64/build`: PASS,
`-Werror` with modinfo `vermagic=6.12.107+deb13-amd64`.
`aupe_registry_bind`, `aupe_registry_dispatch` and
`aupe_dispatch_verified` are in the linked module.

**Not verified:** insmod/module init execution, Debian 13 VM, full WG/AWG
cryptographic interoperability, rekey with overlapping old/new keypairs,
actual routing, 6.12.113 build, ARM64, fuzzing beyond recorded tests.
