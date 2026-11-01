# 0.5.66 test matrix

| Scenario | Evidence | Status |
|---|---|---|
| Baseline regression (8 suites) | GCC/Clang logs | PASS |
| Monotonic crypto-owner generation / epoch fencing | crypto_binding_tests | PASS |
| Foreign static public key, stale pointer, ABA, duplicate | crypto_binding_tests | PASS |
| Actual ChaCha20-Poly1305 AEAD/tag-corruption in test harness | TEST_AEAD.log | PASS (synthetic, not WG framing) |
| Kernel compile / modinfo | KERNEL_BUILD_LOG.txt | PASS (6.12.107) |
| Kernel load-time smoke execution | no guest run | NOT RUN |
| Complete ASan/UBSan regression | timed out during pair_mux_tests | INCOMPLETE; new binding test passed separately |
| Debian 13 kernel 6.12.113 | headers unavailable | NOT RUN |
| Real WG/AWG Noise, handshake and encrypted UDP | no owner implementation connected | NOT RUN |
