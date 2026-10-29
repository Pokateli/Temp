Additional VM control helper: `windows-vm-runtime/vm-control.py`
supports local QEMU HMP status/screenshot/ACPI shutdown for a running VM;
the synthetic dual-QEMU workflow validates its Python syntax.

## 2026-10-30 — VERIFIED READY TINY11 IMAGE PUBLISHED

**SUCCESS** [GitHub Actions run 40928819947](https://github.com/Pokateli/Temp/actions/runs/40928819947) completed with conclusion `success`. This is the first publicly uploaded preinstalled Tiny11 VM kit ready for ChatGPT/VM-Fabric (not a mere synthetic test). Source was a verified DISM-applied Windows evaluation install-stage image from run 40908895996.

The following *actual published* artifacts were downloaded and inspected:
- `tiny11-ready-public-index` (artifact 12923083196): contains `ready-parts.json`, 20 shards `part-000.bin` through `part-019.bin`, each 384MiB except final smaller shard, all with SHA256.
- `tiny11-kvm-guest-proof` (artifact 12924120589): `probe-result.json` reports `boot_confirmed=true`, `runtime=qemu-kvm`, `fat_marker_confirmed=true`, `serial_marker_confirmed=true`, `probe_overlay_discarded=true`, `acpi_shutdown_requested=true`; `guest_shutdown_clean=false` and `host_terminated=true` on **second probe only**.
- Reusable base disk `tiny11-ready-zstd.qcow2`: **8,021,803,008 bytes** (64GiB virtual), SHA256 `9d6b897b3ba5326b6de380b8ef48898c5c9f561b65e4b44507eb713e4300d402`. All six expected files are included, plus `OVMF_CODE.fd`, `OVMF_VARS.fd`, manifest, checksum list, and guest credentials.
- The **first** KVM Windows OOBE/first login ended with the guest shutting down normally, after which `qemu-img convert -c ... zstd` flattened and produced the immutable ready disk. The **second** KVM boot proved live Windows through COM1 and FAT; ACPI shutdown did not complete; the disposable test overlay was discarded. This is correctly not reported as `guest_shutdown_clean`.
- Full ready artifacts reside at the run's Artifacts panel, named `tiny11-ready-public-shard-000` to `-019` and `tiny11-ready-public-index`. They have finite retention and must be archived in an authorized destination before expiration.
- `windows-vm-runtime/ready-parts.py unpack /path/to/parts /mnt/data/tiny11-ready --kind ready` verifies SHA256 on every shard and reconstructed member. `windows-vm-runtime/bootstrap-chatgpt.sh verify` tests the restored disk with portable QEMU; `start` boots under QEMU TCG in ChatGPT without KVM. The entire 8GB ready image has NOT yet been downloaded into this current chat runtime; only the small index and proof were independently inspected. Do not claim a ChatGPT VM boot before this step is executed.

**Parallel production run 40922779780** failed *after successful shard reassembly*, because its workflow asked for legacy `STAGE-SHA256SUMS` (not included in the new stage shard format). This obsolete check has already been removed in later branch versions. The newer #40971481022 workflow was still preparing WIM at last check, so its result is not evidence of a second installed-Windows image.

---
# 2026-10-30 — FIRST SUCCESSFUL READY TINY11 QCOW2, RECOVERED IN CHATGPT

**Ready Windows image is confirmed built and published** in
[GitHub Actions 40928819947](https://github.com/Pokateli/Temp/actions/runs/40928819947)
(`ubuntu-kvm` job success). The workflow reused the DISM-installed Windows
stage from run 40908895996, finalized the first KVM OOBE/login with clean
shutdown, verified second installed-Windows startup, and published 20 public
`tiny11-ready-public-shard-NNN` artifacts plus
`tiny11-ready-public-index` and `tiny11-kvm-guest-proof`.

**Exact verified content**:
- Disk: `tiny11-ready-zstd.qcow2`, 8,021,803,008 bytes physical, 64 GiB virtual,
  `qcow2` compression-type `zstd`, **no backing file**.
- Disk SHA-256:
  `9d6b897b3ba5326b6de380b8ef48898c5c9f561b65e4b44507eb713e4300d402`.
- The 20 transfer parts span 8,026,009,600 TAR bytes; individually SHA256 verified.
- Exact six member files: `tiny11-ready-zstd.qcow2`, `OVMF_CODE.fd`,
  `OVMF_VARS.fd`, `tiny11-ready-manifest.json`, `ready-credentials.txt`,
  `SHA256SUMS`.
- GitHub final guest proof `probe-result.json`: `boot_confirmed=true`,
  `fat_marker_confirmed=true`, `serial_marker_confirmed=true`,
  `probe_overlay_discarded=true`, `acpi_shutdown_requested=true`,
  `guest_shutdown_clean=false`, `host_terminated=true`.
  The second-boot guest was forcibly stopped after confirming boot; its
  disposable overlay was **not merged** into the clean ready base.

**ChatGPT sandbox extraction**:
- All 20 GitHub artifacts were downloaded to active session,
  ZIP container removed after extraction, parts preserved under
  `/mnt/data/tiny11-ready-public-parts/part-000.bin` ... `part-019.bin`.
- Reconstructed exact ready kit under `/mnt/data/tiny11-ready/`, all
  six source members SHA256 verified against index. `qemu-img check -f qcow2`
  returned `No errors were found on the image.`
- Portable QEMU 10.0.13 extracted to
  `/mnt/data/tiny11-local-qemu/debian13-chatgpt-vm`.
- Launched QEMU **TCG** in this sandbox with separate overlay
  `tiny11-sandbox-session.qcow2` and copy of UEFI VARS. First serial
  and VGA frames showed UEFI Windows Boot Manager and `Getting devices ready`.
  **Confirmed real Windows 11 desktop and taskbar** in this chat via QEMU
  HMP `screendump`, after the `Sandbox VM / Welcome` screen. A second
  screenshot after sending `sendkey shift` woke the sleeping display and
  showed active Start/Search/taskbar and evaluation watermark.
  This is a successful installed-Windows GUI boot in ChatGPT without KVM.
  The GUI remains slow under TCG and the Windows Enterprise Evaluation
  screen shows `Windows License is expired`; a properly licensed build is
  required for sustained production use.

**Bug fixed**: earlier KVM shard-transfer run 40922779780 failed solely because
it verified restored stage against `STAGE-SHA256SUMS`, which the
`stage` shard FILESET intentionally omitted. The stage pack/unpack itself
had correctly verified the constituent files. This redundant check has been
removed from `.github/workflows/tiny11-ready-kvm-public.yml`.
After that workflow edit, a new full build triggered; it is not needed to
establish validity of ready run 40928819947.

**ChatGPT compact QEMU bootstrapping** was also updated to detect the
`debian13-chatgpt-vm` root of the archived QEMU runtime and autoextract
`/mnt/data/tiny11-qemu-runtime-linux-amd64.tar.gz` when available.

**NEXT**: integrate the verified ready kit into VM-Fabric lifecycle and persist
its 20 shards somewhere longer-lived than ephemeral Actions artifacts.
GUI desktop boot in ChatGPT TCG is now visually verified. This project's public Temp
artifacts were explicitly approved by the user for temporary development.

---

## 2026-10-29 — Root cause of large GitHub artifact transfer and fix

**Confirmed real failure** (not Node warning): [KVM run 40906341119, ubuntu job 123600680280](https://github.com/Pokateli/Temp/actions/runs/40906341119/job/123600680280) first transferred `tiny11-kvm-stage-plain`, one 7,906,067,331 byte artifact. The `actions/download-artifact@v4` output included Node `DEP0005 Buffer()` warning, but later SHA256 stage validation failed: `tiny11-zstd.qcow2: FAILED`, `ready-credentials.txt: No such file or directory`. Therefore the *reassembled/extracted 7.9GB stage was invalid*, regardless of the incidental deprecation warning. No guest Windows KVM OOBE was attempted on that corrupt stage.

**Implemented**: `windows-vm-runtime/ready-parts.py` accepts `--kind stage`, preserving both stage QCOW2 and credentials inside a streamed tar archive split into independently hashed 384MiB `part-NNN.bin` files with `stage-parts.json` manifest; the old `ready` format remains supported. `.github/workflows/tiny11-ready-kvm-public.yml` uses one GitHub artifact **per shard** (32 conditional uploads max) plus an index artifact, downloads with `pattern: tiny11-kvm-stage-part-*` and `merge-multiple: true`, verifies both individual part hashes and restored file hashes, and then checks the original `STAGE-SHA256SUMS`. The previous one-piece `tiny11-kvm-stage-plain` upload and download were removed in that workflow. Note stage-sharding does not make Windows boot/OOBE success automatic; it fixes transfer integrity.

**Validation**:
- [Stage and ready sharding cross-boundary smoke #40922811605](https://github.com/Pokateli/Temp/actions/runs/40922811605) passed.
- [Windows uploader → Ubuntu downloader cross-runner test #40922888751](https://github.com/Pokateli/Temp/actions/runs/40922888751) passed. Both jobs completed success; logs show `PASS: Windows uploader to Linux downloader produced byte-identical stage and credentials`. This test uses synthetic data, **not** a real Windows image.
- New full production build [#40922779780](https://github.com/Pokateli/Temp/actions/runs/40922779780) was triggered by the sharded workflow change and is subject to completing the original Microsoft WIM build first.
- Earlier jobs, including [#40908895996](https://github.com/Pokateli/Temp/actions/runs/40908895996), retain their *original checkout* and old 7.9GB monolithic download, so do not mistake their ongoing deprecation warning for a test of this fix.

Next decisive step: the new Windows stage job finishes installing WIM to ZSTD QCOW2, splits it into independently uploaded shards, Ubuntu assembles it with SHA256 verification, and only then Linux KVM boots Windows to OOBE, performs clean shutdown and second-boot verification. Never mark the finished ready disk as available before actual guest proof.

---

## 2026-10-29 latest continuation (active build status, do not claim OS is ready)

New checks and changes:
- [KVM full-hardware synthetic smoke #40908732098](https://github.com/Pokateli/Temp/actions/runs/40908732098) succeeded with unprivileged QEMU+KVM, 4M OVMF pflash, ZSTD QCOW2, writable virtual FAT, and a 921615-byte VGA screendump. HMP showed `kvm support: enabled`.
- COM1 PowerShell guest construction fixed to `[System.IO.Ports.SerialPort]::new(...)` and checked on real Windows runner. [marker preflight #40908496961](https://github.com/Pokateli/Temp/actions/runs/40908496961) passed.
- Pinned upstream Tiny11 maker optimized: copy **only** `sources/install.wim` instead of the entire ISO, then stop immediately after exported install.wim, before installer `boot.wim` editing or installer ISO creation. [Preflight #40909164315](https://github.com/Pokateli/Temp/actions/runs/40909164315) passed. This optimization applies to **future new builds**; currently running builds use their original checkout commit.
- The upstream installer-oriented `Windows/System32/Sysprep/autounattend.xml` is removed from the DISM-applied image to avoid competing with our authoritative `Windows/Panther/Unattend/Unattend.xml`, which creates `vmuser` and controls first OOBE. [marker preflight #40909403622](https://github.com/Pokateli/Temp/actions/runs/40909403622) passed.
- New KVM ready-disk workflow uploads diagnostic `firstboot-*.ppm` screenshots if the guest boot fails or succeeds. Changes to workflows caused separate fresh runs; do not conflate run SHAs.
- Active KVM ready-disk build [#40906341119](https://github.com/Pokateli/Temp/actions/runs/40906341119) and later [#40908895996](https://github.com/Pokateli/Temp/actions/runs/40908895996): **still on Windows WIM customization when last checked**. Neither ready-image KVM first OOBE nor second installed Windows boot has yet been confirmed.
- The public Temp authorization for plaintext development artifacts remains in effect only as stated by the user, with Microsoft licensing restrictions still applicable.

Next decisive gate: `ubuntu-kvm` job begins only after `windows-stage` uploads a SHA256-checked `tiny11-kvm-stage-plain` artifact. Look for `firstboot-serial.log`, `firstboot-*.ppm`, guest clean shutdown and then `probe-result.json` with `"boot_confirmed": true`. Do not call a synthetic test a real Windows boot.

---

# Update — accelerated public Tiny11 development pipeline (2026-10-29)

The user explicitly authorized **unencrypted Windows development disk artifacts**
in the public Temp repository, pending a future move to a private repository.
This authorization supersedes the earlier engineering choice that *all* artifacts
must be encrypted. The old encrypted workflows remain available as alternatives.
Microsoft evaluation licensing/redistribution terms remain separate.

## Current confirmed facts
- GitHub Ubuntu KVM works under sudo after /dev/kvm owner/permissions restrictions:
  [KVM run 40905059946](https://github.com/Pokateli/Temp/actions/runs/40905059946).
  Windows-runner WHPX failed with nested virtualization error 80370302.
- Previously produced installed (pre-OOBE) Windows QCOW2 was **7,905,869,824 bytes**,
  with ZSTD compressed extents and byte-identical VHDX comparison. First boot
  stalled at Getting ready and failed the 40 minute TCG gate:
  [failure run 40885221311](https://github.com/Pokateli/Temp/actions/runs/40885221311).
- Public unencrypted KVM firstboot pipeline:
  [run 40906341119](https://github.com/Pokateli/Temp/actions/runs/40906341119).
  **Its successful completion was not yet confirmed at this update.**
- Streamed 384-MiB shard roundtrip and cross-boundary test passed:
  [run 40906485624](https://github.com/Pokateli/Temp/actions/runs/40906485624).

## Workflow map
- `.github/workflows/tiny11-ready-kvm-public.yml`: **Windows WIM/DISM to a
  first-login stage QCOW2**, temporary plain development artifact with
  SHA256, transfer to Linux KVM, first OOBE and guest clean poweroff,
  ready ZSTD standalone QCOW2, second boot proof under KVM.
  After success, creates `tiny11-ready-public-index` and numbered
  `tiny11-ready-public-shard-000` onward (384 MiB each).
- `.github/workflows/tiny11-ready-public-run.yml`: reassemble output of an
  earlier successful build run from public shards on Linux KVM and prove
  a new installed Windows boot without reinstalling.
- `windows-vm-runtime/ready-parts.py`: streamed tar serialization,
  SHA256-checked 384 MiB unencrypted chunks and verified restoration.
- `windows-vm-runtime/finalize-ready-kvm.py`: Windows first OOBE in KVM, COM1/FAT
  ready proof and clean shutdown, flattened standalone ZSTD QCOW2 manifest.
- `windows-vm-runtime/bootstrap-chatgpt.sh`: accepts public ready shards
  from `PUBLIC_PARTS_DIR` (default `/mnt/data/tiny11-ready-public-parts`),
  or the old AES-GCM bundle. QEMU remains TCG in ChatGPT unless KVM available.
  Compact QEMU_HOME may point to `/mnt/data/tiny11-qemu-runtime-linux-amd64`.

The temporary public stage artifact retains Windows account credentials,
so **do not treat it as secret**. Guest VM network is disabled by launch scripts.
Converting a repo to private does not guarantee revocation of previously
downloaded public artifacts. The final ready disk/desktop is NOT confirmed
until a successful `probe-result.json` with `boot_confirmed: true` is obtained.

---

# Tiny11 dual-run project — verified handoff state

Project repo: `Pokateli/Temp`, feature branch `windows-tiny11-dual-ready-20261029`. Do not merge to `main` until tested.

## User goal

A **single pre-installed Windows 11 Tiny11 virtual disk** with completed first-login OOBE and a clean shutdown, compressed with QCOW2/Zstandard. It must start without reinstalling Windows in BOTH GitHub Windows Actions and the ChatGPT Linux QEMU TCG sandbox.

## Grounded results

- **Windows ISO → Tiny11 WIM → installed UEFI VHDX → real compressed QCOW2:** confirmed in [run 40866329555](https://github.com/Pokateli/Temp/actions/runs/40866329555). The **pre-first-login** compressed disk was about 7.9 GB. This is **not** the final ready image.
- **Windows screenshot on TCG reached Starting services / Getting ready:** [run 40869976476](https://github.com/Pokateli/Temp/actions/runs/40869976476). Fully completed OOBE/desktop **not confirmed** by that run.
- **Linux-ready runtime, SHA-256, separate writable QCOW2 session, negative guest-ready probe and AES-256-GCM encrypted transfer:** [run 40890543506](https://github.com/Pokateli/Temp/actions/runs/40890543506) success.
- **Windows-runner ready-disk syntax and OOBE XML:** [run 40886168895](https://github.com/Pokateli/Temp/actions/runs/40886168895) success.
- **Workflow YAML, Bash/Python syntax:** [run 40890543510](https://github.com/Pokateli/Temp/actions/runs/40890543510) success.
- **Windows boot-marker batch executed on native Windows:** [run 40890789408](https://github.com/Pokateli/Temp/actions/runs/40890789408) success.

## Active/future checks

1. [Run 40885221311](https://github.com/Pokateli/Temp/actions/runs/40885221311) — prepare-once ready disk. At the last check it was processing the Tiny11 WIM after the official SHA-256 validated ISO download.
2. Once the WIM succeeds, Stage 3 applies Windows to GPT VHDX. Stage 4 uses `finalize-ready.ps1` to enter OOBE, prove the first local `vmuser` logon, shut down Windows normally, and create standalone `tiny11-ready-zstd.qcow2`.
3. Stage 5 tests a second start from that **standalone** QCOW2 using `dual-run.py probe`, requiring an in-guest startup marker; QEMU process running alone is not proof.
4. The ready build's **encrypted** artifact is emitted only when GitHub repository Actions secret `TINY11_ARCHIVE_PASSPHRASE` is set; absent this secret, no Windows disk is uploaded. The default artifact retention currently is 14 days.
5. The executable GitHub reboot workflow is `.github/workflows/tiny11-ready-run.yml` (repaired after a truncated YAML error). It can be triggered with a committed `windows-vm-runtime/github-run-request.json` containing the successful build run ID, or manually when GitHub exposes the workflow.
6. For the ChatGPT sandbox, decrypt the same artifact into `/mnt/data/tiny11-ready` and execute `windows-vm-runtime/bootstrap-chatgpt.sh start`. An isolated synthetic portable disk and native ChatGPT QEMU/UEFI startup were verified; **the actual Windows ready disk has not yet been imported into ChatGPT**.

## Important restrictions

- `Temp` is **public**. Never commit or upload raw Windows ISO/WIM/QCOW2, unencrypted account credentials, or encryption passphrases.
- Encryption of public artifacts does not itself grant Windows redistribution rights; ensure license-compliant storage/delivery.
- Current ready-disk size is **unknown until the final first-login preparation finishes**.
- Do not claim that a bootable QCOW2 with an incomplete OOBE is the requested finished VM.
- Keep base QCOW2 immutable. Writable session overlay should be separate.
