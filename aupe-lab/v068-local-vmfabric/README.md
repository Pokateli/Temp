# AUPE 0.5.68 — VM-Fabric local boot preflight

This directory records a verified *preflight*, not a boot success. It expects a preseeded Debian13 VM-Fabric bundle with portable QEMU binaries and a QCOW2 base image, plus a host-built Rust `vm_smoke` binary.

Source: Pokateli/VM-Fabric release `debian13-v1.2.0-fabric`; package `debian13-qemu-bundle.tar.gz` (or Actions ZIP containing that file). Do not substitute the older Temp release unless the guest has already been preseeded: it can boot into the interactive `systemd-firstboot` wizard.

Use:

```
bash /mnt/data/aupe068-vmfabric-local/prepare-and-run.sh /path/to/debian13-qemu-bundle.tar.gz /path/to/vm_smoke
```

With no bundle present, the launcher fails explicitly before trying to execute QEMU. It always creates a separate overlay and disables guest networking.

`rustc`, `cargo` and the portable QEMU runtime are not available in the current ChatGPT container. The C regression tests were executed locally; Rust parity was confirmed separately in GitHub Actions; running Rust *inside this local VM* remains unverified.
