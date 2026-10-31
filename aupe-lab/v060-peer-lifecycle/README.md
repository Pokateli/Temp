# AUPE v0.5.60 — peer lifecycle + epoch-fenced fragment RX (AMD64 laboratory)

**NOT a complete VPN implementation and NOT a drop-in replacement for `aupe.ko`.**
This directory contains an integration candidate for the already-decrypted
IPv4 traffic of a cryptographically authenticated WG/AWG peer. The actual
AUPE v0.5.56 sources are not present in this checkout.

## Changes from v0.5.59

- Added `aupe_gate` (portable C): explicit unbound/active/terminal-closed
  states, trusted Noise epoch enforcement, and fail-closed rejection after
  close. Gate operations are serialized by their owner.
- Added reference-counted kernel `aupe_rx_peer_get/put`, terminal
  `aupe_rx_peer_close`, and retained `destroy` compatibility for the owner.
  A pinned worker may safely finish/return closed after the owner revokes the
  session; outstanding tasks must never dereference an unpinned peer.
- Added per-peer, lock-protected counters (`aupe_rx_peer_snapshot`) without
  inner IPs, keys, endpoint addresses, or plaintext contents.
- Preserved two concurrent IPv4 reassembly slots per confirmed peer, bounded
  at 64 peer adapters across this standalone lab module; no RX allocations.
- Added deterministic gate + mux integration checks and a POSIX-threaded
  concurrency scenario with owner serialization.

## Trust and lifetime contract

1. Owner locates a `struct aupe_rx_peer*` under **external synchronization**.
   No `get()` is valid if another task could already have completed the
   final `put()` and freed that pointer.
2. Owner acquires a reference (`aupe_rx_peer_get`) **before** enqueueing
   deferred work, then worker calls `aupe_rx_peer_put` when done. All adapter
   functions, including `get`/`put`, are process-context (MAY SLEEP).
3. Only a verified handshake's cryptographic peer session may call
   `aupe_rx_peer_bind(peer, trusted_epoch)`. Each distinct promoted session
   must receive a distinct, nonzero epoch value from the controller.
4. Transport AEAD and anti-replay must already have succeeded before
   calling `aupe_rx_peer_ingest`. This lab does **not** prove authenticity.
5. On deletion, the owner atomically unpublishes its pointer (preventing
   fresh `get()`), calls `aupe_rx_peer_close`, and releases its original
   reference with `aupe_rx_peer_put`. Pinned tasks subsequently return
   `AF_CLOSED`. `aupe_rx_peer_destroy` is shorthand for close + owner put
   and is **not** safe to call twice.
6. On regular session rotation, call `aupe_rx_peer_bind` under owner control.
   Pending fragments from the old epoch are wiped before reassembly resumes;
   stale queued packets are rejected. Unbind is not terminal; close is.
7. Do not derive peer identity from inner IPv4 src/dst, subnet, IP ID or UDP
   endpoint. Routing after successful reconstruction must use the authenticated
   outer peer identity; duplicate 172.16/12 addresses are supported.

## Build and tests

```sh
make test
make kernel KDIR=/lib/modules/6.12.107+deb13-amd64/build
modinfo aupe_frag_lab.ko
```

The lab `.ko` only links and exports no UDP, Netfilter, Netlink, tunnel,
manager, crypto, or routing functionality. It does not install a live hook.
The module targets available `6.12.107+deb13-amd64` headers. The full user
intended target is `6.12.113+deb13-amd64`, **not verified here**.

See `docs/BUILD_REPORT.md`, `docs/INTEGRATION.md`,
`docs/KERNEL_ADAPTER.md` and `docs/GITHUB_AND_VM.md`.
