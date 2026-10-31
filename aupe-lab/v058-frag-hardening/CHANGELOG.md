# AUPE fragment safety milestone 0.5.58

- Harden secure fragment storage by wiping all plaintext on datagram reset,
  completion, timeout, errors and trusted epoch revocation.
- Enforce bounded per-peer fragment count (256) and ECN merge/reject rules.
- Fix undelivered completed-packet expiration that previously held state.
- Expand regression coverage to shuffled datagrams, ECN and 4 concurrent peer
  objects with identical inner IPv4 addressing.
- Verified 435898 checks GCC/Clang/ASan+UBSan, link-only module on 6.12.107.
- **No** integration with AUPE 0.5.56 source or full data plane yet.
