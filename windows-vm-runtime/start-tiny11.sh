#!/usr/bin/env bash
# Launch a compressed Tiny11 QCOW2 via portable Debian 13 QEMU TCG + OVMF.
# This launcher is experimental until the Tiny11 QCOW2 build and Windows boot are validated.
set -Eeuo pipefail
BASE="${1:-/mnt/data/tiny11-lab}"
QEMU_HOME="${QEMU_HOME:-/mnt/data/debian13-chatgpt-vm}"
if [[ "$BASE" != /* ]];then BASE="$PWD/$BASE";fi
if [[ "$QEMU_HOME" != /* ]];then QEMU_HOME="$PWD/$QEMU_HOME";fi
[ -f "$BASE/tiny11-zstd.qcow2" ] || { echo "Missing compressed base QCOW2" >&2;exit 1; }
[ -f "$BASE/OVMF_CODE.fd" ] || { echo "Missing OVMF_CODE.fd" >&2;exit 1; }
[ -f "$BASE/OVMF_VARS.fd" ] || { echo "Missing OVMF_VARS.fd" >&2;exit 1; }
[ -x "$QEMU_HOME/bin/qemu-system-x86_64" ] || { echo "Missing portable Debian 13 QEMU" >&2;exit 1; }
[ -x "$QEMU_HOME/bin/qemu-img" ] || { echo "Missing portable qemu-img" >&2;exit 1; }
runqemu(){
  "$QEMU_HOME/lib/ld-linux-x86-64.so.2" --library-path "$QEMU_HOME/lib" "$@"
}
[ -f "$BASE/tiny11-session.qcow2" ] || runqemu "$QEMU_HOME/bin/qemu-img" create \
  -f qcow2 -F qcow2 -b "$BASE/tiny11-zstd.qcow2" "$BASE/tiny11-session.qcow2"
[ -f "$BASE/OVMF_VARS-session.fd" ] || cp "$BASE/OVMF_VARS.fd" "$BASE/OVMF_VARS-session.fd"
mkdir -p "$BASE/sharefiles"
echo "Starting Tiny11/TCG; VNC 127.0.0.1:5900 and HMP monitor $BASE/monitor.sock"
echo "This does not provide external networking."
exec "$QEMU_HOME/lib/ld-linux-x86-64.so.2" --library-path "$QEMU_HOME/lib" \
  "$QEMU_HOME/bin/qemu-system-x86_64" \
  -L "$QEMU_HOME/share/qemu" \
  -machine pc,accel=tcg -cpu max -smp 2 -m 2560 \
  -drive "if=pflash,format=raw,readonly=on,file=$BASE/OVMF_CODE.fd" \
  -drive "if=pflash,format=raw,file=$BASE/OVMF_VARS-session.fd" \
  -drive "if=ide,format=qcow2,file=$BASE/tiny11-session.qcow2" \
  -drive "file=fat:ro:$BASE/sharefiles,if=ide,format=raw,readonly=on" \
  -nic none -vga std -display vnc=127.0.0.1:0 \
  -monitor "unix:$BASE/monitor.sock,server=on,wait=off" \
  -serial "file:$BASE/serial.log" -no-reboot
