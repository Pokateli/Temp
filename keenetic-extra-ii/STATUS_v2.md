# ZyXEL Keenetic Extra II (ki_rb) — status, 2026-11-01

## Validation of donors on GitHub Actions (ephemeral, no artifact upload)

- **Official 3.05.C.10.0-0** firmware file was downloaded from Keenetic's archive ZIP and analyzed, including CRC and complete SquashFS inode traversal.
- SHA256 (original inner BIN): `34f6cdf179dd4cd5145b8f93a57a307f9c4c55f1bc2acd16512190d5b2651443`
- BIN length `11665468` bytes; SquashFS offset `0x180000`; `1037` inode paths; has `/sbin/init`; status `STRUCTURE_OK_NOT_BOOT_TESTED`.
- **Broken 5.01.C.8.0-2 ki_rb**: SHA256 `bac3342f7c4b4767bc8ad53195e679014d43216abf7ec76af45370f3eb69e67e`, BIN length `7798844` bytes; SquashFS at `0x170000`, 616 inode paths, no `/sbin/init`, no `/bin`, no `/sbin`; `BLOCKED`. Headers/checksums correct.
- Important clarification: official valid 3.05 does NOT contain `TLV.NDM` either; absence of TLV alone is **not** the cause of a kernel boot failure. SDK 5.01 unpack.sh needs TLV for automatic import; this is a separate tooling issue.
- Official 5.01.C.8.0-2 SDK contains profile `KN-1713`, but does not contain `ki_rb`.
- KN-1713 has MT7613 5GHz radio, while original Extra II uses MT7612E/MT76x2 5GHz radio. Directly reusing KN-1713 board profile is incompatible.
- All attempted guessed `osvault` URLs for the full KN-1713 5.1.8 donor and ki_rb 4.3.6.3 donor were inaccessible from runner as of probe, so no compatible 5.01 vendor userspace has been acquired.

## Remaining work before producing firmware

1. Obtain a legitimate complete KeeneticOS 5.01 (MIPS/MT7628) donor BIN with exact version and matching loader, libc, init and ndm. The proprietary root filesystem is not reconstructible from the broken fragment alone.
2. Use SDK matching tag `5.01.C.8.0-2`, unpack that 5.01 donor with correct TLV and xattrs (SDK `unpack.sh`). Ensure required proprietary packages are not empty.
3. Implement actual `ki_rb` target: hardware ID, flash regions, dual-image rules, MT7612 radio, EEPROM/calibration partition, GPIO, Ethernet switch and USB using known-good old image and live board information.
4. Build only after a complete rootfs exists, test rootfs executables and interpreter dependencies, package CRC/TLV metadata and image size.
5. Boot-test first with UART access and a recovery path. Full dual-slot hardware validation cannot be guaranteed in generic QEMU.

**Do not flash the broken BIN. No repaired or verified bootable 5.1.8 binary has yet been made.**

## GitHub

- Repo folder: https://github.com/Pokateli/Temp/tree/main/keenetic-extra-ii
- Donor report: https://github.com/Pokateli/Temp/blob/main/keenetic-extra-ii/reports/donor-probe.md
- 5.x donor attempt: https://github.com/Pokateli/Temp/blob/main/keenetic-extra-ii/reports/5x-donor-probe.md
- Official SDK: https://github.com/keenetic/keenetic-sdk/tree/5.01.C.8.0-2