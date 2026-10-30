#!/usr/bin/env bash
# Build the portable QEMU TCG runtime only. No Windows, Debian disk or user data.
# Linux x86_64 runtime compatible with the ChatGPT sandbox.
set -Eeuo pipefail
for tool in docker tar sha256sum; do
  command -v "$tool" >/dev/null || { echo "Missing $tool" >&2; exit 1; }
done
[ "$(uname -m)" = "x86_64" ] || { echo "Builder must be Linux x86_64" >&2; exit 1; }
OUT="${OUTPUT_DIR:-$PWD/output}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
DEST="$OUT/tiny11-qemu-runtime-linux-amd64"
[ ! -e "$DEST" ] || { echo "Destination already exists: $DEST" >&2; exit 1; }
mkdir -p "$DEST"/{bin,lib,share/qemu}

docker run --rm --platform linux/amd64 \
  -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
  -v "$DEST:/out" debian:13 bash -euo pipefail -c '
  export DEBIAN_FRONTEND=noninteractive
  apt-get update -qq
  apt-get install -y -qq --no-install-recommends qemu-system-x86 qemu-utils seabios ca-certificates
  for name in qemu-system-x86_64 qemu-img; do
    src="/usr/bin/$name"
    test -x "$src"
    cp -L "$src" "/out/bin/$name"
    ldd "$src" |
      awk '\''$1 ~ /^\// {print $1} $3 ~ /^\// {print $3}'\'' |
      sort -u | while IFS= read -r lib; do
        cp -L "$lib" "/out/lib/$(basename "$lib")"
      done
  done
  cp -L /lib64/ld-linux-x86-64.so.2 /out/lib/ld-linux-x86-64.so.2
  cp -aL /usr/share/qemu/. /out/share/qemu/
  # VGA option ROMs are packaged separately in Debian seabios. Without
  # these QEMU cannot start a Windows guest with -vga std at all.
  cp -aL /usr/share/seabios/vgabios-*.bin /out/share/qemu/
  test -s /out/share/qemu/vgabios-stdvga.bin
  test -s /out/share/qemu/vgabios-bochs-display.bin
  /out/lib/ld-linux-x86-64.so.2 --library-path /out/lib /out/bin/qemu-img --version
  /out/lib/ld-linux-x86-64.so.2 --library-path /out/lib /out/bin/qemu-system-x86_64 -L /out/share/qemu -machine help >/dev/null
  chown -R "$HOST_UID:$HOST_GID" /out
'
cat > "$DEST/README.md" <<'README'
# Tiny11 portable QEMU runtime (Linux x86_64)
This is ONLY QEMU binaries, libraries and firmware data; **no operating-system disk**.
Set QEMU_HOME to this extracted directory when running windows-vm-runtime/dual-run.py.
QEMU uses TCG and does not need /dev/kvm. The preinstalled Tiny11 QCOW2 and
OVMF_CODE.fd/OVMF_VARS.fd must be obtained separately.
Guest network is disabled by the Tiny11 dual-run launcher.
README
(
  cd "$DEST"
  sha256sum bin/* lib/* > SHA256SUMS
)
tar -czf "$OUT/tiny11-qemu-runtime-linux-amd64.tar.gz" -C "$OUT" "$(basename "$DEST")"
sha256sum "$OUT/tiny11-qemu-runtime-linux-amd64.tar.gz" |
  tee "$OUT/tiny11-qemu-runtime-linux-amd64.tar.gz.sha256"
