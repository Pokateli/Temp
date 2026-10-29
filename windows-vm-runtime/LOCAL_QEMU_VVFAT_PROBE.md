# Real QEMU vvfat guest/host readiness marker validation

Date: 2026-10-29; run environment: ChatGPT Linux container, QEMU TCG 10.0.13.

## Purpose

Verify that the **real Windows guest** (not mock/test Python) can write
a boot-ready signal to the host filesystem via the `file=fat:rw:...` QEMU drive.
Both the ready-disk builder and the portable sandbox launcher depend on this.

## Configuration

- Guest: Microsoft Windows 11 Enterprise 26H2 evaluation **WinPE**, from official ISO boot.wim (Windows 10.0.26100.9457).
- Firmware: OVMF UEFI, QEMU `pc,accel=tcg`, `-cpu max`, 2 vCPU, 2560 MiB RAM.
- Storage: Windows PE 611 MiB ZSTD QCOW2 with independent writable overlay.
- Additional IDE drive: `-drive file=fat:rw:/mnt/data/tiny11-local-run/share,format=raw,if=ide`.
- Guest network disabled.
- Guest shell opened through QEMU monitor `sendkey shift-f10`.

## Observations — real guest

1. Guest `dir C:` returned **Volume in drive C is QEMU VVFAT**, showing
   `TINY11_PROBE.TXT` created by the host.
2. Guest `echo ok>C:\test.txt` wrote a new file visible in Linux host directory
   as `test.txt`, with content `ok`.
3. Guest `echo READY>C:\TINY11_FIRST_LOGON_READY.TXT` created the exact
   **long Windows filename** required by the Tiny11 first-login readiness protocol.
4. After an additional guest `dir C:`, the host directory showed
   `TINY11_FIRST_LOGON_READY.TXT` (7 bytes) alongside the original marker.
   This confirms host-visible guest-to-host writes and VFAT long filenames.
5. The guest readiness marker can be delayed by virtual-FAT metadata refresh;
   scripts should execute `dir %%D:\` after writing and host readiness
   checks must also check after VM shutdown.

## Code changes from this finding

- `tiny11-qcow2-lab/apply-tiny11-ready.ps1`: guest batch scripts refresh
  the exchange volume after first-login and later boot marker writes.
- `tiny11-qcow2-lab/finalize-ready.ps1`: after QEMU closes normally, recheck
  the first-login marker before treating an OOBE boot as failed.

This verifies the guest-to-host communication **only**. It is not a substitute
for an actual completed Tiny11 OOBE/desktop readiness test. That remains gated
on the ready-disk build workflow succeeding.
