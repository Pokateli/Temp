# AUPE 0.5.63 — authenticated transport-counter replay lab (AMD64)

Continuation of 0.5.62. This **lab-only** out-of-tree kernel module is NOT
an operational WireGuard or AmneziaWG endpoint. It has no UDP listener,
Noise handshake, WG/AWG AEAD, routing, tunnel interface, or working device
attachment. Do not enable it as an external ingress path.

## New

- `aupe_replay.c/.h`: per-peer, per-confirmed-keypair counter window (8192
  values), signed rejection statuses, no on-RX memory allocation, explicit
  reset and scrub on teardown and keypair rotation.
- `aupe_dispatch_counter`: takes a counter from an already authenticated
  transport packet and atomically consumes it under the caller's peer lock
  before fragment assembly / IPv4 delivery. Replaying an already accepted
  authenticated transport counter cannot call the emit callback again.
- `aupe_registry_dispatch_counter`: acquires a generation-scoped peer handle
  and drives the locked replay-aware dispatch path. Old
  `aupe_registry_dispatch` remains a legacy lab helper without this additional
  check and MUST NOT be wired directly to a public UDP receiver.
- Counter bits reset only upon a *fresh* confirmed, strictly increasing
  peer epoch. Duplicate bind preserves bitmap and pending fragments; older
  epochs fail closed. Revocation wipes the replay bitmap.
- Differential randomized replay tests with an independent stamped-counter
  oracle, replay after callback error, malformed buffers, two fragment packets,
  key rotation, duplicate inner-IP peers and limit edge cases.

## Commands

```bash
make -B test CC=gcc
make -B test CC=clang
make -B test CC=gcc CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread -fno-omit-frame-pointer -fsanitize=address,undefined'
make -B kernel KDIR=/lib/modules/6.12.107+deb13-amd64/build
modinfo aupe_frag_lab.ko | grep -E 'version:|vermagic:'
```

All three local test runs passed, with each set of GCC assertions totaling
1,559,043. Threaded gate checks can vary slightly between schedulers.

## Important boundaries

The caller **MUST** first authenticate the WireGuard/AmneziaWG peer and
transport counter through the actual Noise keypair and AEAD authentication,
without using the inner IP address to identify clients. Counter and epoch
must originate from trusted local keypair state, never an untrusted UDP
header. `aupe_replay` is additional replay defense; it is not a WireGuard
cryptographic implementation and does not replace WG's own replay window.

For details see `docs/SECURITY_CONTRACT.md`, `docs/INTEGRATION.md`, and
`docs/BUILD_REPORT.md`.

ARM64 is intentionally not covered. No GitHub Actions used.
