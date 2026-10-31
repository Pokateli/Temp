# AUPE 0.5.61 — Peer handle registry

- Add bounded owner-scoped handle table (`aupe_handles.h/.c`) with generation
  accounting, slot retirement on generation overflow, and explicit serialization.
- Add per-registry randomized generation initialization, not a security token.
- Add kernel wrapper (`aupe_registry.h/.c`) that atomically pins a peer reference
  under registry locking, supports terminal revoke, and drains cleanly.
- Add load-time kernel smoke test for peer revocation and peer isolation.
- Add userspace tests covering stale handles, wrong generation, full capacity,
  no-wrap, independent registry instances, and pthread churn.
- Retain all features and test suites from v0.5.60.
- No changes to WG/AWG cryptography, wire framing, routing, or controller.

Release type: unprivileged/compile-verified **development kit**, not production.
