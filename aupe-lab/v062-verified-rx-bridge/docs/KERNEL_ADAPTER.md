# AUPE 0.5.62 kernel RX adapter

This lab module embeds a 64-peer handle registry, Noise epoch gate, two-slot
IPv4 defragmenter and synchronous verified-RX callback bridge. It has no
network listener and performs no cryptography, packet decryption or routing.

The owner's preferred path after verified WG/AWG AEAD and anti-replay is
`aupe_registry_dispatch(registry, handle, trusted_epoch, now, ipv4, len,
 scratch, AF_MAX_PACKET, callback, ctx)`.

The registry pins peer under registry mutex, then takes the per-peer mutex for
verification of the *locally confirmed epoch*, fragment processing and
synchronous delivery. This prevents revocation and rekey from interleaving
with the callback but makes callback re-entry into registry/peer a deadlock
hazard. Callback MUST NOT re-enter; use a bounded synchronous forwarder.

The scratch buffer is wiped on reassembled delivery. If callback returns
nonzero, dispatch returns AF_BAD. BYPASS forwards the original validated
unfragmented IPv4, caller retains ownership of original plaintext.

Noise epochs are local monotonic counters, not external timestamps. Duplicate
active epoch notifications return success without clearing the fragment
slots; older epochs are permanently rejected for that peer lifetime.

`aupe_rx_peer_ingest()` and `aupe_registry_acquire()` are retained for lab
compatibility, but callers that separate RX classification and output delivery
must implement their own teardown synchronization. Prefer the atomic dispatch
path to avoid that race.

No direct `softirq`/NAPI invocation: these operations may sleep. Registry
teardown requires owners to quiesce all outstanding registry calls. All
cryptographic authentication and transport anti-replay remain OUTSIDE lab.
