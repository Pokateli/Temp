# AUPE 0.5.61 build and test report

Environment: isolated AMD64 build container with Debian 13 kernel headers
`6.12.107+deb13-amd64`, GCC, Clang, GCC AddressSanitizer/UBSan.

Results:

| Suite | GCC | Clang | ASan+UBSan |
|---|---:|---:|---:|
| IPv4 fragment | 435898 pass | 435898 pass | 435898 pass |
| two-slot mux | 100128 pass | 100128 pass | 100128 pass |
| Noise epoch gate (thread scheduling-sensitive) | 59451 pass | 59452 pass | 59452 pass |
| peer handle registry | 680313 pass | 680313 pass | 680313 pass |

All reported tests: zero failures. Kernel `.ko` compile and link passed with
`-Werror` and `vermagic=6.12.107+deb13-amd64`; module exports expected
`aupe_registry_*` symbols. `MODULE_VERSION=0.5.61`.

**Not verified:** successful `insmod`, kernel init smoke execution, network
traffic E2E, actual WG/AWG interoperability, or compilation with exact 6.12.113
headers. No QEMU executable or exact 6.12.113 headers was available locally;
the GitHub Release binary could not be retrieved owing to DNS/network access.
No new GitHub Actions run or paid storage used.
