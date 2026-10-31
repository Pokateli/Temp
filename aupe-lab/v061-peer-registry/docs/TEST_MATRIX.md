# Target qualification matrix (pending)

1. Debian 13 `6.12.113+deb13-amd64` with exact build headers: build and
   inspect vermagic; load test the lab selftest and inspect `dmesg`.
2. Confirm load + unload, repeated registry create/destroy and no leaks.
3. Load with kernel concurrency instrumentation (KASAN / lockdep), if available.
4. Confirm single and dual peer with duplicate `172.16.2.2` inner IP addresses.
5. Confirm WG and AWG handshakes, actual AEAD verification, replay rejection,
   then two concurrent IPv4 fragment flows per authenticated peer.
6. Confirm rekey, revocation with outstanding RX jobs, process/service stop,
   and namespace teardown. Test adversarial stale handle and epoch replay.
7. Test UDP endpoint with WG and AWG versions only once compatible kernel
   crypto/parser and controller actually exist. The current lab is not this.

Tests must use offline/local QEMU/VM-Fabric assets or existing non-billable
GitHub Releases. Avoid new Actions artifact uploads.
