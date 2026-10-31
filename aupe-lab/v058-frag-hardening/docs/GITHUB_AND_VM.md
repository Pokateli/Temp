# Offline delivery / VM recheck checklist

Use this source-only branch to reproduce without consuming GitHub Actions
storage. No workflow is required or started.

1. Copy/checkout this tree into the Debian13 profile of VM-Fabric.
2. Install headers matching the **guest's actual** `uname -r`.
3. `make test`, `make kernel KDIR=/lib/modules/$(uname -r)/build`.
4. Run `modinfo aupe_frag_lab.ko`, compare `vermagic` to `uname -r`.
5. Lab module loads no hooks or sockets. `insmod`/`rmmod` in a test VM only;
   never replace the real `aupe.ko` with this laboratory stub.
6. Before claiming AUPE 0.5.58, obtain verified 0.5.56 source archive,
   merge the C core at the post-decrypt/replay point, compile against exact
   `6.12.113+deb13-amd64` and test actual WG/AWG VPN data end-to-end.

Note: a Debian13 `6.12.107` compile is not evidence of binary compatibility
with target `6.12.113`. Use proper kernel module vermagic.
