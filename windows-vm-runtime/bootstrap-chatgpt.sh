#!/usr/bin/env bash
# ChatGPT sandbox boot of the same preinstalled Tiny11 image produced by GitHub.
# No root, no KVM, no Windows installation. Local VNC 127.0.0.1:5900 only.
set -Eeuo pipefail
RUNTIME="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
READY_DIR="${READY_DIR:-/mnt/data/tiny11-ready}"
ENCRYPTED_DIR="${ENCRYPTED_DIR:-/mnt/data/tiny11-ready-encrypted}"
ACTION="${1:-verify}"
QEMU_HOME="${QEMU_HOME:-/mnt/data/debian13-chatgpt-vm}"
if [ ! -f "$READY_DIR/tiny11-ready-manifest.json" ]; then
  if [ ! -f "$ENCRYPTED_DIR/envelope.json" ]; then
    echo "No ready disk found. Supply encrypted GitHub artifact parts into $ENCRYPTED_DIR" >&2
    echo "Expected source: Temp Actions artifact tiny11-ready-encrypted-v1 (requires repo passphrase secret)" >&2
    exit 2
  fi
  echo "Decrypting AES-256-GCM authenticated ready disk into $READY_DIR"
  python3 "$RUNTIME/secure-bundle.py" decrypt "$ENCRYPTED_DIR" "$READY_DIR"
fi
echo "Checking the installed-Windows QCOW2, UEFI, and SHA-256 manifest..."
python3 "$RUNTIME/dual-run.py" verify --vm-dir "$READY_DIR" --qemu-home "$QEMU_HOME"
case "$ACTION" in
  verify)
    echo "Verified: Windows is already installed. Usage: bash $0 start | probe"
    ;;
  start)
    exec python3 "$RUNTIME/dual-run.py" start --skip-hash --vm-dir "$READY_DIR" --qemu-home "$QEMU_HOME"
    ;;
  probe)
    exec python3 "$RUNTIME/dual-run.py" probe --skip-hash --headless --seconds 1200 --vm-dir "$READY_DIR" --qemu-home "$QEMU_HOME"
    ;;
  reset-session)
    # Explicit reset: only session changes are removed; compressed base remains unchanged.
    exec python3 "$RUNTIME/dual-run.py" start --skip-hash --reset-session --vm-dir "$READY_DIR" --qemu-home "$QEMU_HOME"
    ;;
  *)
    echo "Usage: $0 [verify|start|probe|reset-session]" >&2
    exit 2;;
esac
