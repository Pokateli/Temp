# AUPE 0.5.62 — verified-RX callback bridge laboratory (AMD64)

**Development candidate, not a deployable `aupe.ko` VPN implementation.**
This lab extends 0.5.61's per-peer handle registry and two-slot IPv4
reassembly with an atomic dispatch callback and monotonic Noise epoch guard.
It does not provide WireGuard/AmneziaWG Noise, AEAD or anti-replay itself.

## New code

- `src/aupe_dispatch.c` / `include/aupe_dispatch.h`: portable, testable
  post-authentication IPv4 dispatch policy. The owner must call it only after
  real WG/AWG AEAD authentication **and replay-window acceptance**.
- `aupe_registry_bind(registry, handle, epoch)`: secure lifecycle API by pinned
  peer handle. Idempotent same-epoch activation and strict rejection of older
  session epochs (including those already unbound).
- `aupe_registry_dispatch(...)`: handle -> peer pin -> locked delivery callback
  -> peer unpin. Revocation and rekey cannot interrupt the callback; missing
  or revoked handles fail closed. No address/subnet-based peer lookup.
- Reassembled plaintext scratch is wiped immediately after the synchronous
  callback, even on delivery failure. Caller must provide >=65535 bytes of
  non-overlapping output storage. `AF_BYPASS` forwards the original valid IPv4
  bytes directly to the callback; fragments are delivered only on `AF_DONE`.
- Expanded pure-C tests, plus an additional kernel load-time smoke path
  (compiled, but **not** executed in a real VM in this session).

## Reproduce locally (no GitHub Actions)

```sh
make -B test
make -B test CC=clang CFLAGS='-O2 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread'
make -B test CC=gcc CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread -fno-omit-frame-pointer -fsanitize=address,undefined'
make -B kernel KDIR=/lib/modules/6.12.107+deb13-amd64/build
modinfo ./aupe_frag_lab.ko
```

The `.ko` contains only a load-time self-test, no socket, Netfilter hook,
WireGuard interface, and no routing or key management.

## Current limits

- Built against 6.12.107+deb13-amd64 headers, **not yet** 6.12.113.
- VM/FULL E2E not run; QEMU and exact 6.12.113 headers unavailable locally.
- The authenticated WG/AWG implementation from AUPE 0.5.56 is not available
  as source bytes in the current workspace: no real cryptographic integration.
- IPv4 IHL=5 only, <=2 in-flight fragment datagrams per peer, 64 peers per
  laboratory module; inner IP duplicates are permitted at registry level but
  actual routing/demux is **not** implemented.
- One active Noise epoch per peer; overlapping active old/new keypairs and
  real seamless rekey still need architecture and E2E validation.
- Handle and monotonically increasing epoch **are not credentials** and must
  be assigned only inside the real authenticated peer owner.

See `docs/INTEGRATION.md`, `docs/BUILD_REPORT.md` and `docs/SECURITY_CONTRACT.md`.
