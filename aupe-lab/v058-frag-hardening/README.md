# AUPE 0.5.58 — IPv4 fragment safety hardening candidate (AMD64)

> **Not an integrated AUPE VPN release.** This is an individually verified C component
> and a *link-only* Linux kernel module. It does not register UDP handlers, implement
> WG/AWG handshakes, perform transport decryption, hook Netfilter, or transmit IP data.

## Source provenance

Based on AUPE 0.5.57 standalone fragment safety laboratory. The historic
`AUPE_v0.5.56_amd64_ipv4_fragment_reassembly_devkit.zip` is present in a
previous-chat Library index, but its raw bytes could not be opened in the
current session. Consequently this tree **has not been integrated with,
compiled from, or regression-tested against the real AUPE 0.5.56 dataplane**.

## Changes from 0.5.57

- Explicitly scrub the entire plaintext fragment workspace on reset, successful
  delivery, overlap rejection, timeout, session unbind and key epoch changes.
- Enforce a 256-fragment ceiling per in-flight datagram per authenticated peer.
- Preserve ECN Congestion Experienced across fragments in accordance with
  RFC 3168 §5.3. Fail-closed on incompatible Not-ECT/ECT and ECT(0)/ECT(1)
  mixing, which can drop some non-conforming traffic; this is deliberate.
- Reject non-final fragments that contradict a previously observed final end.
- Expire undelivered **completed** datagrams instead of keeping them stuck
  indefinitely with `AF_NOT_READY`. New `af_expire()` allows the owner to
  apply expiry from a periodic timer, including when no new packets arrive.
- Add reproducible 4-worker peer separation, 2,500 shuffled multi-fragment
  datagrams, ECN, expiry wraparound, lifetime, bounds and hygiene tests.

## Correct trust boundary

Feed **only** IPv4 inner packets after WG/AWG AEAD authentication and replay
window acceptance. Never identify a client from an inner source IP, destination
IP, IP identification or fragment header; the calling secure peer/keypair owns
exactly one separate `af_state`. A new trusted, nonzero Noise epoch invalidates
all previous incomplete/completed fragments.

All methods are **unsynchronized**. The production RX caller must serialize
operation on one `af_state` and guarantee peer lifetime during RX. Allocate
72+ KiB objects outside kernel stack and outside atomic path; account per-peer
memory and impose a global quota. No such peer integration is in this kit.

One in-flight IPv4 datagram per peer; IPv4 options deliberately rejected;
IPv6 not supported; fragment limit can reject extreme 8-byte legitimate
fragment streams. All of these require compatibility/performance review.

## Local test

```sh
make test
make kernel KDIR=/lib/modules/6.12.107+deb13-amd64/build
modinfo aupe_frag_lab.ko
```

See `docs/BUILD_REPORT.md` and `docs/INTEGRATION.md` for verified and
unverified gates. AMD64 only; ARM64 is postponed.
