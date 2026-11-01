#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
make -B test CC=gcc CFLAGS='-O2 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread'
make -B test CC=clang CFLAGS='-O2 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread'
make -B test CC=gcc CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread -fno-omit-frame-pointer -fsanitize=address,undefined'
if [ -d /lib/modules/6.12.107+deb13-amd64/build ]; then
  make -B kernel KDIR=/lib/modules/6.12.107+deb13-amd64/build
else
  echo 'Debian 13 kernel headers 6.12.107 missing: kernel build SKIPPED' >&2
fi