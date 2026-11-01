# 0.5.65 integration notes (not yet connected to production cryptography)

The RX owner already invokes the registry under the peer lock:

```c
/* In trusted process-context worker, after AEAD authentication, replay check
 * and cryptographic keypair lifetime pinning: */
aupe_registry_dispatch_keypair(registry, confirmed_peer_handle,
    decrypting_keypair_local_epoch, authenticated_transport_counter,
    monotonic_ms, decrypted_inner_ipv4, packet_len,
    disjoint_scratch, AF_MAX_PACKET, deliver_ipv4, context);
```

On completed keypair handshake: `aupe_registry_install_keypair(registry,
handle, new_local_epoch, monotonic_ms)`; on keypair retirement:
`aupe_registry_retire_previous(registry, handle, old_local_epoch)`.

The current `aupe_pair_mux` switches between two two-slot workspaces. The
previous mux keeps pending fragments of the old epoch. After expiry, the bank
marks the previous epoch invalid and kernel RX scrubs the previous mux. The
migration is synchronized with the per-peer `mutex`.

**No real connection to the WG/AWG cryptographic owner is present.** In
particular, no mapping from authentic keypair pointer to the local epoch exists
in this devkit. Do not expose the raw epoch/counter API to untrusted userspace.
Network stack interactions, TX framing, routing, duplicate inner IP support
end-to-end, protocol detection and Netlink control need separate verification.
