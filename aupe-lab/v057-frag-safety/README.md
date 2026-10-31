# AUPE 0.5.57 — IPv4 Fragment Reassembly Safety Lab (AMD64)

**Status: independently built/verified lab component, not a replacement for the AUPE VPN kernel dataplane.**

## Provenance

The latest AUPE artifact found in the previous chats' Library is
`AUPE_v0.5.56_amd64_ipv4_fragment_reassembly_devkit.zip` (31 October 2026).
That ZIP was not available as authorized raw bytes in this session, and no
0.5.56 kernel sources were found on the accessible GitHub branches. This lab
was written as an **isolated 0.5.57 integration candidate**, not as a patched
copy of 0.5.56. No 0.5.56 code is implied to have been changed or tested.

## Safety contracts

- Exactly **one in-flight IPv4 datagram per trusted cryptographic session**.
- The caller provides one separate `af_state` for each authenticated peer /
  Noise session. No global mapping on source IP or subnet; duplicated
  `172.16.0.0/12` addresses across clients are permitted by design.
- **Only packets that already passed AEAD authentication AND anti-replay** may
  reach `af_feed_ipv4`. The `trusted_epoch` is assigned by the kernel's trusted
  session controller, never derived from the packet.
- The state is **not internally synchronized**. The production caller must
  serialize `feed`, `take`, epoch rotation and teardown; protect lifetime so
  the state cannot be freed while an RX worker uses it.
- No memory allocation on the fragment RX path: fixed-size workspace stored
  inside caller-allocated `struct af_state` (~72 KiB per enabled peer).
  Allocate with `kvzalloc(..., GFP_KERNEL)` in sleepable context. Do not
  allocate this large object on the kernel stack.
- Hard limit: 65,515 IPv4 payload bytes + 20-byte header, 5-second assembly
  timeout, one incomplete packet/session, no timeout extension on input.
- Malformed, overlapping and duplicate fragments are rejected. Conflicting
  final lengths are rejected; mismatched IDs/source/destination/protocol do
  not overwrite another in-flight packet.
- IPv4 options (`IHL != 5`) are deliberately **not supported** by this lab
  candidate; the module rejects those packets instead of guessing option
  semantics. This limitation requires compatibility testing before deployment.
- On Noise epoch change and session teardown, previous plaintext buffers
  are explicitly scrubbed. On packet completion, the caller receives a
  reconstructed IPv4 header (offset and MF cleared, new total length/checksum).
- `AF_BYPASS` is the valid ordinary unfragmented IPv4 path: send it through
  the existing AUPE post-decryption dataplane, not to the fragment workspace.

## How to verify

```bash
make test
# Optional userspace sanitizer tests:
gcc -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
    src/aupe_frag.c tests/frag_tests.c -o build/frag_tests_asan
ASAN_OPTIONS=halt_on_error=1:detect_leaks=1 ./build/frag_tests_asan

# Link-only kernel module (no hooks/no VPN), against installed kernel headers:
make kernel KDIR=/lib/modules/6.12.107+deb13-amd64/build
modinfo aupe_frag_lab.ko
```

The module compiled here has **vermagic 6.12.107+deb13-amd64**. It cannot be
loaded into a running kernel with a different release. It was not loaded or
used for an end-to-end VPN exchange. The prior 0.5.56 target
`6.12.113+deb13-amd64` has not been verified in this session.

## Next integration milestone

See `docs/INTEGRATION.md` for the minimum release gates for an actual AUPE
0.5.57. Work on ARM64 remains deferred. No GitHub Actions artifacts,
billable storage or scheduled workflows are required to build/test this kit.
