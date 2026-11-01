# GitHub/VM-Fabric integration notes

The source is intended for an AUPE development branch of `Pokateli/Temp`.
No GitHub Actions are needed to build the userspace tests. Kernel module
compiles against the local 6.12.107 Debian13 headers, but the target
6.12.113 headers were not available to this runtime. QEMU is not installed
here; no VM was claimed to have been booted.

For future testing with VM-Fabric, prefer a local throwaway qcow2 overlay,
no paid or metered persistent storage, a network-isolated guest, `insmod`
then `rmmod` logs, followed by real encrypted WG/AWG peer tests once crypto
integration is implemented. ARM64 is postponed.
