#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd -- "$(dirname -- "$0")/.." && pwd)"
TEMP="$(mktemp -d)"
trap 'rm -rf "$TEMP"' EXIT
URL='https://github.com/Pokateli/Temp/releases/download/actions-artifacts-archive-2026-10/run-40843031312-artifact-12883472746-debian13-qemu-tcg-bundle.zip'
echo "VM bundle source: public Temp Release (not the private VM-Fabric URL)"
curl --fail --show-error --retry 3 -L "$URL" -o "$TEMP/debian.zip"
file "$TEMP/debian.zip"
unzip -Z1 "$TEMP/debian.zip" | head -40
unzip -q "$TEMP/debian.zip" -d "$TEMP/extracted"
expected="$(awk 'NR==1 {print $1}' "$TEMP/extracted/debian13-qemu-bundle.tar.gz.sha256")"
actual="$(sha256sum "$TEMP/extracted/debian13-qemu-bundle.tar.gz" | cut -d' ' -f1)"
[ "$expected" = "$actual" ] || { echo "VM bundle SHA256 mismatch" >&2; exit 22; }
echo "Nested Debian13 bundle SHA256: verified ($actual)"
tar -xzf "$TEMP/extracted/debian13-qemu-bundle.tar.gz" -C "$TEMP/extracted"
BUNDLE="$(find "$TEMP/extracted" -type f -name start.sh -print -quit | xargs -r dirname)"
if [ -z "$BUNDLE" ] || [ ! -d "$BUNDLE/sharefiles" ]; then
  echo "VM bundle does not have start.sh + sharefiles; contents:" >&2
  find "$TEMP/extracted" -maxdepth 4 -type f | head -70 >&2
  exit 21
fi
cp "$HERE/build/cargo/release/vm_smoke" "$BUNDLE/sharefiles/aupe68_vm_smoke"
python3 "$HERE/vm/guest-exec.py" "$BUNDLE" "$HERE/vm/guest-test.sh" "$HERE/build/vm-console.log"