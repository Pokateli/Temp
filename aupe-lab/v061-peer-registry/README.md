# AUPE 0.5.61 — Peer Handle Registry (AMD64 laboratory)

This is an **integration candidate**, not the complete `aupe.ko` VPN module.

## Changes

- Owner-scoped, 64-entry kernel peer registry, with no inner-IP/subnet lookup.
- A `u64` handle encodes a slot and monotonically increasing non-wrapping generation.
- Each new kernel registry seeds the initial generation with `get_random_u64()`;
  this reduces accidental identifier reuse across new instances but does **not**
  make handles an authentication token or guarantee global uniqueness.
- A registry mutex pins the peer reference *before* releasing the handle lookup lock.
- Revocation removes the handle, closes the peer, scrubs pending IPv4 fragments,
  then drops the registry owner reference. Already pinned workers remain memory-safe
  and return `AF_CLOSED` on subsequent RX operations.
- The existing Noise epoch gate, two-slot reassembly, bounds/ECN validation,
  and peer counters are unchanged from 0.5.60.

## Build and test

```bash
make test
make kernel KDIR=/lib/modules/6.12.107+deb13-amd64/build
modinfo ./aupe_frag_lab.ko
```

Run `make test` as ordinary user. The `aupe_frag_lab.ko` module is a **link and
load-time self-test** only: loading it never installs a UDP endpoint, netfilter
hook, tunnel interface, or routing policy. The init path tests registration,
revocation, old-handle rejection and the isolation of a second peer. It has been
compiled but **not load-tested** in this environment.

## Security / integration contract

The real owner must perform WG/AWG Noise validation, AEAD decryption, and
anti-replay checking **before** handing decrypted inner IPv4 packets to this RX
component. Neither the handle nor source IP authenticate a peer. The code
may sleep: queue packet processing into process context; never call the registry
or peer API in `softirq`/atomic RX context. `aupe_registry_destroy()` requires
all registry callers to be quiesced; outstanding acquired peer references must
still be released by their holders. Every `aupe_registry_acquire()` requires a
corresponding `aupe_rx_peer_put()`.

Binding/rotating the Noise epoch is **only** for confirmed new keypair/session
lifecycle events. Re-binding on every packet would clear pending fragments.
Handles are local routing IDs and must not be accepted as untrusted API proof.
The IPv4 parser intentionally supports only IHL=5 and discards unsupported
packets. IPv6 and full WG/AWG interoperability are out of scope of this lab.

See `docs/INTEGRATION.md` for call order and constraints, and
`docs/BUILD_REPORT.md` for verified tests and unverified targets.
