#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
make -B test CC=gcc
make -B test CC=clang
# The full sanitizer stress suite can be slow on emulated or shared CPUs.
make -B build/crypto_binding_tests CC=gcc CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread -fno-omit-frame-pointer -fsanitize=address,undefined'
./build/crypto_binding_tests
if pkg-config --exists openssl; then
    make -B test-crypto CC=gcc
else
    echo 'OpenSSL development package missing: synthetic AEAD test skipped' >&2
fi
KDIR=${KDIR:-/lib/modules/6.12.107+deb13-amd64/build}
if [ -d "$KDIR" ]; then make -B kernel KDIR="$KDIR"; else echo "Headers unavailable: $KDIR" >&2; fi
