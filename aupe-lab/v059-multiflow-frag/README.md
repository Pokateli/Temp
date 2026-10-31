# AUPE v0.5.59 — peer-scoped dual-flow IPv4 fragment RX laboratory (AMD64)

**Status: source candidate / link-only `.ko`, NOT a standalone VPN server.**
No WG/AWG cryptography, UDP listener, Netfilter hook, routes, keypair handshake,
or deployable AUPE controller are included here. The previous 0.5.56 complete
AUPE source was not recoverable from the previous chat archive in this session.

## New in 0.5.59

* `af_mux` maintains **two concurrent fragmented IPv4 datagrams per authenticated
  peer session**. No client identity is derived from inner IPv4 addresses. The
  third concurrent datagram returns `AF_MUX_BUSY`, without evicting either one.
* Correctly bypasses validated unfragmented inner IPv4 packets even when both
  reassembly slots are occupied; no packet is passed out before AEAD/replay
  verification (which must be done by the external caller).
* A fixed 2-slot memory budget, zero additional memory allocations in RX;
  rekey and unbind scrub both fragment buffers. Timer expiry frees both slots.
* `aupe_rx_peer` kernel-only adapter provisions memory in sleepable process
  context, limits live peer adapters to 64, serializes each peer's operations
  with a mutex and provides a single-lock `ingest()` + `take()` operation.
* Deterministic tests for interleaving, identical inner IPs across peers,
  malformed floods, overlaps, full slots, expiry, epoch rotation, no spillover.

## Verified (local, not GitHub Actions)

* GCC and Clang test runners: **435,898 base + 100,128 new checks**, zero failures.
* GCC ASan+UBSan: same totals, zero reports.
* Linux kernel **6.12.107+deb13-amd64**: standalone `.ko` link/build with `-Werror`.
* No VM boot, module insertion or E2E WG/AWG data transmission tested.
* Target kernel `6.12.113+deb13-amd64` was **not** available in local headers.
* ARM64 intentionally untouched.

## Commands

    make test
    make kernel KDIR=/lib/modules/6.12.107+deb13-amd64/build
    modinfo aupe_frag_lab.ko

The `.ko` only links AUPE fragments, mux and the peer RX adapter. It registers
NO network handlers. Do not install it as a substitute for AUPE or WireGuard.

## Integration constraints

* The real AUPE confirmed WG/AWG peer and **trusted Noise epoch** must own the
  object. No address/route/subnet can function as client authentication.
* Route **only after** AEAD authentication and accepted anti-replay counter.
* `aupe_rx_peer` operations MAY SLEEP; schedule into process-context worker.
  Do not invoke the adapter from kernel softirq/NAPI/atomic context.
* `aupe_rx_peer_destroy` requires all pending tasks and timers to be drained,
  and peer pointer references to be quiesced. There is no internal kref/RCU.
* The caller supplies a distinct output buffer (`>= 65535` bytes recommended).
  Preferred path is `aupe_rx_peer_ingest()` under a single per-peer mutex.
* The 64-peer quota is module-scoped, not yet netns-aware. A real manager must
  establish netns + per-client memory limits and expose drop counters.
* A source/ABI comparison against real AUPE 0.5.56 must precede any merge.
* One session can hold up to **2 × sizeof(struct af_state)** reassembly memory.
  Each slot supports <=256 fragments and retains the 5-second timeout.

See `docs/INTEGRATION.md` and `docs/KERNEL_ADAPTER.md`.
