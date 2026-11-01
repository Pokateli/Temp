# Repeatable build and limitations

No GitHub Actions are required or executed by the local test suite. Publish
sources to a separate `aupe/0.5.64-dual-keypair` branch; do not push edits to
`.github/workflows/` or turn on CI because GitHub Actions storage is limited.

Validation already run locally: GCC/Clang, GCC ASan+UBSan, and external kernel
module build using installed Debian 13 headers `6.12.107+deb13-amd64`.
Kernel module smoke logic is **compiled only**, not exercised via insmod.

Target testing must still use Debian 13 AMD64 VM-Fabric with kernel and headers
`6.12.113+deb13-amd64` when a locally accessible VM and matching headers are
available. Use QEMU overlay and no paid/charged storage. Network access is not
required for functional loopback tests; real WG/AWG cryptographic integration
must be completed first.
