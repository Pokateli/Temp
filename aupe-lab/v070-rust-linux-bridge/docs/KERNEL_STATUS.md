# AUPE 0.5.70 kernel status

* The local sandbox has Debian 6.12.107+deb13-amd64 **kernel headers**. Its `.config` reports `CONFIG_RUSTC_VERSION=0`, meaning this is NOT a confirmed kernel Rust build.
* The local VM has been booted on Debian 13.7, `6.12.113+deb13-amd64` (not the 6.12.107 host Kbuild target). Kernel header and rust toolchain are absent inside the guest. VM is OFF between tests. No VM runs on GitHub.
* `scripts/kernel-object-check.sh` compiles the Linux C adapter `.o` under `-Werror` against local 6.12.107 headers. This is a **Kbuild object test**, not a link or load test. Unresolved Rust symbols are deliberately listed.
* Native Rust-for-Linux module cannot be claimed until we use kernel headers/CONFIG_RUST and compatible Rust toolchain for the **same** kernel and obtain successful modpost, insmod, rmmod tests inside local guest.
* The C ABI userspace test is separate from kernel ABI; it does not prove kernel symbol/runtime compatibility.
* WG, AWG 1.x/2.x/3.x handshakes, transport crypto, UDP endpoint, netlink and routing remain incomplete at this stage. No VPN E2E.
