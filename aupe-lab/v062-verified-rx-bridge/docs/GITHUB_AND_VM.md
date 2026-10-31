# AUPE 0.5.62 source publication and VM qualification

- Publish source in a dedicated branch rooted at prior 0.5.61; keep `main`
  and all GitHub Actions workflows unchanged. No CI artifact uploads.
- The ZIP holds sources, lab `.ko`, logs and SHA256SUMS; GitHub stores source.
- For Debian 13 VM testing: boot AMD64 VM-Fabric with exact 6.12.113 kernel,
  install matching header packages, then build module against the *running*
  `uname -r` headers; verify vermagic, insmod/rmmod, dmesg smoke and memory.
- Tests that require root privileges or a real authenticated WG/AWG dataplane
  remain deliberately NOT RUN in current environment. Do not mark them PASS
  based merely on `make kernel` or a portable C unit test.
