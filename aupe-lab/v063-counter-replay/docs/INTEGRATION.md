# AUPE 0.5.63 trusted RX integration contract

This package does not provide the actual WG/AWG transport crypto. The
following *illustrative* code is a contract for the eventual authenticated
owner (not runnable by itself):

```c
/* When a NEW confirmed Noise keypair becomes active, increment the
 * LOCAL per-peer epoch (64-bit, no wrap). Never bind from an UDP field. */
aupe_registry_bind(registry, crypto_peer_handle, local_epoch);

/* Only after the real WG/AWG engine has authenticated (AEAD) the packet,
 * matched it to the correct keypair/public key AND validated the transport
 * counter with its own anti-replay window: */
status = aupe_registry_dispatch_counter(registry,
    crypto_peer_handle, local_epoch, authenticated_transport_counter,
    monotonic_ms, verified_plaintext_ipv4, verified_plaintext_length,
    per_worker_disjoint_scratch, AF_MAX_PACKET, synchronous_emit, peer_ctx);

/* Disconnect: reject future work first; revoke peer and drain workers
 * before destroying the registry. */
aupe_registry_revoke(registry, crypto_peer_handle);
```

Only invoke the process-context API; peer mutexes may sleep. A callback runs
under peer lock and **must not re-enter** registry/peer methods, block waiting
for rotation/revocation, or retain its input. The replay window gets advanced
for each authenticated transport counter even when the IPv4 fragment is bad,
awaiting more fragments or delivery fails. Exactly-once forwarding is NOT
guaranteed (callbacks can fail) but duplicate delivery on the same counter is
rejected. There is no IP/subnet-to-peer binding.

**Do not call** `aupe_registry_dispatch()` in new externally reachable RX
paths: it has no internal replay counter check and is preserved solely for
older laboratory tests. Real WireGuard's cryptographic anti-replay must also
remain enabled even if the new wrapper is used.

Remaining: integrate genuine Noise / AEAD receiver, session counter and
identity mapping, session-specific IPv4/route ownership, netns separation,
UDP demultiplexing, KASAN and VM testing under the target kernel.
