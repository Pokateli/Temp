# AUPE 0.5.65 test matrix

**Done (userspace C on host):** GCC / Clang / ASan+UBSan; cross-keypair
IPv4 ID collision; old and new partial reassemblies; replay after delivery;
previous-key retirement; four workspace zeroization; 40,000 successive
keypair generations; all seven prior suites.

**Done (build only):** isolated `.ko` compiled with `-Werror` against available
6.12.107 Debian headers; load-time smoke now checks fragment isolation.

**Not done:** load-time smoke execution in Debian 13, exact kernel 6.12.113,
real kernel WG and AWG handshakes, AEAD/anti-replay integration, peer revoke
races under network load, TX, NAT/Netfilter and per-session routing, protocol
interoperability and long-running stress.
