#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
LIB="${1:-rust/target/release/libaupe_rust_core.a}"
[[ -r "$LIB" ]] || { echo "Missing Rust staticlib: $LIB" >&2; exit 2; }
CC="${CC:-gcc}"
"$CC" -std=c11 -O2 -g -Wall -Wextra -Werror -Iinclude -Ic-baseline/include \
    tests/bridge_abi_tests.c "$LIB" -lpthread -ldl -lm \
    -o "${OUT:-/tmp/aupe070_bridge_abi_tests}"
"${OUT:-/tmp/aupe070_bridge_abi_tests}"
