# Linux kernel integration — 0.5.69 status

Verified in the LOCAL Debian13 VM: Debian 13.7, `6.12.113+deb13-amd64`, `CONFIG_MODULES=y`.

`CONFIG_RUST` was not readable from `/boot/config-*`, `/proc/config.gz`, or guest `/lib/modules/*/build/.config`; **unknown** (not evidence that Rust support is enabled). The guest lacks kernel headers, GCC/make, and rustc. Previous C `aupe_frag_lab.ko` was compiled with `vermagic` for 6.12.107, not 6.12.113, so cannot be loaded into the current guest.

This release is *not* a genuine `aupe.ko` or working WG/AWG VPN. Kernel interfaces and Noise/AEAD wire integration need separate work and tests under a Rust-enabled matching kernel. Do not link a generic Rust userspace staticlib into the Linux module without the kernel's supported Rust toolchain/runtime/ABI. Only the `no_std` core and C-ABI surface are prepared.
