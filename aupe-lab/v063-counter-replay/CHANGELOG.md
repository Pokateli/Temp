# AUPE 0.5.63 — additional authenticated transport-counter replay gate

- Added `aupe_replay_window` (8192 authenticated transport counters) with
  per-epoch reset and non-wrapping ceiling; zero on teardown.
- Added atomic-under-peer-lock `aupe_dispatch_counter`, plus registry and peer
  wrappers, after an external owner has authenticated a transport counter.
- Reject duplicate, too old, near-overflow or out-of-epoch counters before
  IPv4 forwarding; callback failure does not reset replay history.
- Extended lab module init test with duplicate-counter suppression.
- Extended peer statistics (`rx_replay_drop`) without logging inner IPs.
- New randomized differential tests comparing bitmap implementation with an
  independent full-counter oracle.
- Did not modify actual WG/AWG Noise, AEAD, wire UDP, kernel routing or netns.
  Only 6.12.107 AMD64 build is confirmed, not 6.12.113 or live VM.
