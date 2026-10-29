# Tiny11 QEMU-TCG sandbox runtime (experimental)

This branch tests **real** qcow2 Zstandard compressed clusters and a Windows 11 Tiny11 base image. No Windows OS binaries are committed to Temp.

## Confirmed components
- GitHub Actions QEMU+WinPE TCG guest boot test: https://github.com/Pokateli/Temp/actions/runs/40859157565
- ZSTD qcow2 roundtrip on Ubuntu runner: https://github.com/Pokateli/Temp/actions/runs/40861790121
- OVMF firmware generated from Linux packages, verified in sandbox.
- Local QEMU runtime with independent writable qcow2 overlay validated using a synthetic disk. This does **not** imply that Tiny11 guest boot succeeded.

## Required runtime layout
\`\`\`text
/mnt/data/tiny11-lab/
    tiny11-zstd.qcow2            <- compressed base image, read-only by convention
    OVMF_CODE.fd                 <- from tiny11-ovmf-uefi-firmware artifact
    OVMF_VARS.fd
    sharefiles/                  <- FAT rw guest exchange (create if absent)
    tiny11-session.qcow2         <- created automatically; guest writes here
    OVMF_VARS-session.fd         <- copied automatically; guest changes go here
/mnt/data/debian13-chatgpt-vm/
    bin/qemu-system-x86_64, bin/qemu-img, lib/..., share/qemu/...
\`\`\`

## Start and diagnose
\`\`\`bash
QEMU_HOME=/mnt/data/debian13-chatgpt-vm \
  bash windows-vm-runtime/start-tiny11.sh /mnt/data/tiny11-lab
\`\`\`
Video: VNC loopback on port 5900; HMP monitor: \`/mnt/data/tiny11-lab/monitor.sock\`. No public VNC listener. External networking is off.

Test QEMU runtime & take screenshot headlessly:
\`\`\`bash
QEMU_HOME=/mnt/data/debian13-chatgpt-vm \
  python3 windows-vm-runtime/probe-tiny11.py /mnt/data/tiny11-lab --duration 90
\`\`\`
The probe writes \`probe-console.log\`, \`probe-screen.ppm\` and \`probe-result.json\`. A QEMU or UEFI screenshot is **not evidence** Windows finished booting. Use an in-guest marker, serial output, or agent to verify Windows.

## QCOW2 compression
Offline conversion on Windows runner:
\`\`\`bash
qemu-img convert -p -c -f vhdx -O qcow2 \
   -o compat=1.1,compression_type=zstd tiny11.vhdx tiny11-zstd.qcow2
\`\`\`
Important: compressed base is **not** a read/write compressed disk. Writable overlay is an ordinary QCOW2, with base preserved and compressed allocated clusters. \`qemu-img map --output=json\` proves compressed extents; \`qemu-img compare\` can verify the contents.

## Legal/operational limits
- Only use a Microsoft OS image that you are licensed or authorized to use. Enterprise evaluation images are time-limited.
- This public Temp branch does not upload Windows ISOs or installed Windows images.
- The ChatGPT sandbox lacks KVM and has 4 GiB RAM in the tested environment, so normal Windows 11 is not guaranteed to be usable there.
- WinPE boot on GitHub was confirmed; Tiny11 **installation/guest boot is not established** until the dedicated build and boot tests succeed.
