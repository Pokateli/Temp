# Reproduction and VM-Fabric

This devkit is built locally to avoid GitHub Actions Storage consumption.
No runner or Actions artifact is required.

To reproduce in a Debian 13 AMD64 guest with matching exact kernel headers:
copy the extracted source, install the matching headers and C compiler,
run `make -B test CC=gcc`, then `make kernel KDIR=/lib/modules/$(uname -r)/build`.
Do not `insmod` a module built against a different vermagic. The module
currently registers only a load-time laboratory self-test and no network path.
The VM-Fabric project is separate from this source tree and no VM was started.
