# Crypto-owner ABI and security contract (0.5.66)

The bridge is an **internal Linux kernel API**, not an external or userspace netlink interface. It is callable only by a trusted WG/AWG cryptographic engine in **sleepable process context**; the current design contains mutexes and cannot be called in NAPI/softirq.

1. **Create** one `aupe_rx_registry` within the AUPE instance/netns and add an `aupe` peer handle. The cryptographic owner must have verified the client's static public key by a successful Noise handshake, must maintain its own unique public-key→peer mapping, and then creates `aupe_crypto_peer_create(registry,handle,confirmed_pubkey,...)`. Do not create/attach from a claimed wire public key or receiver index alone.
2. **Install** a *pinned*, fully authenticated local keypair object with a monotonically increasing owner-local generation: `aupe_crypto_peer_install(owner,confirmed_pubkey,ref,monotonic_ms)`. AUPE assigns epoch 1, 2, ... per attached peer; it does not accept a remote epoch. A duplicate event for the *same* pointer+generation is idempotent. Stale generations, a foreign public key and ambiguous pointer reuse are rejected.
3. **Receive** only after the crypto implementation has selected and pinned that exact decrypting keypair **using its authenticated receive state**, successfully verified the ChaCha20-Poly1305 AEAD tag and supplied the authenticated transport counter: `aupe_crypto_verified_rx(owner,ref,counter,now,plaintext,...,emit,ctx)`. AUPE resolves the corresponding epoch and applies the independent replay window and per-keypair IPv4 reassembly; it does *not* accept an externally supplied epoch in this API.
4. **Rekey** with a new keypair object and strictly increasing generation; previous keypair receives a maximum 180s bounded overlap. Explicitly retire the old keypair before freeing or reusing its memory; old generations are never reactivated. Reusing the same address for a *new* object while it is still active or previous is rejected.
5. **Close**: quiesce all in-flight users, ensure the registry outlives the crypto-owner adapter, call `aupe_crypto_peer_destroy` which wipes bindings and revokes the peer. Destroy the registry only after all its users are quiesced. Do not call a crypto callback that recursively enters AUPE or retains plaintext.

## Boundaries and remaining risks

- The bridge does **not** validate the AEAD tag, Noise handshake, static-key identity, kernel keypair lifetime, or authenticity of the supplied counter: those remain responsibilities of the cryptographic owner.
- `authenticated_public_key[32]` is the *authenticated static key itself*; AUPE currently does not derive its hash in this lab. Public-key uniqueness and peer mapping belong to the crypto owner.
- Owner-managed `generation` is not the transport counter; it must be strictly increasing per peer and never sourced from UDP or client settings.
- The `const void *` identity must be stable and pinned for each call. Object lifetimes and the initial `owner` pointer lookup remain the responsibility of the integrator.
- Epoch and anti-replay state do not transfer between servers or netns; cross-instance keypair binding is not implemented.
- The callback runs under AUPE locks: it may sleep but must not re-enter AUPE. Avoid expensive/blocking networking work in it until the real kernel dataplane integration is designed.
- **No** full WireGuard I/O, handshake, L3 routing, endpoint auto-detection, AWG 1/2/3 compatibility, or Debian 13 VM E2E is proved by this kit.
