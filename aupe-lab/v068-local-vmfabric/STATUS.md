# AUPE 0.5.68: local VM status — 2026-11-01

## GitHub releases confirmed

Pokateli/VM-Fabric release `debian13-v1.2.0-fabric` exists and has a verified durable asset:
- `debian13-qemu-bundle.tar.gz`
- 427,155,458 bytes
- SHA256 `b711d8f33bb5cde476cb31fff7f44e13260ed3ad8ca86b1f968876cd4740697f`
- A separate SHA256 sidecar exists.

The older public `Pokateli/Temp` archived VM bundle is NOT an equivalent preseeded guest. The recent GitHub runner boot log shows Debian `systemd-firstboot` waiting for user input. Use the private VM-Fabric release.

## Current local runtime

- Debian GNU/Linux 13 (Trixie), container kernel 6.18.57
- QEMU system binary and qemu-img: missing (portable QEMU comes in the release, but its archive is not mounted)
- Guest compressed QCOW2: missing from this session
- rustc/cargo/vm_smoke: missing from this session
- Container network DNS: GitHub unreachable (NETWORK=caas_packages_only)
- Github connector: source code and release metadata works, but its fetch operation does not export oversized binary assets into `/mnt/data`.

Thus **local Debian 13 VM not booted** in this session. No claim of guest-side AUPE Rust success.

## What was verified

- Local GCC, Clang, GCC ASan+UBSan C-baseline regressions PASS in existing test logs.
- Rust 0.5.68 ABI parity and no_std rlib were confirmed in separate GitHub Actions logs (not reproduced inside local VM).
- `prepare-and-run.sh` Bash syntax passes; preflight cleanly reports absent archive (exit 20).
- The preparer pins the actual VM-Fabric Release SHA256 when a separate `.sha256` sidecar is absent. It will never overwrite an existing image; all guest writes use fresh QCOW2 overlay.
- Guest network remains disabled.
- VM-Fabric image's target kernel configuration must be read from the guest; the current local `/boot/config-6.12.107+deb13-amd64` has `CONFIG_RUSTC_VERSION=0`, which does **not** establish Rust-for-Linux support for a `.ko`.

## Files

`download-private-vmfabric.sh` uses an authorized local gh CLI connection to fetch from the existing private release without GitHub Actions or metered Actions storage. It cannot download in this restricted sandbox.

`prepare-and-run.sh VERIFIED_VM_BUNDLE VM_SMOKE_BINARY [WORKDIR]` checks hashes, stages the portable QEMU bundle, and invokes the AUPE guest serial console test with isolated overlay and no network.
