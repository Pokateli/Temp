# AUPE 0.5.64 — dual-keypair authenticated RX lab (AMD64)

This is a **laboratory extension**, not a deployable WG/AWG VPN server. The binary
`aupe_frag_lab.ko` has no UDP listener, Noise handshake implementation, AEAD or
production dataplane, and it has **not been loaded** in a VM. In particular, no
WG/AWG interoperability or Debian 13 kernel 6.12.113 test is claimed.

## Change since 0.5.63

The verified RX bridge now supports **two distinct authenticated keypair
sessions** (current and previous) with independently advancing 8192-bit replay
windows, hard 180-second prior-key grace, explicit previous-key retirement,
monotonic keypair generations, and irreversible disabling of unsafe legacy RX
entry points when keypair mode is enabled. The peer handle and IPv4 fragment
mux continue to be independent of any client inner IPv4 address.

WireGuard/AmneziaWG's real cryptographic owner must bind an AUPE peer handle to
the **cryptographically verified public key**, assign a never-reused monotonic
local epoch to each established keypair, and call the new registry APIs only
**after successful AEAD authentication**. These APIs **do not decrypt packets**
or independently verify that a keypair epoch belongs to the authenticated peer.

## Build and test

```sh
make test CC=gcc
make -B test CC=clang
make -B test CC=gcc CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread -fno-omit-frame-pointer -fsanitize=address,undefined'
make kernel KDIR=/lib/modules/6.12.107+deb13-amd64/build
```

The C code is shared between native tests and the load-time kernel smoke. The
kernel smoke is compiled, **not executed** in this environment.

## Interfaces

- `aupe_registry_install_keypair(registry, peer_handle, trusted_epoch, now_ms)`
- `aupe_registry_dispatch_keypair(registry, handle, keypair_epoch, authenticated_counter, now_ms, ... )`
- `aupe_registry_retire_previous(registry, handle, old_epoch)`
- `aupe_registry_revoke(registry, handle)`

All functions may sleep, and callbacks run under a peer mutex: call only from a
process-context worker, never an IRQ/softirq. The owner must synchronize the
registry's own lifecycle and must never let a callback reenter the peer/registry.

See `docs/INTEGRATION.md`, `docs/SECURITY_CONTRACT.md` and `docs/BUILD_REPORT.md`.
