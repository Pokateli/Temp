# AUPE 0.5.62 test matrix

| Case | Status | Notes |
|---|---|---|
| IPv4 fragments and ECN | PASS | Portable C tests |
| Peer dual-flow isolation | PASS | Two local slots / equal inner IPv4 |
| Handle generation and revocation | PASS | Portable stress tests |
| Idempotent same-epoch bind | PASS | No fragment reset in gate/bridge |
| Rejected previous Noise epoch | PASS | Also after unbind |
| Verified-RX dispatch on bypass | PASS | Synchronous callback |
| Verified-RX dispatch on reassembly | PASS | Scratch scrubbed |
| Callback rejection and buffer overlap | PASS | Drop + scratch scrub |
| Clang and GCC + ASan/UBSan | PASS | All portable tests |
| Kernel 6.12.107 compile/link | PASS | `-Werror`, modinfo 0.5.62 |
| Kernel 6.12.113 compile/link | NOT RUN | Headers unavailable |
| Guest insmod and module selftest | NOT RUN | QEMU unavailable |
| WG/AWG E2E / replay / rekey | NOT RUN | Crypto source integration pending |
| Duplicate inner IP real routing | NOT RUN | Dataplane routing pending |

No GitHub Actions or metered storage is necessary to run local tests.
