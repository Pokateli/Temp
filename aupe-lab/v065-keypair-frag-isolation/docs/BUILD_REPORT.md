# AUPE 0.5.65 local verification — 2026-11-01

| Test suite | GCC checks | Status |
|---|---:|---|
| IPv4 fragment safety | 435,898 | PASS |
| Two-flow mux | 100,128 | PASS |
| Noise epoch gate | 59,459 | PASS |
| Generation-safe handles | 680,313 | PASS |
| Verified callback bridge | 102,119 | PASS |
| Transport replay | 181,126 | PASS |
| Dual keypair bank | 667,190 | PASS |
| **Keypair fragment isolation (new)** | **320,039** | **PASS** |
| **TOTAL GCC** | **2,546,272** | **PASS** |

Clang: same suites PASS; GCC with ASan/UBSan: same suites PASS.
Existing gate thread scheduling makes assertion totals vary by one occasionally.
The new 320,039-assertion suite passed identically in all three runs.
Kernel module compiled with `-Werror` for `6.12.107+deb13-amd64`:
`MODULE_VERSION=0.5.65` and correct vermagic.

Not performed: `insmod`/`rmmod` in a guest, real WG/AWG Noise/AEAD, transport
TX/RX, kernel `6.12.113` compile, performance/soak. QEMU not installed locally;
network DNS resolution for the archived exact-header package failed. No
GitHub Actions workflow or paid storage was used for these tests.
