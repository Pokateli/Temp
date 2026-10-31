# Actual AUPE 0.5.57 integration gates

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
