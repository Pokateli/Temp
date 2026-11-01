# AUPE 0.5.64 — dual-keypair replay overlap laboratory

- Added `aupe_keypair_bank` with two *per-keypair* replay windows (8192 counters each),
  monotonic generation tracking, 180-second previous-key grace, explicit retirement.
- Corrected prior single-window rekey behaviour: old keypair counters can remain
  independently valid during a bounded overlap; duplicate old counters still drop.
- Added `aupe_dispatch_keypair()` and pinned registry/peer interfaces.
- After installing a keypair, all older unscoped RX/dispatch APIs fail closed;
  keypair mode cannot be reverted by unbind.
- Stale keypair epochs fail closed after retirement, timeout or second rotation;
  no peer lookup depends on tunnel IP. Old fragmented plaintext is scrubbed at
  primary epoch rotation. Previous packets arriving afterward use the current
  IPv4 reassembly generation, subject to their own replay window.
- Added deterministic and randomized per-keypair tests plus compile-only kernel
  load-time smoke for rotation and retirement.
- Verified native GCC, Clang and GCC ASan/UBSan tests. Built for Debian 13
  headers 6.12.107+deb13-amd64, `-Werror`; not loaded. 6.12.113 and real WG/AWG
  cryptographic E2E remain not verified.
