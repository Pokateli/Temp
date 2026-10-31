# AUPE 0.5.61 — Real dataplane integration boundaries

## Intended owner flow

1. Instantiate one `aupe_rx_registry` per actual AUPE server instance/netns.
   A registry is **not** a cross-namespace global; enforce owner isolation.
2. Create peer using `aupe_registry_add()` (receives `u64 handle`). Store the
   handle in the cryptographically authenticated peer object, not in an IP map.
3. After *successful* Noise keypair derivation and replay-window activation,
   `p = aupe_registry_acquire(registry, handle)` in process context; derive
   a trusted monotonically unique `epoch` **from the confirmed local session**;
   call `aupe_rx_peer_bind(p, epoch)` once on session activation/rotation,
   then `aupe_rx_peer_put(p)`.
4. On decrypted packet delivery, **only after AEAD success and anti-replay**,
   pin the same peer with `aupe_registry_acquire()`, then
   `aupe_rx_peer_ingest(p, trusted_epoch, now_ms, plaintext_ipv4, n, out,
   capacity, &rebuilt)`; finally release with `aupe_rx_peer_put(p)`.
   - `AF_WAIT`: do not forward an incomplete datagram.
   - `AF_BYPASS`: forward validated original unfragmented packet.
   - `AF_DONE`: forward `out[0..rebuilt)` reassembled IPv4 datagram.
   - all negative codes: drop, count, and never forward.
5. After revocation, call `aupe_registry_revoke()` from process context. It
   removes the handle first, closes the peer, scrubs plaintext, and releases the
   registry owner ref. In-flight holders must still call `put`.
6. Stop all packet workers and registry callers before `aupe_registry_destroy`;
   destroy must not race calls using the registry pointer.

## Threading and scope

Registry lock -> peer lock order; no inverse calls from peer API to registry.
All functions may sleep; no direct softirq usage. The authoritative user key
is the authenticated WG/AWG peer public key, not any inner IPv4 address.
Duplicate inner client addresses are supported **at the registry level**, but
actual per-session routing/demux is not implemented in this laboratory module.
A 64-bit handle is not a capability or protocol authentication proof. Input
`trusted_epoch` must not come from network-controlled unauthenticated values.

## Not yet implemented / verified

- Actual `wg`/`awg` packet ingress and handshake integration.
- WG/AWG AEAD and replay verification in the complete kernel dataplane.
- Multiple protocol demux over a single public UDP socket.
- Full per-session route enforcement, AllowedIPs, netns teardown integration.
- VM boot/load/E2E tests for Debian 13 kernel `6.12.113+deb13-amd64`.
