#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
CC=${CC:-gcc}
export LC_ALL=C
mkdir -p build
SRC=(baseline/aupe_replay.c baseline/aupe_keypairs.c baseline/aupe_gate.c baseline/aupe_handles.c baseline/aupe_crypto_binding.c)
CFLAGS=(-O3 -std=c11 -Wall -Wextra -Werror -Ibaseline -Itests)
"$CC" "${CFLAGS[@]}" -DAUPE_C_ONLY tests/compare.c "${SRC[@]}" -o build/compare_c
./build/compare_c | tee "build/result_c_${CC##*/}.txt"
if ! command -v cargo >/dev/null 2>&1 || ! command -v rustc >/dev/null 2>&1; then
  echo 'RUST_TOOLCHAIN_UNAVAILABLE: Rust binary and differential performance test NOT executed' | tee build/rust_unavailable.txt
  if [[ "${AUPE_REQUIRE_RUST:-0}" == 1 ]]; then exit 2; fi
  exit 0
fi
rustc --version
cargo --version
cargo build --manifest-path rust/Cargo.toml --release --offline
"$CC" "${CFLAGS[@]}" tests/compare.c "${SRC[@]}" rust/target/release/libaupe_rust_core.a -ldl -lpthread -lm -o build/compare_dual
./build/compare_dual | tee "build/result_dual_${CC##*/}.txt"
grep -q 'ABI sizes/alignments: MATCH' "build/result_dual_${CC##*/}.txt"
