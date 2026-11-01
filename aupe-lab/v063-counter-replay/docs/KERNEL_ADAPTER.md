# Kernel adapter 0.5.63

`aupe_registry_dispatch_counter` => `aupe_registry_acquire` (registry mutex,
refcount pin) => `aupe_rx_peer_dispatch_counter` (peer mutex) =>
`aupe_dispatch_counter` (gate + replay bitmap + fragment mux + synchronous
emit) => `aupe_rx_peer_put`.

The replay counter MUST be captured from the authenticated WG/AWG keypair,
not a raw packet pointer or UDP header. Epoch only changes on verified
keypair activation. `aupe_registry_bind` rejects rollback and preserves
fragment/replay state on a duplicate bind event. `aupe_registry_revoke`
unpublishes the handle, closes the peer under lock, scrubs the bitmap and
plaintext fragments, drops the owner ref. Pinned workers subsequently put.

Module load-time smoke verifies duplicate counter rejection (not real RX).
`aupe_frag_lab.ko` does not install a UDP receive hook or a TUN interface.
