# AUPE 0.5.60 — authenticated-peer RX lifecycle candidate

- New portable `aupe_gate`: UNBOUND / ACTIVE / CLOSED, terminal close,
  monotonic **owner-assigned** session-epoch checks. It does not implement
  Noise, peer authentication or anti-replay validation.
- Kernel adapter `aupe_rx_peer` now uses `refcount_t` with explicit get/put
  and a terminal close operation. External owner lock must protect initial
  pointer lookup and publication; workqueue tasks are pinned by reference.
- On close, wipe fragment state and prohibit rebind. Old transport epochs
  fail closed after rotation. Preserve old `destroy` as close + owner put.
- Per-peer aggregate RX counters and a snapshot call, no plaintext/keys/IPs.
- New `tests/gate_tests.c`: epoch rotation, terminal-close, identical inner
  addresses, and pthread-concurrent gate transitions under a single lock.
- Previous IPv4 safety tests and two-slot mux regressions unchanged.
- Kernel 6.12.107 link-only build, no live WG/AWG interoperability proof.
