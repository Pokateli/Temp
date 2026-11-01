# Test matrix, 0.5.64

| Scope | Status |
|---|---|
| IPv4 header, fragments, overlap, CE | PASS (native) |
| Dual-slot fragments, duplicate inner IPv4 | PASS (native) |
| Monotonic gate & close under concurrency | PASS (native) |
| Handle generation / stale handle | PASS (native) |
| RX callback / scratch / counter consumption | PASS (native) |
| Single replay 8192-window / fuzz oracle | PASS (native) |
| Dual keypair 2 windows / rotation / expiry / retired key | PASS (native) |
| GCC/Clang -Werror, ASan/UBSan | PASS (native) |
| Debian 13 headers 6.12.107 module compilation | PASS (compile only) |
| Debian 13 headers 6.12.113 module compilation | NOT RUN |
| Module insmod, kernel smoke execution | NOT RUN |
| Real WG/AWG cryptographic handshake, TX/RX E2E | NOT IMPLEMENTED |
| Long-running network/netns/rekey soak | NOT RUN |
