# AUPE 0.5.65 (2026-11-01) — keypair fragment isolation

- Fix mixed-generation IPv4 fragment reassembly while both WG/AWG keypairs may
  decrypt packets. Two independent fragment slots per keypair: four per peer.
- Preserve pending old-key fragments across rekey; evict and scrub oldest mux
  on next rotation, explicit retirement, expiry, peer close and revoke.
- `aupe_dispatch_keypair()` now selects its fragment mux by the **trusted local
  keypair epoch**, while the active Noise gate checks the latest generation.
- Extend kernel lab load-time smoke with same IPv4 tuple over two keypairs.
- Add 320,039 assertions for overlapping IP IDs, independent completions,
  replay, retirement, and 40,000 sequential keypair generations.
- Confirm GCC / Clang / ASan+UBSan and kernel module compilation on 6.12.107.

No claims of a real WG/AWG handshake, actual `insmod`, full dataplane or 6.12.113
compatibility are made.
