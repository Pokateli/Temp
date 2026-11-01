# AUPE 0.5.65 — keypair-scoped IPv4 fragmentation laboratory

**Status: laboratory-only. This is not a production VPN server.** This module has no
UDP listener, WG/AWG Noise/AEAD handshake or production routing dataplane.
It is **not** a confirmed build for Linux `6.12.113+deb13-amd64`.

## 0.5.65 change and vulnerability addressed

`0.5.64` separated authenticated replay windows by keypair, but passed IPv4
fragments for current and previous keypair into the *same* two-slot mux. An
identical inner IPv4 `(src,dst,protocol,identification)` on the two authenticated
keypairs could combine fragments across keypair generations.

This version allocates four reassembly workspaces outside RX (two per keypair),
retains the previous keypair's two workspaces during its limited grace period,
and scrubs the older previous mux on subsequent rotation or explicit retirement.
The same inner IP and IPv4 ID may appear on both keys without mixing.
The previous keypair still uses its own authenticated transport-counter replay
window; there is no allocation in packet processing.

New files: `include/aupe_pair_mux.h`, `src/aupe_pair_mux.c`,
`tests/pair_mux_tests.c`. Changed: dispatch interface, peer RX integration,
kernel load-time smoke and build rules. All legacy RX entrypoints remain locked
once keypair mode is entered.

## Test and build

```sh
make -B test CC=gcc
make -B test CC=clang
make -B test CC=gcc CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread -fno-omit-frame-pointer -fsanitize=address,undefined'
make kernel KDIR=/lib/modules/6.12.107+deb13-amd64/build
```

The smoke in `kernel/lab.c` is compiled but **has not been executed in a VM**.

## Crypto-owner boundary

The interface still requires the *actual* cryptographic owner to authenticate
the WG/AWG peer, pin the decrypting keypair, check AEAD, choose the **trusted
local epoch** associated with that keypair, and supply the authenticated counter.
A user-controlled header, UDP index, tunnel IP or untrusted local handle must
**never** select an AUPE epoch. The kernel bridge remains process-context only,
using mutexes; callbacks must not recursively call AUPE or retain plaintext.

See `docs/SECURITY_CONTRACT.md`, `docs/INTEGRATION.md` and `docs/BUILD_REPORT.md`.
