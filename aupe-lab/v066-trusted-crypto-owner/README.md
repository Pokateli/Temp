# AUPE 0.5.66 — authenticated keypair owner bridge

**Development kit only — NOT a usable or production VPN server.** The previous 0.5.65 fragment/keypair lab is extended with a *kernel-only owner interface* that can be called by a genuine WireGuard/AmneziaWG crypto engine after it has authenticated the Noise peer and AEAD-decrypted a packet. **No actual WG/AWG crypto engine, UDP listener, TUN, TX, NAT or complete dataplane is included.**

## Main change

An AUPE peer is registered under its confirmed, 32-byte Noise static public key. On an authenticated keypair installation the crypto owner supplies its pinned local keypair object and an owner-managed monotonic generation; AUPE chooses its own **local epoch**. After AEAD succeeds using that exact pinned keypair object, `aupe_crypto_verified_rx` resolves the corresponding local epoch, then calls the existing per-keypair 8192-counter replay gate and dedicated fragment reassembly mux. The API does not accept an untrusted tunnel address or caller-chosen epoch. The previous keypair may remain valid for at most 180,000 ms, as before.

**Critical trust contract:** the wrapper does *not* perform Noise or AEAD authentication itself. It must be integrated in the trusted cryptographic owner's code; the caller MUST pin the keypair, authenticate its public key and validate the AEAD tag before calling. A pointer/serial is an owner-internal identity, NOT cryptographic proof on its own. The complete transport counter comes from the successfully decrypted WireGuard message. An unauthenticated UDP index, public key claimed in a message, tunnel IP or manager input must not be used as any of these values.

## Build and checks

```sh
make -B test CC=gcc
make -B test CC=clang
make -B build/crypto_binding_tests CC=gcc CFLAGS='-O1 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread -fno-omit-frame-pointer -fsanitize=address,undefined'
./build/crypto_binding_tests
make -B test-crypto CC=gcc     # optional libcrypto / OpenSSL dependency
make -B kernel KDIR=/lib/modules/6.12.107+deb13-amd64/build
modinfo aupe_frag_lab.ko
```

`docs/BUILD_REPORT.md` contains actual test results and limitations. The `aupe_frag_lab.ko` smoke is compiled, **not executed in the guest OS**. No release activity or GitHub Actions builds are needed.

See `docs/CRYPTO_OWNER_ABI.md` for ownership and threading requirements. `src/aupe_crypto_binding.c` is a portable model testable without WireGuard. No key or credentials are bundled.
