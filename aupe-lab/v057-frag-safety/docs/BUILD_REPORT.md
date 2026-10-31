# AUPE 0.5.57 lab — reproducible local verification

Executed locally on 2026-11-01 with no GitHub Actions runs.

| Verification | Result |
|---|---|
| GCC `-O2 -std=c11 -Wall -Wextra -Werror -pedantic` | PASS |
| Deterministic normal, malformed, expiration, overlap, epoch, large-packet and generated-case checks | 144389 checks / 0 failures |
| GCC AddressSanitizer + UndefinedBehaviorSanitizer | PASS (144389 / 0) |
| Debian 13 kernel headers `6.12.107+deb13-amd64`, `-Werror` | PASS, link-only `aupe_frag_lab.ko` |
| Kernel module loaded in VM | NOT RUN |
| Exact kernel `6.12.113+deb13-amd64` | NOT RUN |
| Actual AUPE 0.5.56 source modified or retested | NO (bytes unavailable) |
| WireGuard/AWG full E2E, VPN DATA, multi-peer stress | NOT RUN |
| ARM64 | NOT ATTEMPTED |

Kernel module is only a link test; no hooks, listen socket, route or UAPI.
Therefore this report is not evidence of production readiness or a complete
0.5.57 release.
