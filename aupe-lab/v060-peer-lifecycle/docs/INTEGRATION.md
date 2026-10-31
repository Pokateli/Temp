# AUPE 0.5.60 — full AUPE integration gates

**Current status: separate component, not merged into original AUPE.**

1. Acquire exact AUPE 0.5.56 code and compare struct layouts, exported
   symbols, cryptography, module Makefile, transport packet routing and API
   against this lab adapter. Do not overwrite active WireGuard/AWG objects.
2. Attach `aupe_rx_peer` to the **authenticated public-key identity** after
   Noise handshake completion; choose a unique trusted 64-bit epoch per
   session (do not trust client-supplied metadata to choose it).
3. For every queued RX operation, obtain peer reference under the owner's
   external lookup/list lock. A task must retain it through ingestion,
   routing decision and final `put()`.
4. Verify AEAD and replay-window acceptance before any inner IPv4 handling.
   The adapter accepts decrypted plaintext; it does not implement the WG/AWG
   handshake, cipher, replay checks, or protocol autodetection.
5. From sleepable process-context RX worker call `aupe_rx_peer_ingest`.
   `AF_WAIT`: consumed incomplete fragment; `AF_DONE`: route rebuilt packet;
   `AF_BYPASS`: route original already authenticated, validated IPv4 packet;
   negative result: drop. Never use inner src/dst as peer identity.
6. On keypair rotation, bind a *different* trusted epoch to scrub old slots.
   On permanent removal, unpublish pointer, close, then release owner ref.
   Pinned workers may safely reject with `AF_CLOSED` until their last put.
7. Add netns-scoped admission accounting, RX queue capacity and backpressure,
   expiry callback (5 s), netlink counters and low-memory handling to main
   module before making this path default. Current quotas are module-global.
8. In VM-Fabric Debian 13 run KASAN/KCSAN/lockdep load/unload tests, repeated
   overlapping fragments, same-IP multi-peer tests, session rekey races,
   concurrent delete/worker drain, and WG/AWG interop; route and packet
   accounting must use authenticated peers only.
9. Build and boot on exact target kernel `6.12.113+deb13-amd64`; verify
   `vermagic`, dynamic symbol compatibility and live module insertion.
   No success claim for these gates exists in this devkit.

Use no metered cloud storage. No GitHub Actions necessary: tests run locally;
share small source archives in git branches or Release assets if needed.
