# AUPE 0.5.63 test matrix

| Scenario | Outcome |
|---|---|
| Pure anti-replay duplicate, old, wrap-limit | PASS in userspace |
| Independent peer replay windows with identical inner IP | PASS in userspace |
| 180k deterministic differential replay sequences | PASS in userspace |
| Replay before and after callback failure | PASS in userspace |
| Partial fragment followed by replayed fragment | PASS in userspace |
| Rebind new local epoch and reject previous epoch | PASS in userspace |
| Prior IPv4 and handle registry regressions | PASS in userspace |
| GCC, Clang, ASan/UBSan | PASS |
| Kernel 6.12.107 module compile (`-Werror`) | PASS |
| Kernel 6.12.113 module compile | NOT RUN |
| Module insmod / unload / KASAN / lockdep | NOT RUN |
| Live WG/AWG Noise+AEAD+anti-replay interoperability | NOT RUN |
| Concurrent old/new keypair rekeys | NOT RUN |
| VPN route isolation, duplicate inner IP at live netns | NOT RUN |
| ARM64 | NOT RUN (on hold) |
