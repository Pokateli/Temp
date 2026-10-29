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
