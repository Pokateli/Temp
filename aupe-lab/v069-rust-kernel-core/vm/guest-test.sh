#!/usr/bin/env bash
set -euo pipefail
echo "GUEST_OS=$(cat /etc/debian_version)"
echo "GUEST_KERNEL=$(uname -r)"
if [ -e "/boot/config-$(uname -r)" ]; then
  grep -E '^CONFIG_RUST=|^# CONFIG_RUST is not set' "/boot/config-$(uname -r)" || true
fi
cp /mnt/dual-vm-run/aupe68_vm_smoke /tmp/aupe68_vm_smoke
chmod 0755 /tmp/aupe68_vm_smoke
/tmp/aupe68_vm_smoke
echo "AUPE68_DEBIAN_VM_RESULT=PASS"
