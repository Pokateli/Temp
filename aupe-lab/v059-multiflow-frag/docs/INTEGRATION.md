# Actual AUPE 0.5.59 integration gates

This directory contains portable C implementation `src/aupe_frag.c` and a
kernel link-only stub. It DOES NOT contain the AUPE 0.5.56 dataplane.

## Required steps before merging into `aupe.ko`

1. Retrieve and compare the exact `0.5.56` sources from the last kit.
   Verify the existing fragmentation implementation's API and ownership;
   preserve working WG/AWG transport, replay window, peer identity, UAPI,
   routing, and all protocol-specific behavior.
2. Call only **after** keypair lookup, AEAD decryption, authenticating the
   confirmed peer and accepting its anti-replay counter. Never take client
   identity from IPv4 source address, ID or protocol fields.
3. Bind `af_state` to a trusted nonzero session epoch when the Noise session
   is promoted; on rekey invoke `af_bind_epoch` to flush old fragments. During
   teardown call `af_unbind`, then free memory only after RX users drain.
4. Allocate the workspace in safe context; impose global allocation quotas
   to prevent a large pool of inactive peers from consuming RAM. Size each
   `struct af_state` for the deployed architecture using `sizeof`.
5. Serialize `af_feed_ipv4`/`af_take_ipv4`/`af_bind_epoch`/`af_unbind` per
   session. Avoid expensive copies while holding a global or high-contention
   spinlock. Consider deferred processing to fit kernel softirq constraints.
6. For `AF_DONE` call `af_take_ipv4` with a distinct output buffer of at
   least `AF_MAX_PACKET` bytes. For `AF_WAIT` hold; on negative result drop
   and account; for `AF_BYPASS` pass the original packet to normal routing.
7. Put the reassembled packet through the *existing* AUPE inner IPv4 route
   selection and checksum validation. Keep duplicate client IPs allowed;
   do NOT use IP source as session identity.
8. Re-run regression suites with 1, 2 and >=3 peers where available, using
   duplicate inner IPs, WG and AWG modes, key rotation, plaintext rejection,
   malicious fragments, memory quotas and forced teardown racing RX.
9. Build against the exact Debian 13 target headers (`6.12.113+deb13-amd64`)
   with `-Werror`, then boot test that exact `vermagic` inside VM-Fabric.
   Only claim kernel/E2E success after module load and actual inner IP DATA.

## Known candidate limitations

- IPv4 header options are rejected (IHL != 5). IPv6 fragmentation is out of
  scope. No protocol autodetection logic is changed here.
- One incomplete datagram per peer/session may cause drops under concurrent
  fragmentation; preserve existing throughput unless feature is explicitly
  enabled or this tradeoff is accepted after measurement.
- Worker synchronization, memory accounting, metrics, UAPI and module lifecycle
  are integration responsibilities not implemented by this lab.
- Fragment acceptance does not prove data-plane safety unless the caller
  guarantees cryptographic preconditions and routes per trusted session.

## Additional 0.5.58 integration gates

- On key epoch/peer transitions, atomically exclude in-flight RX operations;
  do not free a workspace before its last reference drops.
- Never call `af_feed_ipv4` before the keypair/peer ID is authenticated and
  its transport counter accepted by the replay window. Fragments cannot
  inherit the identity of an IP address or subnet.
- Audit kernel-side lock hold time at maximum 65535-byte datagram and any
  memory wipe: do not unconditionally copy 65 KiB within a global spinlock.
- Allocate workspaces in process context, enforce netns/global active limits;
  test active peer quota exhaustion and concurrent key revocation.
- Decide whether ECN inconsistent fragments should be dropped (this candidate)
  or whether Linux net/ipv4 reassembly logic should be reused instead.
- Ensure parity with IPv4 options, IP ID collisions and extreme small-fragment
  traffic before enabling by default.
- Add counters for `AF_ECN_CONFLICT` and `AF_FRAGMENT_LIMIT` without emitting
  secret data or untrusted inner packet metadata.

- Call `af_expire(state, trusted_epoch, monotonic_ms)` from the trusted peer
  maintenance timer to enforce expiry when RX becomes idle. The component does
  not install a timer itself. Serialize this callback with RX and teardown.

## 0.5.59 addition

`af_mux` supports two datagrams per authenticated peer, but only the
process-context `aupe_rx_peer` adapter enforces a live-object quota and
per-peer mutex. Use `aupe_rx_peer_ingest` for single-lock feed + take, after
authentication and accepted anti-replay. Never register it as a stand-alone
Netfilter callback. See `KERNEL_ADAPTER.md`.