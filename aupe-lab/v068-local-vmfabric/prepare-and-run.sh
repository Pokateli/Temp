#!/usr/bin/env bash
set -Eeuo pipefail
if (( $# < 2 || $# > 3 )); then
  echo "Usage: $0 <verified Debian13 VM-Fabric ZIP/TAR.GZ> <vm_smoke executable> [workdir]" >&2
  exit 2
fi
ARCHIVE="$1"
RUST_BIN="$2"
WORK="${3:-/mnt/data/aupe068-vmfabric-local/work}"
[[ -f "$ARCHIVE" ]] || { echo "NOT_AVAILABLE: local VM archive not transferred from GitHub: $ARCHIVE" >&2;exit 20; }
[[ -x "$RUST_BIN" ]] || { echo "NOT_AVAILABLE: local Rust vm_smoke executable missing: $RUST_BIN" >&2;exit 21; }
[[ -f /mnt/data/AUPE_v0.5.68_rust_full_port/vm/guest-test.sh ]] || exit 22
mkdir -p "$WORK"
python3 - "$ARCHIVE" "$WORK" <<'PY'
import hashlib, os, sys, tarfile, zipfile
from pathlib import Path
archive, work = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
work.mkdir(parents=True, exist_ok=True)
staging=work/'stage'
if staging.exists():
    raise SystemExit('Refusing to overwrite existing VM staging directory')
staging.mkdir()
try:
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as z:
            for item in z.infolist():
                p=Path(item.filename)
                if p.is_absolute() or '..' in p.parts or item.file_size>8*1024**3:
                    raise SystemExit('Unsafe ZIP member: '+item.filename)
            for name in ('debian13-qemu-bundle.tar.gz','debian13-qemu-bundle.tar.gz.sha256'):
                if name in z.namelist():
                    (staging/name).write_bytes(z.read(name))
        inner=staging/'debian13-qemu-bundle.tar.gz'
        if not inner.exists(): raise SystemExit('No Debian VM bundle in ZIP')
    else:
        inner=archive
    # Some historical archived SHA files contain absolute runner paths; trust only the hex digest.
    checksum=staging/(inner.name+'.sha256') if inner.parent==staging else Path(str(inner)+'.sha256')
    if checksum.is_file():
        expected=checksum.read_text().split()[0].lower()
        actual=hashlib.file_digest(inner.open('rb'), 'sha256').hexdigest()
        if not len(expected)==64 or expected!=actual:
            raise SystemExit('Bundle SHA256 mismatch')
        print('VM_BUNDLE_SHA256=PASS',actual,flush=True)
    else:
        # Pinned by the connected GitHub release metadata for VM-Fabric/debian13-v1.2.0-fabric.
        pinned='b711d8f33bb5cde476cb31fff7f44e13260ed3ad8ca86b1f968876cd4740697f'
        if inner.name!='debian13-qemu-bundle.tar.gz' or inner.stat().st_size!=427155458:
            raise SystemExit('No detached SHA256 and bundle does not match known VM-Fabric release')
        actual=hashlib.file_digest(inner.open('rb'),'sha256').hexdigest()
        if actual!=pinned:
            raise SystemExit('Pinned GitHub Release SHA256 mismatch')
        print('VM_BUNDLE_GITHUB_PINNED_SHA256=PASS',actual,flush=True)
    with tarfile.open(inner,'r:gz') as z:
        members=z.getmembers()
        for m in members:
            path=Path(m.name)
            if path.is_absolute() or '..' in path.parts or not (path.parts and path.parts[0]=='debian13-qemu-bundle'):
                raise SystemExit('Unsafe TAR member '+m.name)
            if not (m.isfile() or m.isdir()):
                # Do not allow symlink/hardlink escapes; this bundle is expected to have plain files only.
                raise SystemExit('Unsupported TAR member '+m.name)
        z.extractall(staging,filter='data')
    base=staging/'debian13-qemu-bundle'
    if not (base/'start.sh').is_file() or not (base/'debian13.qcow2').is_file():
        raise SystemExit('Incomplete QEMU bundle')
    if (base/'DEBIAN_IMAGE_SHA512SUMS').is_file():
        s=(base/'DEBIAN_IMAGE_SHA512SUMS').read_text().strip().split()
        if len(s)<2 or s[1]!='debian13.qcow2':raise SystemExit('Bad image checksum metadata')
        got=hashlib.file_digest((base/'debian13.qcow2').open('rb'),'sha512').hexdigest()
        if got!=s[0]:raise SystemExit('QCOW2 SHA512 mismatch')
        print('VM_QCOW2_SHA512=PASS',flush=True)
    print('VM_STAGED='+str(base),flush=True)
except Exception:
    raise
PY
BUNDLE="$WORK/stage/debian13-qemu-bundle"
# QEMU is portable in the validated VM-Fabric bundle.
if [[ ! -x "$BUNDLE/bin/qemu-system-x86_64" || ! -x "$BUNDLE/bin/qemu-img" || ! -x "$BUNDLE/lib/ld-linux-x86-64.so.2" ]]; then
  echo 'NOT_AVAILABLE: portable QEMU missing from bundle' >&2;exit 23
fi
# Use existing AUPE launcher (makes isolated overlay + disabled guest network).
bash /mnt/data/AUPE_0.5.68_local_vm_validation/run-local-vm.sh --bundle "$BUNDLE" --rust-bin "$RUST_BIN" --workdir "$WORK/run"
