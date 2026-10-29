# Tiny11 for ChatGPT Sandbox: reproducible experiments

This is **experimental Windows 11 Enterprise Evaluation 26H2** built with pinned
[NTDev Tiny11 Builder](https://github.com/ntdevlabs/tiny11builder) at
`00e7d8a151a39ccffccab4a267bb81fb3756a01d`.
It uses official Microsoft installation media with SHA-256
`BC3F24086EBADC94489066B5AD78089E2CF5C3491E90E790BB81A2B199C10E38`.
Evaluation usage and licensing remain subject to Microsoft's terms.

## CI pipelines

| Workflow | Purpose |
|---|---|
| `tiny11-staged.yml` | ISO download + Tiny11 ISO creation + VHDX application + ZSTD QCOW2 + TCG boot |
| **`tiny11-wim-only.yml`** | **Optimized: ISO download + Tiny11 modified WIM directly + VHDX + ZSTD QCOW2 + TCG boot** |
| `tiny11-qcow2.yml` | Original exploratory build, plus independent ZSTD roundtrip |
| `vhdx-to-qcow2-preflight.yml` | Proven native Windows VHDX → real ZSTD QCOW2 and bit-identical compare |
| `tiny11-uefi-vhdx-layout.yml` | Proven GPT EFI/MSR/NTFS partitions |
| `tiny11-iso-download-speed.yml` | Microsoft CDN range-read probe |

The WIM-only pipeline removes a redundant intermediate Tiny11 installer ISO,
because only `install.wim` is necessary for applying an OS to a virtual disk.
The Tiny11 customization still runs Microsoft DISM image servicing, which may
take significant time on a shared Windows runner.

## Disk format

```bash
qemu-img convert -p -c -f vhdx -O qcow2 \
  -o compat=1.1,compression_type=zstd tiny11.vhdx tiny11-zstd.qcow2
qemu-img info --output=json tiny11-zstd.qcow2
qemu-img map --output=json tiny11-zstd.qcow2
qemu-img compare -f vhdx -F qcow2 tiny11.vhdx tiny11-zstd.qcow2
```

A compressed QCOW2 is used as immutable **base**. For writable sessions, create
an overlay:

```bash
qemu-img create -f qcow2 -F qcow2 -b /path/to/tiny11-zstd.qcow2 \
    /path/to/tiny11-session.qcow2
```

## Booting in ChatGPT sandbox (once image exists)

- QEMU TCG no KVM, OVMF UEFI, 2 vCPU, 2560 MiB RAM, x86_64 guest.
- Use `windows-vm-runtime/start-tiny11.sh` and `probe-tiny11.py`.
- Base QCOW2 and OVMF_CODE/OVMF_VARS must be supplied separately.
- Guest network disabled by default. Never expose VNC publicly.
- Detect **actual Windows guest readiness** with a guest-authored marker, not
  merely `qemu-img info`, QEMU's process existence, or a UEFI screenshot.

## Images and licensing

The **public** `Pokateli/Temp` repository deliberately uploads
**diagnostics only**. GitHub Actions runners automatically discard built
Microsoft ISO, Tiny11 WIM, VHDX and QCOW2 at job end. Nothing in this
experimental branch is a ready-to-download Windows OS image.
After a successful QEMU guest test, arrange an appropriately permissioned,
private and license-compliant delivery method.

## Verified results (2026-10-29)

- **Full Tiny11 build + Windows offline installation + ZSTD QCOW2 succeeded:** [GitHub Actions run 40866329555](https://github.com/Pokateli/Temp/actions/runs/40866329555).
- Real installed-Windows VHDX physical size: **15,103,688,704 bytes**.
- Real ZSTD QCOW2 physical size: **7,905,148,928 bytes** (~47.66% less).
- `qemu-img map` reported **3,207 compressed extents**.
- Separate synthetic and native VHDX preflights have independently verified `qemu-img compare`; the original successful full-image build did **not** yet include a full byte-level comparison step.
- The successful run did **not** perform an actual Windows guest boot test or retain the QCOW2 artifact. **Real Windows boot in TCG remains unconfirmed** until `tiny11-boot-proof.txt` is produced.
- The updated WIM-only workflow adds both a full-image comparison and QEMU TCG first-boot validation, while preserving the public repo's diagnostics-only policy.

