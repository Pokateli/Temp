# AUPE 0.5.62 trusted RX boundary — mandatory invariants

1. **Crypto first:** WireGuard / AmneziaWG authentically establishes the peer
   public key, successfully decrypts transport with AEAD, and accepts transport
   counter in the anti-replay window, *before* passing bytes to the bridge.
   `aupe_registry_dispatch` does not establish or verify any of these facts.
2. **Peer identity:** the real keypair owner stores the handle; packets never
   choose peers by inner IP / subnet or by untrusted handle network fields.
   Equal 172.16.x.x addresses are valid across distinct peer handles.
3. **Trusted epochs:** assign strictly increasing `u64` epochs locally for
   new confirmed keypair activation, not from UDP, remote timestamps or packet
   counters. Repeated active epoch is a no-op, lower/retired epochs fail closed;
   there is no wrap-around. New peer object starts a new lifecycle.
4. **Process context:** all registry and peer APIs acquire `mutex` or allocate
   memory and may sleep. Move packets out of softirq/NAPI into worker context.
5. **Lifetime:** acquire handle under registry lock and pin peer before access;
   synchronize registry shutdown with ALL current/future callers. Revocation
   removes the handle, closes peer and clears buffers, but already pinned work
   must still release refs. Final free is on last put.
6. **Delivery:** always use `aupe_registry_dispatch` with a synchronous
   callback to prevent revocation/rekey interleaving with delivery. The callback
   runs UNDER peer lock, must not re-enter registry/peer APIs or call code that
   waits for revocation/rotation; must not retain pointers; keep it bounded.
   Callback return zero = accepted delivery, nonzero = delivery failure.
7. **Scratch:** output scratch at least AF_MAX_PACKET (65535) bytes, strictly
   disjoint from input, is owned by the caller. Reassembled plaintext is
   explicitly wiped after callback, success or failure. For AF_BYPASS the
   original input remains owned by the authenticated owner.
8. **Networking:** this candidate does not enforce inner-route ownership,
   AllowedIPs, per-session NAT, actual netns boundaries or egress device
   selection. Correct routing must be built and validated separately.

## Missing protections before deployment

- Real cryptographic WG/AWG code paths and on-wire interoperability.
- Concurrent rekey with live previous and new keypairs, not just serial epoch
  transitions; packet replay boundary under genuine key rotation.
- In-kernel KASAN/lockdep fault-injection and tested teardown under load.
- Multi-session routing with duplicate inner addresses and route isolation.
- Full netfilter/UDP integration and tests on Debian 13, kernel 6.12.113.
