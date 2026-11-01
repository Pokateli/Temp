# 0.5.66 integration steps

- Replace the previous `aupe_registry_install_keypair` and `aupe_registry_dispatch_keypair` *external usage* with the kernel-only `aupe_crypto_peer_*` adapter. Existing low-level APIs remain available internally for testing.
- The real WG/AWG crypto owner must pin the keypair and prove the authenticated static key via a successful Noise handshake; attach the AUPE peer to that key, then call owner install.
- After a **successful AEAD**, pass the owner keypair object/monotonic keypair-generation and the authenticated transport counter to `aupe_crypto_verified_rx` in a sleepable worker context. No wire/client-selected epoch.
- Preserve lifecycle: retire old keypair before free; terminal owner destroy before registry destroy; do not route by client tunnel IP. Client IPs may collide by design.
- Load module on a matched Debian 13 guest and execute real WG/AWG encrypted UDP traffic before enabling public endpoints.

No code in this DevKit hooks stock Linux WireGuard or implements Noise/AEAD itself. Therefore it is **not production integration**.
