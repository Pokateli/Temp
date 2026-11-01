# Proposed integration with real WG/AWG cryptographic RX

This is **design/contract only**. These functions are not connected to a real
WireGuard or AmneziaWG receiver in 0.5.64.

```c
/* Called once per *cryptographically* confirmed new keypair, by the trusted
 * kernel cryptographic owner. Each keypair gets a strictly increasing local
 * epoch, never reused for the life of this peer. */
aupe_registry_install_keypair(registry, peer_handle, local_epoch, now_ms);

/* In process context after a real AEAD decrypt authenticated this packet.
 * The crypto owner passes the epoch tied to the actual decrypting keypair,
 * and the authenticated WG transport counter. The peer handle itself is not
 * authentication proof. */
aupe_registry_dispatch_keypair(registry, peer_handle, authenticated_epoch,
    authenticated_counter, now_ms, inner_ipv4, packet_len,
    scratch, AF_MAX_PACKET, deliver_ipv4_to_peer, callback_context);

/* After cryptographic owner retires the previous keypair (or on teardown). */
aupe_registry_retire_previous(registry, peer_handle, previous_epoch);
aupe_registry_revoke(registry, peer_handle);
```

The replay bank has *two* independent 8192-counter windows for the current and
previous keypair, and a hard 180-second deadline for the previous window.
`af_mux` remains associated with the current **peer generation**: rekey clears
old incomplete fragment buffers, but packets decryptable with the previous
keypair during the grace period may begin *new* IPv4 reassemblies. Never mix
untrusted packets or peers in one reassembly workspace.

If the real cryptographic owner already enforces WG replay with its own
anti-replay window, this second window is a defensive check, not a replacement;
it must have a compatible lifetime and no code path bypassing either guard.

Protocol discovery/handshake WG, AWG 1/2/3/3.1, multi-session routing and
status/reporting are separate AUPE features, not implemented in this devkit.