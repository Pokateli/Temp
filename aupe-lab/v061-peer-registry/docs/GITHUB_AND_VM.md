# GitHub / Debian VM reproducibility

- Source development uses `Pokateli/Temp`, isolated `aupe/0.5.60-*` branch.
- `VM-Fabric` remains the authorized Debian VM orchestrator. Do not assume
  a GitHub-hosted VM is persistent or accessible by this chat.
- No workflow dispatches, Actions artifact uploads or paid storage are used.
- Test locally with `make test`, followed by exact-header `make kernel`.
- The 6.12.107 headers in the chat runtime support **compile/link only**.
- Debian 13 target 6.12.113 headers and the QEMU guest are **not currently
  mounted in this runtime**. VM E2E and real `aupe.ko` integration are pending.
- Archive includes `SHA256SUMS.txt` for post-transfer verification.
