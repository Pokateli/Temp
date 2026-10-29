#!/usr/bin/env bash
# ChatGPT sandbox boot of the same preinstalled Tiny11 image produced by GitHub.
# No root, no KVM, no Windows installation. Local VNC 127.0.0.1:5900 only.
set -Eeuo pipefail
RUNTIME="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
READY_DIR="${READY_DIR:-/mnt/data/tiny11-ready}"
ENCRYPTED_DIR="${ENCRYPTED_DIR:-/mnt/data/tiny11-ready-encrypted}"
ACTION="${1:-verify}"
if [ -z "${QEMU_HOME:-}" ]; then
  for candidate in \
    /mnt/data/debian13-chatgpt-vm \
    /mnt/data/debian13-qemu-bundle \
    /mnt/data/tiny11-local-sandbox/debian13-qemu-bundle \
    /mnt/data/tiny11-portable-qemu/debian13-qemu-bundle; do
    if [ -x "$candidate/bin/qemu-system-x86_64" ] && [ -x "$candidate/bin/qemu-img" ]; then
      QEMU_HOME="$candidate"
      break
    fi
  done
  if [ -z "${QEMU_HOME:-}" ]; then
    portable_zip=/mnt/data/debian13-qemu-tcg-bundle.zip
    if [ ! -f "$portable_zip" ]; then
      echo "Portable QEMU kit missing: $portable_zip. Set QEMU_HOME to an extracted compatible portable kit." >&2
      exit 2
    fi
    echo "Preparing portable QEMU TCG from existing Debian13 kit (no system installation)"
    mkdir -p /mnt/data/tiny11-portable-qemu
    unzip -p "$portable_zip" debian13-qemu-bundle.tar.gz | tar -xz -C /mnt/data/tiny11-portable-qemu
    QEMU_HOME=/mnt/data/tiny11-portable-qemu/debian13-qemu-bundle
  fi
fi
export QEMU_HOME
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
