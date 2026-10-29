# Windows QEMU sandbox feasibility test

Experimental Windows VM validation is isolated in branch \`windows-qemu-lab-20261029\` and does **not** change the \`main\` branch.

## Purpose
1. Download the official Microsoft Windows ADK and WinPE add-on on an ephemeral Windows GitHub Actions runner.
2. Create a bootable Windows PE ISO with an automatic, verifiable guest-side readiness marker.
3. Boot the ISO on an Ubuntu GitHub Actions runner with QEMU **TCG** (software emulation, without KVM).
4. Detect a guest-written marker on a FAT disk, and publish test logs, **not the Microsoft ISO**.

This validates a Windows kernel/WinPE guest running on QEMU without CPU virtualization. **Windows PE is not a complete Windows 11 desktop installation**. Full Windows 11 needs more storage, RAM, installation time, and lawful OS licensing. Official ISO: https://www.microsoft.com/software-download/windows11

### Limitations
- Windows PE is intended for installation, recovery, and deployment, not as a general-purpose desktop OS.
- Test ISO is a short-lived GitHub Actions handoff (retention 1 day); no Microsoft proprietary binaries should be committed to this public repository or uploaded as public permanent releases.
- Even a successful test on GitHub Actions does not prove that the ChatGPT local sandbox has sufficient RAM/CPU/disk for a fully-installed Windows 11 guest.
- QEMU's `-accel tcg` is deliberately required so we test the sandbox-compatible emulation path.
