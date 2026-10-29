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
