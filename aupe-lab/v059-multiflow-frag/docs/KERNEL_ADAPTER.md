# Kernel RX integration proposal (not installed)

1. On a confirmed **cryptographic peer identity** (not inner IP), allocate
   `aupe_rx_peer_create(GFP_KERNEL)` in a sleepable worker. Handle `ERR_PTR`.
   64 adapter objects max, each with two reassembly workspaces.
2. Once a Noise session is confirmed and its replay state ready, call
   `aupe_rx_peer_bind(peer_rx, trusted_epoch)` in process context.
3. After decrypting WG/AWG transport and validating the counter against the
   anti-replay window, queue the inner IPv4 plaintext to the peer RX worker.
   Pin peer memory for the lifetime of the queued item. The adapter itself
   does **not** verify cryptography, transport framing, or peer ownership.
4. Invoke `aupe_rx_peer_ingest` from the process worker. Its one-mutex path
   returns AF_DONE and a rebuilt packet length, AF_BYPASS for an original
   unfragmented packet, AF_WAIT for incomplete, or negative drop/reject.
   Route the `AF_DONE` result using the owner peer's session routing.
   Never look up the client by IPv4 source address; duplicate addresses must
   remain supported.
5. On Noise epoch change, drain or serialize outstanding RX and bind the new
   epoch; this destroys previous plaintext fragments. Requeue stale packets
   only after they have passed the *new* session's crypto & replay checks.
6. Keep an owner-driven timer calling `aupe_rx_peer_expire` (the component
   does not register timers). On teardown, revoke the session, stop timers,
   drain RX work, call unbind/destroy, then release outer peer references.
7. Keep the original WG/AWG control path unchanged. This is an optional
   data-path component, not a protocol parser/detection/replacement.

## Limitations

- Internal mutex may sleep: never call from softirq/NAPI/atomic contexts.
- No RCU/kref lifetime management: the integrating real peer must pin and
  drain references before freeing the adapter.
- No netns-aware quota, global traffic/accounting policy, IPv6, IPv4 options,
  fragment selection beyond two simultaneous flows, or production benchmarks.
- No real userspace controller/Netlink, UDP socket, kernel load or WG/AWG E2E
  evidence in this devkit. Thus no claim of release-ready `aupe.ko`.
