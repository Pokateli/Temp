#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd -- "$(dirname -- "$0")/.." && pwd)"
TEMP="$(mktemp -d)"
trap 'rm -rf "$TEMP"' EXIT
URL='https://github.com/Pokateli/VM-Fabric/releases/download/debian13-v1.2.0-fabric/debian13-qemu-bundle.tar.gz'
curl --fail --retry 3 -L "$URL" -o "$TEMP/debian.tar.gz"
tar -xzf "$TEMP/debian.tar.gz" -C "$TEMP"
BUNDLE="$(find "$TEMP" -type f -name start.sh -print -quit | xargs -r dirname)"
test -n "$BUNDLE" && test -f "$BUNDLE/start.sh" && test -d "$BUNDLE/sharefiles"
cp "$HERE/build/cargo/release/vm_smoke" "$BUNDLE/sharefiles/aupe68_vm_smoke"
# VM-Fabric guest-exec uses serial root console; guest has no outside network.
curl --fail --retry 3 -L https://raw.githubusercontent.com/Pokateli/VM-Fabric/main/profiles/debian13/scripts/guest-exec.py -o "$TEMP/guest-exec.py"
python3 "$TEMP/guest-exec.py" --bundle "$BUNDLE" --script "$HERE/vm/guest-test.sh" \
    --log "$HERE/build/vm-console.log" --output "$HERE/build/vm-output.log" --timeout 720
grep -q 'AUPE68_DEBIAN_VM_RESULT=PASS' "$HERE/build/vm-output.log"
