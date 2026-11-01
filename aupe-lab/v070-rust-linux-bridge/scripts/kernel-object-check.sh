#!/usr/bin/env bash
# Compile the C Linux adapter against available target headers. NEVER link
# it into a fake loadable .ko while native Rust-for-Linux is unavailable.
set -euo pipefail
cd "$(dirname "$0")/.."
KDIR="${KDIR:-/lib/modules/6.12.107+deb13-amd64/build}"
[[ -f "$KDIR/Makefile" ]] || { echo "Missing Kbuild headers $KDIR" >&2; exit 2; }
make -C "$KDIR" M="$PWD/kernel-bridge" aupe_rs_linux_owner.o
file kernel-bridge/aupe_rs_linux_owner.o
nm -u kernel-bridge/aupe_rs_linux_owner.o | grep 'aupe_rs_peer_'
echo 'AUPE070_KERNEL_OBJECT_COMPILE=PASS (not a .ko)'
