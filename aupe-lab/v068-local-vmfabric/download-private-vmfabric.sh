#!/usr/bin/env bash
# Run ONLY in an authorized local shell that has network access, a GH_TOKEN or gh auth,
# and the gh GitHub CLI installed. This does not start a GitHub Actions job.
set -Eeuo pipefail
ROOT="${1:-/mnt/data/aupe068-vmfabric-local/downloads}"
command -v gh >/dev/null || { echo 'gh CLI is not installed' >&2; exit 40; }
mkdir -p "$ROOT"
if [[ -e "$ROOT/debian13-qemu-bundle.tar.gz" ]]; then
  echo 'Existing bundle present: no overwrite' >&2
else
  gh release download 'debian13-v1.2.0-fabric' \
    --repo 'Pokateli/VM-Fabric' \
    --pattern 'debian13-qemu-bundle.tar.gz' \
    --pattern 'debian13-qemu-bundle.tar.gz.sha256' \
    --dir "$ROOT" --clobber
fi
EXPECTED=b711d8f33bb5cde476cb31fff7f44e13260ed3ad8ca86b1f968876cd4740697f
ACTUAL="$(sha256sum "$ROOT/debian13-qemu-bundle.tar.gz" | cut -d' ' -f1)"
[[ "$ACTUAL" == "$EXPECTED" ]] || { echo 'Official VM-Fabric Release SHA256 mismatch' >&2; exit 41; }
echo "Verified VM-Fabric tarball: $ROOT/debian13-qemu-bundle.tar.gz"
