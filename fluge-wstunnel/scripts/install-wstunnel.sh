#!/usr/bin/env bash
set -euo pipefail

VERSION="11.1.0"
DEST="${1:-$HOME/.local/bin}"
ARCH="$(uname -m)"

case "$ARCH" in
  x86_64|amd64)
    TARGET="linux_amd64"
    EXPECTED="512edf4e7bb64fc133522220123514868fae75b5bae4124b53e396787276aa23"
    ;;
  aarch64|arm64)
    TARGET="linux_arm64"
    EXPECTED="148895f957a68be39e2b65d28c5f2065b9c58c4a0cfd48c560b2b3267f398464"
    ;;
  *)
    echo "Unsupported architecture: $ARCH" >&2
    exit 2
    ;;
esac

for cmd in curl sha256sum tar find install mktemp; do
  command -v "$cmd" >/dev/null || { echo "Missing dependency: $cmd" >&2; exit 2; }
done

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
ARCHIVE="wstunnel_$VERSION\_$TARGET.tar.gz"
URL="https://github.com/erebe/wstunnel/releases/download/v$VERSION/$ARCHIVE"

echo "Downloading $URL"
curl --fail --location --silent --show-error --retry 3 --proto '=https' --proto-redir '=https' \
  --output "$TMP/archive.tar.gz" "$URL"

printf '%s  %s\n' "$EXPECTED" "$TMP/archive.tar.gz" | sha256sum --check --status || {
  echo "SHA256 mismatch: refusing to install" >&2
  exit 1
}

mkdir -p "$TMP/extract" "$DEST"
tar -xzf "$TMP/archive.tar.gz" -C "$TMP/extract"
BIN="$(find "$TMP/extract" -type f -name wstunnel -print -quit)"
if [[ -z "$BIN" ]]; then
  echo "Missing wstunnel executable in official archive" >&2
  exit 1
fi

install -m 0755 "$BIN" "$DEST/wstunnel"
echo "Installed: $DEST/wstunnel"
"$DEST/wstunnel" --version
