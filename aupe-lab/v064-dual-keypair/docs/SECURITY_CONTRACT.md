# AUPE 0.5.64 authenticated RX integration contract

## Trust boundary

1. Authenticate a Noise/WG/AWG peer using the appropriate cryptographic
   handshake, not client inner IP, source UDP endpoint or a supplied handle.
2. Resolve/hold an authenticated keypair object with its verified public-key
   peer identity. A **locally allocated, monotonically increasing, nonzero**
   keypair generation is assigned exactly once to that authenticated keypair.
   Do not use remote-controlled WG receiver indices as trusted generations.
3. After AEAD succeeds for a transport packet, obtain its authenticated counter
   from the verified transport header; call `aupe_registry_dispatch_keypair`.
4. Pass a trusted **monotonic** clock in milliseconds; caller timestamps must
   not regress. The bridge enforces a maximum 180-second overlap for previous
   keypair. The cryptographic engine still independently enforces WG key
   lifetime, counter limits, replay and rekey requirements.
5. User-space callers cannot forge a peer handle, epoch, packet digest or
   authenticated counter: these are privileged internal kernel interfaces,
   not an ioctl or netlink interface exposed to untrusted clients.

## Lifecycle and invariants

- Max two installed keypair epochs per peer. New rekeys evict the oldest.
- Keypair windows are always independent: counter 0 on new keypair is not
  implicitly a replay of counter 0 on the previous keypair.
- An exact repeat of an accepted `(keypair, counter)` is rejected, even if
  the downstream IPv4 callback reports an error.
- A previous epoch becomes permanently unusable after timeout, explicit
  retirement, or a subsequent rekey. Generations cannot decrease.
- The current epoch is retired only by unbind/revoke. Unbind never re-enables
  the older unscoped RX entry points.
- Buffer overlap with scratch is rejected before consuming counter.
- `aupe_registry_revoke` first unpublishes the handle and closes the peer;
  live pinned references drain before final memory scrub.
- Owner must quiesce registry users before `aupe_registry_destroy`.
- RX, bind, rekey and retire execute under per-peer mutex, in process context.
  Callback must not reenter AUPE APIs and must not retain pointers.

## Still missing

- Connection to genuine WG/AWG Noise/AEAD and local keypair object lifecycle.
- Validated association of real keypair (pointer or cryptographic identity)
  with the AUPE handle in the production netns manager.
- NETFILTER/route/TUN TX dataplane, peer-specific IP addressing and full E2E.
- Network adversarial tests, actual `insmod`/`rmmod`, reboot and soak testing.
- Exact 6.12.113 Debian build and virtual-machine smoke.
