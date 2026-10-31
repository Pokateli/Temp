# AUPE 0.5.60 kernel RX adapter

## Lifetime protocol

```
(owner external synchronization)
peer = aupe_rx_peer_create(GFP_KERNEL)   // returns refcount=1
publish(peer)
verified Noise session -> aupe_rx_peer_bind(peer, new_epoch)

enqueue work:
  lock owner publication/list mutex
  p = aupe_rx_peer_get(peer)
  if (!IS_ERR(p)): queue_work(p)
  unlock owner publication/list mutex

worker (process context):
  authenticate/decrypt and accept replay counter outside this adapter
  aupe_rx_peer_ingest(p, trusted_epoch, monotonic_ms, plaintext, ...)
  aupe_rx_peer_put(p)

teardown:
  lock owner publication/list mutex
  unpublish(peer)                 // no new lookups/get
  unlock owner publication/list mutex
  aupe_rx_peer_close(peer)       // terminal, scrubs both slots
  aupe_rx_peer_put(peer)         // relinquish owner's original reference
```

**Important:** `aupe_rx_peer_get()` cannot resurrect an already freed
pointer. The caller MUST protect the initial lookup before it calls `get()`.
`close()` and `put()` MUST run in sleepable process context; do not call
from softirq/NAPI/RCU-only context. `aupe_rx_peer_destroy` is for legacy
single-owner code and MUST NOT be called twice.

## Epoch semantics

The owner constructs a unique nonzero epoch per promoted Noise session;
never use an untrusted packet field, IPv4 address or process timestamp as
proof of an authenticated peer. Binding a new epoch resets both reassembly
slots. Each `feed`, `take`, `ingest`, and `expire` checks the trusted epoch
while holding the peer mutex. After close the state remains CLOSED forever.

`aupe_rx_peer_snapshot()` reads aggregate counters under the same lock.
The stats never hold packet addresses or cryptographic secrets. Stats
count feed/ingest outcomes; `rx_reassembled` counts completed reassemblies,
not proof of routing or delivery by the larger VPN stack.

## Current limitations

- No native WG/AWG ingress exists in the lab. Do not bypass authenticated
  transport and anti-replay checks to feed the fragment collector.
- The adapter does not integrate with a real AUPE peer registry, netns,
  credentials, route lookup, skb, timer, or Netlink UAPI.
- The per-module 64-object quota is not netns-aware and quotas do not
  extend to queued plaintext outside the adapter. Add queue limits.
- Not safe to use in atomic context; fragmentation buffers scrub under
  mutex. The owner must avoid starving RX queues during scrubbing.
- No kernel-runtime/KASAN/KCSAN/lockdep test; a successful `.ko` build only
  proves a compatible link against the provided headers.
