#!/bin/sh
# Offline build/test, no CI/Artifacts, no network or container image pulls.
set -eu
cd "$(dirname "$0")/.."
make -B test CC=gcc CFLAGS='-O2 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread'
make -B test CC=clang CFLAGS='-O2 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread'
make -B test CC=gcc CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread -fno-omit-frame-pointer -fsanitize=address,undefined'
if [ -d /lib/modules/6.12.107+deb13-amd64/build ]; then
    make -B kernel KDIR=/lib/modules/6.12.107+deb13-amd64/build
else
    echo 'Kernel headers 6.12.107 unavailable: kernel build skipped' >&2
fi
