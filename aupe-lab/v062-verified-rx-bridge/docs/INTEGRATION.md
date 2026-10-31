# AUPE 0.5.62 bridge integration specification

Intended call order for real WG/AWG dataplane owner (not implemented here):

```c
/* Process context; not allowed in softirq. Peer is first authenticated by
 * WG/AWG Noise and bound to a real keypair / public key by crypto engine. */
register_handle = aupe_registry_add(registry, GFP_KERNEL, &handle);
/* Local monotonically increasing session epoch; only after keypair confirmed */
result = aupe_registry_bind(registry, handle, local_epoch);
/* On each transport packet: crypto verifies AEAD and replay window FIRST.
 * Never promote unverified UDP payload to 'inner_ipv4'! */
if (aead_success && replay_window_accepted && confirmed_handle) {
    result = aupe_registry_dispatch(registry, handle, local_epoch, now_ms,
        inner_ipv4, plaintext_size, scratch, AF_MAX_PACKET,
        trusted_synchronous_ipv4_delivery, delivery_context);
}
/* On disconnect: stop queueing packets, revoke handle, quiesce workers. */
aupe_registry_revoke(registry, handle);
/* Destroy registry only after owner guarantees no concurrent callers. */
aupe_registry_destroy(registry);
```

Pseudocode above is for integration and **does not implement verification**.

### Return status

- `AF_WAIT`: one or two incomplete datagrams; no delivery.
- `AF_BYPASS`: valid, unfragmented IPv4 delivered via callback.
- `AF_DONE`: fully reconstructed IPv4 delivered via callback.
- `AF_STALE_EPOCH`: packet/callback epoch is not current.
- `AF_CLOSED`: peer revoked or handle unavailable; never deliver.
- `AF_BAD` or other negatives: drop. Never forward on error.

The synchronous callback is called while the peer mutex is held; it **must not
reenter** peer/registry or wait for a concurrent revocation. The callback must
not keep the packet pointer after returning.

### Existing 0.5.61 caveats still apply

64-entry registry and global peer cap are lab-only limits. No actual per-session
routing, peer-specific IPv4 forwarder, IPv6, WG/AWG Noise/AEAD/replay, unified
UDP socket, or protocol auto-detection is supplied by this archive. Use the
actual AUPE 0.5.56 kernel crypto sources to integrate in a separate stage.
