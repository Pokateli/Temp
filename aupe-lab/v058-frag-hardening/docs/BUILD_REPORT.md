# AUPE 0.5.58 — local verification (2026-11-01)

| Gate | Result |
|---|---|
| GCC `-O2 -std=c11 -Wall -Wextra -Werror -pedantic -pthread` | PASS |
| Clang `-O2 -std=c11 -Wall -Wextra -Werror -pedantic -pthread` | PASS |
| GCC AddressSanitizer and UndefinedBehaviorSanitizer | PASS |
| Deterministic, malformed, ECN, overlap, expiry, key epoch, boundary and randomized shuffled tests | **435898 checks / 0 failures** |
| Four simultaneous peer-owned threads, identical IPv4 source/destination/ID but separate state | PASS (included above) |
| Link-only kernel module on Debian 13 headers 6.12.107+deb13-amd64, Werror | PASS |
| `modinfo` module version / vermagic | `0.5.58` / `6.12.107+deb13-amd64` |
| Exact target headers 6.12.113+deb13-amd64 | NOT RUN (not present locally; archived release asset not download-accessible) |
| Module loaded into a guest VM | NOT RUN (QEMU guest image unavailable) |
| Actual AUPE 0.5.56 dataplane merged/rebuilt | NO (raw archive bytes unavailable) |
| WG/AWG traffic, Noise handshake, Netfilter routing E2E, multiple clients same IPv4 | NOT RUN |
| ARM64 | NOT ATTEMPTED |

Commands to reproduce optional sanitizer runs:

```sh
mkdir -p build
gcc -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -std=c11 -Wall -Wextra -Werror -pedantic -pthread -Iinclude \
  src/aupe_frag.c tests/frag_tests.c -o build/frag_tests_asan
ASAN_OPTIONS=halt_on_error=1:detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ./build/frag_tests_asan
clang -O2 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread -Iinclude \
  src/aupe_frag.c tests/frag_tests.c -o build/frag_tests_clang
./build/frag_tests_clang
```

Test scope matters: threaded tests deliberately use separate `af_state`
objects; they are not a concurrency test of shared state or the future kernel
peer lock. The `.ko` exports no VPN dataplane functionality.
