# Windows 11 Tiny11 — Audit Mode TCG validation

This experimental branch uses official Microsoft Windows 11 Enterprise evaluation media and NTDev Tiny11 Builder. It is **not** a downloadable Microsoft OS distribution.

## Evidence (2026-10-29)

- [Tiny11 ISO → installed UEFI VHDX → ZSTD QCOW2: successful run 40866329555](https://github.com/Pokateli/Temp/actions/runs/40866329555). Raw VHDX: 15,103,688,704 bytes; ZSTD QCOW2: 7,905,148,928 bytes; 3,207 compressed extents.
- [WIM-only full-image comparison and QEMU first boot, run 40869976476](https://github.com/Pokateli/Temp/actions/runs/40869976476). Compressed output: 7,905,607,680 bytes, 3,227 extents, **qemu-img compare: Images are identical**.
- Run 40869976476's actual QEMU VGA screenshots reached **Windows "Starting services" (420s) and "Getting ready" (840s)**. This confirms UEFI boot into Windows setup, but not completion of initial setup/desktop. The original SetupComplete.cmd marker was not written before 1,050s.
- Previous runs booted with **QEMU TCG, no KVM and guest network disabled**, 2 CPUs / 2,560 MB.

## Why Audit Mode

The original unattended Tiny11 first boot was insufficiently observable: SetupComplete.cmd runs late in setup and its absence does not prove the kernel failed to boot.

Microsoft documents **Microsoft-Windows-Deployment / RunSynchronous** commands in the *specialize* and *auditUser* passes, and **Reseal / Mode = Audit** in the *oobeSystem* pass. Here:

1. The modified image is directly applied to GPT FAT32 EFI + NTFS VHDX.
2. `windows-vm-runtime/audit-unattend.xml` is installed into `Windows\Panther\Unattend\Unattend.xml`.
3. `Tiny11Probe.cmd` writes `TINY11_SPECIALIZE.TXT` on the shared FAT disk if the Windows specialize phase runs.
4. Windows is configured to enter Audit mode, which uses the built-in Administrator account on an **offline** test VM.
5. `Tiny11Audit.cmd` writes `TINY11_AUDIT_READY.TXT` when the auditUser phase begins.
6. QEMU host declares success **only** if `TINY11_AUDIT_READY.TXT` contains `TINY11_AUDIT_READY`. It separately logs the earlier specialize marker and saves VGA screenshots.

### Safety and limitations

- **Audit mode is for testing, not a secure production Windows install**: automatic Administrator logon, no guest networking, no public VNC.
- Official evaluation/licensing terms still apply.
- The GitHub Actions workflow uploads **logs and screenshots only**; the Windows QCOW2 stays on the transient runner and is discarded. A separate appropriately authorized private delivery design is necessary before any reusable Windows VM artifact can be provided.
- The real Tiny11 first-boot Audit test is experimental until its GitHub Actions run has completed successfully.

## Links

- Microsoft: [Answer files / audit mode deployment](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/update-windows-settings-and-scripts-create-your-own-answer-file-sxs?view=windows-11).
- Microsoft: [Boot to Audit mode or OOBE](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/boot-windows-to-audit-mode-or-oobe?view=windows-11).
