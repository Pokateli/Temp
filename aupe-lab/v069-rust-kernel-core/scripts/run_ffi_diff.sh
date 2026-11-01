#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${CC:=gcc}"
export CARGO_TARGET_DIR="$PWD/build/cargo"
mkdir -p build
cargo build --offline --locked --manifest-path rust/Cargo.toml --release
LIB="$PWD/build/cargo/release/libaupe_rust_core.a"
incs=(-Ic-baseline/include -include "$PWD/tests/aliases.h")
common=(-O2 -g -std=c11 -Wall -Wextra -Werror -pthread)
link=("$LIB" -ldl -lm -lpthread)
for test in frag mux gate handles dispatch replay keypairs pair_mux crypto_binding; do
    echo "=== C test $test vs Rust 0.5.68 (compiler=$CC) ==="
    "$CC" "${common[@]}" "${incs[@]}" "c-baseline/tests/${test}_tests.c" "${link[@]}" -o "build/${test}_rust"
    "build/${test}_rust"
done
# True AEAD -> RX harness with original OpenSSL crypto, Rust post-AEAD.
"$CC" "${common[@]}" "${incs[@]}" c-baseline/tests/crypto_aead_integration_tests.c "${link[@]}" -lcrypto -o build/crypto_aead_rust
build/crypto_aead_rust
