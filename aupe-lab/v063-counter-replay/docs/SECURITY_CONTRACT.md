# AUPE 0.5.63 — authenticated transport boundary

## Trust assumptions

1. `aupe_registry_dispatch_counter` MUST only be invoked by a **trusted**
   WireGuard/AmneziaWG transport handler after AEAD success, keypair-to-peer
   public-key binding, and stock WireGuard anti-replay verification. The
   counter is authenticated *input*, not proof of authentication.
2. Locally controlled, monotonically increasing epochs define confirmed
   keypairs, with no rollback. A new epoch clears replay history and buffered
   IPv4 fragments under the same peer mutex. Duplicate bind is a no-op.
3. Each peer handle is registry-generated; matching cannot be based on the
   decrypted inner IP address. Reused inner addresses are permitted.
4. Replay window is an **extra** serialized 8192-counter ring bitmap.
   Duplicates, counters outside the window, stale epochs and near-wrap values
   fail closed. Each newly authenticated keypair starts a new independent
   counter window. It cannot be shared by multiple concurrently active
   keypairs and cannot replace stock WG's concurrent keypair replay windows.
5. Delivery callback executes under peer mutex and must be fast, synchronous,
   non-reentrant, and must not retain plaintext buffers. Wipe the scratch
   buffer after reassembly. Any failure consumes the authenticated counter.
6. No softirq/NAPI invocation: mutexes and allocation can sleep. Per-peer
   lifetime pin is required for each in-flight deferred RX. Registration and
   destruction of the registry require external owner synchronization.
7. No unauthenticated packet can directly choose handle, epoch, counter,
   routing table or callback. This lab module does not implement public UDP,
   cryptography, AllowedIPs, TUN or egress routing.

## Current non-production limitations

- Multi-keypair overlapping rekeys (previous/current/next) require separate
  windows and careful retirement, not a single instant global epoch switch.
- No genuine WG/AWG AEAD or replay ownership wired in.
- No live VM/physical kernel load test (KASAN/lockdep), no target 6.12.113.
- No actual session-specific route policy for duplicate inner IPv4 addresses.
