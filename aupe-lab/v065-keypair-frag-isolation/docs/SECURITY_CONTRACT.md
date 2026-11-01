# AUPE 0.5.65 trusted keypair RX contract

1. The verified **public key / peer identity** comes only from cryptographic
   authentication, never from inner IPv4, outer source IP, or UDP receiver index.
2. The keypair owner assigns a unique increasing *local* epoch to each newly
   confirmed keypair. Each authenticated transport packet carries that epoch
   in trusted kernel metadata associated with the actual decrypting keypair.
3. The crypto owner verifies AEAD and its own WG/AWG replay rules **before**
   calling `aupe_registry_dispatch_keypair()`. AUPE's separate 8192-counter
   window is an additional defense, not a replacement.
4. The *current* and *previous* replay windows and fragment muxes are separate.
   No fragment can complete a datagram started under another keypair, even if
   its inner src, dst, protocol and IPv4 identification collide.
5. Up to two incomplete IPv4 packets per keypair, four per peer. The older
   previous mux is securely discarded on rekey; previous is also discarded
   on retirement/expiry; all plaintext is scrubbed on peer close/revocation.
6. Previous-key overlap is bounded by `AUPE_KEYPAIR_GRACE_MS` (180,000 ms)
   and does not extend if unrelated packets arrive. Clock must not regress.
7. `af_mux_feed` and `aupe_dispatch_keypair` require a peer-held mutex and a
   pinned peer lifetime. Call only from process context. Callback may not
   recursively reenter registry or retain the provided packet pointer.
8. Scratch and ciphertext/plaintext source must not overlap. The callback's
   return does not resurrect consumed authenticated counters.
9. A local peer handle, epoch and counter are **not** a cryptographic proof.
   Real owner binding is not implemented in this laboratory source tree.

## Required production follow-up

Bind an AUPE registry peer to the cryptographically verified public key in
WG/AWG Noise handshake completion; associate each decrypting keypair object
with a trusted, nonreused local epoch. Then connect TX, routing, Netfilter,
real encrypted UDP I/O and status reporting. Test under Debian 13 kernel
6.12.113 and fuzz malformed packets/rekey races before any public listener.
