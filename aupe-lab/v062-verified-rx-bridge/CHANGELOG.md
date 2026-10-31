# 0.5.62 — Verified-RX callback bridge lab

- Fix same-epoch Noise confirmation wiping pending fragments: idempotent bind.
- Reject a delayed/older confirmed epoch, even following unbind, via per-peer
  monotonic epoch high-watermark. Closed peer remains terminal.
- New portable `aupe_dispatch_verified()` with a synchronous forwarding
  callback protected by peer-owned serialization, and verified-input contract.
- Deliver only valid IPv4 BYPASS or completed reassembled packets; never
  emit incomplete fragments or stale epoch packets; reject overlapping
  scratch/input storage; scrub reassembled plaintext after callback.
- New kernel APIs `aupe_registry_bind()` and `aupe_registry_dispatch()`
  perform pinned handle lookup and drop owner reference in all paths.
- Expanded C tests and load-time smoke compilation, refreshed reproducibility
  and integration documentation. No actual AWG/WG crypto/dataplane added.
