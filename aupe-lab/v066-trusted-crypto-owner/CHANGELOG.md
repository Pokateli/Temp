# AUPE 0.5.66 — Trusted Crypto Owner Bridge (laboratory)

- Added kernel-only `aupe_crypto_peer_{create,install,retire,destroy}` and `aupe_crypto_verified_rx` bridge. The actual decrypting keypair object's pinned address plus owner-managed monotonically increasing generation selects the authenticated local keypair; untrusted wire packet fields never choose epoch.
- Added portable keypair->epoch binder; AUPE assigns its own monotonic epoch after successful Noise-owned binding, checks 32-byte confirmed static public key in constant-time, fences stale/aliased/retired generations, and preserves the previous keypair for a bounded overlap.
- Binds the local epoch to 0.5.65's existing per-keypair replay windows and per-keypair IPv4 fragment mux. Wrong-key, ABA pointer alias, epoch rollback, timeout, close, foreign peer key, and rekey behaviors are tested.
- Added synthetic ChaCha20-Poly1305 post-AEAD-to-RX test with OpenSSL, including modified-tag and wrong-key negative cases. Does not implement WireGuard Noise or frame parsing.
- Kernel module build (6.12.107+deb13-amd64) and compile-only load-time owner bridge smoke added. Real module loading on VM and E2E WG/AWG handshake NOT verified.

No changes to WG/AWG protocol wire format, TX, listener, routing or cryptographic primitives.
