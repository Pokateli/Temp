#!/usr/bin/env bash
# Build portable Debian 13 amd64 QEMU/TCG VM (no /dev/kvm at runtime).
set -Eeuo pipefail
for cmd in docker tar id sha256sum; do
  command -v "$cmd" >/dev/null || { echo "Missing: $cmd" >&2; exit 1; }
done
[ "$(uname -m)" = "x86_64" ] || { echo "Build on x86_64" >&2; exit 1; }
OUT="${OUTPUT_DIR:-$PWD}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
DEST="$OUT/debian13-qemu-bundle"
ARCHIVE="$OUT/debian13-qemu-bundle.tar.gz"
[ ! -e "$DEST" ] || { echo "Destination exists: $DEST" >&2; exit 1; }
mkdir -p "$DEST"/{bin,lib,share/qemu,sharefiles}
echo "[1/4] Bundle Debian 13 QEMU and nocloud disk"
docker run --rm --platform linux/amd64 \
  -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
  -v "$DEST:/out" debian:13 bash -euo pipefail -c '
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y -qq --no-install-recommends \
      qemu-system-x86 qemu-utils ca-certificates curl
    for name in qemu-system-x86_64 qemu-img; do
      src="/usr/bin/$name"
      test -x "$src"
      cp -L "$src" "/out/bin/$name"
      ldd "$src" |
        awk '\''$1 ~ /^\// {print $1} $3 ~ /^\// {print $3}'\'' |
        sort -u | while IFS= read -r library; do
          cp -L "$library" "/out/lib/$(basename "$library")"
        done
    done
    cp -L /lib64/ld-linux-x86-64.so.2 /out/lib/ld-linux-x86-64.so.2
    cp -aL /usr/share/qemu/. /out/share/qemu/
    base="https://cloud.debian.org/images/cloud/trixie/latest"
    image="debian-13-nocloud-amd64.qcow2"
    curl -fLS --retry 5 --retry-delay 3 "$base/$image" -o /out/debian13.qcow2
    curl -fLS --retry 5 --retry-delay 3 "$base/SHA512SUMS" -o /tmp/SHA512SUMS
    expected="$(awk '\''$2 ~ /(^|\/)debian-13-nocloud-amd64\.qcow2$/ {print $1; exit}'\'' /tmp/SHA512SUMS)"
    actual="$(sha512sum /out/debian13.qcow2 | awk '\''{print $1}'\'')"
    echo "Checksum expected: $expected"; echo "Checksum actual: $actual"
    [ -n "$expected" ] && [ "$expected" = "$actual" ] || {
      echo "Debian image SHA512 verification FAILED" >&2; exit 1;
    }
    printf "%s  debian13.qcow2\n" "$actual" > /out/DEBIAN_IMAGE_SHA512SUMS
    echo "Bundled firmware entries:"; ls -l /out/share/qemu/ | head -n 30
    if [ ! -e /out/share/qemu/bios-256k.bin ] && [ ! -e /out/share/qemu/bios.bin ]; then
      echo "Copy SeaBIOS firmware from Debian package"
      cp -aL /usr/share/seabios/. /out/share/qemu/
    fi
    /out/lib/ld-linux-x86-64.so.2 --library-path /out/lib \
      /out/bin/qemu-system-x86_64 --version
    /out/lib/ld-linux-x86-64.so.2 --library-path /out/lib \
      /out/bin/qemu-system-x86_64 -L /out/share/qemu -machine help >/dev/null
    chown -R "$HOST_UID:$HOST_GID" /out
  '
cat > "$DEST/start.sh" <<'START'
#!/usr/bin/env bash
set -Eeuo pipefail
BASE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
[ "$(uname -m)" = "x86_64" ] || { echo "Linux x86_64 required" >&2; exit 1; }
exec "$BASE/lib/ld-linux-x86-64.so.2" --library-path "$BASE/lib" \
  "$BASE/bin/qemu-system-x86_64" \
  -L "$BASE/share/qemu" \
  -machine pc,accel=tcg -cpu max \
  -smp 2 -m 2048 -nographic -nic none \
  -drive "file=$BASE/debian13.qcow2,format=qcow2,if=virtio" \
  -drive "file=fat:ro:$BASE/sharefiles,format=raw,if=virtio"
START
chmod +x "$DEST/start.sh"
cat > "$DEST/sharefiles/README.txt" <<'SHARE'
Put compiled .ko modules, scripts or source files in this folder before starting.
Guest sees a read-only FAT drive, normally /dev/vdb or /dev/vdb1.
In Debian 13 guest:
  uname -a
  lsblk -f
  mkdir -p /mnt/shared
  mount -o ro /dev/vdb /mnt/shared || mount -o ro /dev/vdb1 /mnt/shared
  ls -la /mnt/shared
  insmod /mnt/shared/your_module.ko
  dmesg | tail -n 60
  rmmod your_module
Build any module against the EXACT kernel running inside QEMU.
Guest network is disabled. Kernel headers/compiler are not included by default.
SHARE
cat > "$DEST/README.md" <<'README'
# Portable Debian 13 QEMU TCG VM
- Platform: Linux x86_64. Uses TCG emulation; no /dev/kvm needed.
- Debian disk: official Debian 13 nocloud amd64 qcow2, SHA512 verified.
- Runtime components: bin/, lib/, share/qemu/.
- Run: ./start.sh (2 vCPU, 2048 MiB, no guest network).
- Default login: Debian nocloud presents an automatic root shell.
- To transfer files: add them to sharefiles before starting. Mount /dev/vdb or /dev/vdb1.
- Exit QEMU: Ctrl+A followed by X.
- Kernel module version MUST match guest kernel, not host container kernel.
- Never expose passwordless root to an untrusted network.

Debian cloud images: https://cloud.debian.org/images/cloud/trixie/latest/
README
if [ -n "${1:-}" ]; then
  [ -e "$1" ] || { echo "Missing sharefile source: $1" >&2; exit 1; }
  if [ -d "$1" ]; then cp -a "$1"/. "$DEST/sharefiles/"; else cp -a "$1" "$DEST/sharefiles/"; fi
fi
echo "[2/4] Save initial file checksums"
(cd "$DEST" && sha256sum start.sh bin/* lib/* > SHA256SUMS)
echo "[3/4] Archive"
tar -czf "$ARCHIVE" -C "$OUT" "$(basename "$DEST")"
echo "[4/4] Complete"
sha256sum "$ARCHIVE" | tee "$ARCHIVE.sha256"
