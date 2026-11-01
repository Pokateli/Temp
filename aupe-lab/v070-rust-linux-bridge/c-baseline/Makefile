ifneq ($(KERNELRELEASE),)
ccflags-y += -I$(src)/include -Werror
obj-m += aupe_frag_lab.o
aupe_frag_lab-y := src/aupe_frag.o src/aupe_mux.o src/aupe_gate.o src/aupe_handles.o src/aupe_dispatch.o src/aupe_replay.o src/aupe_keypairs.o src/aupe_pair_mux.o src/aupe_crypto_binding.o kernel/aupe_crypto_owner.o kernel/aupe_peer_rx.o kernel/aupe_registry.o kernel/lab.o
else
CC ?= gcc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread
KDIR ?= /lib/modules/6.12.107+deb13-amd64/build
.PHONY: test test-crypto kernel clean
build/frag_tests: src/aupe_frag.c src/aupe_mux.c include/aupe_frag.h include/aupe_mux.h tests/frag_tests.c tests/mux_tests.c
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude src/aupe_frag.c tests/frag_tests.c -o $@
build/mux_tests: src/aupe_frag.c src/aupe_mux.c include/aupe_frag.h include/aupe_mux.h tests/mux_tests.c
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude src/aupe_frag.c src/aupe_mux.c tests/mux_tests.c -o $@
build/gate_tests: src/aupe_gate.c src/aupe_mux.c src/aupe_frag.c include/aupe_gate.h tests/gate_tests.c
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude src/aupe_gate.c src/aupe_mux.c src/aupe_frag.c tests/gate_tests.c -o $@
build/handles_tests: src/aupe_handles.c include/aupe_handles.h tests/handles_tests.c
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude src/aupe_handles.c tests/handles_tests.c -o $@
build/dispatch_tests: src/aupe_dispatch.c src/aupe_gate.c src/aupe_mux.c src/aupe_frag.c src/aupe_replay.c src/aupe_keypairs.c src/aupe_pair_mux.c tests/dispatch_tests.c include/aupe_dispatch.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude src/aupe_dispatch.c src/aupe_gate.c src/aupe_mux.c src/aupe_frag.c src/aupe_replay.c src/aupe_keypairs.c src/aupe_pair_mux.c tests/dispatch_tests.c -o $@
build/replay_tests: src/aupe_replay.c src/aupe_dispatch.c src/aupe_frag.c src/aupe_gate.c src/aupe_mux.c tests/replay_tests.c include/aupe_replay.h
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude src/aupe_replay.c src/aupe_dispatch.c src/aupe_keypairs.c src/aupe_pair_mux.c src/aupe_frag.c src/aupe_gate.c src/aupe_mux.c tests/replay_tests.c -o $@
build/keypairs_tests: src/aupe_keypairs.c src/aupe_pair_mux.c src/aupe_replay.c src/aupe_dispatch.c src/aupe_gate.c src/aupe_mux.c src/aupe_frag.c tests/keypairs_tests.c
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude src/aupe_keypairs.c src/aupe_pair_mux.c src/aupe_replay.c src/aupe_dispatch.c src/aupe_gate.c src/aupe_mux.c src/aupe_frag.c tests/keypairs_tests.c -o $@
test: build/frag_tests build/mux_tests build/gate_tests build/handles_tests build/dispatch_tests build/replay_tests build/keypairs_tests build/pair_mux_tests build/crypto_binding_tests
	./build/frag_tests
	./build/mux_tests
	./build/gate_tests
	./build/handles_tests
	./build/dispatch_tests
	./build/replay_tests
	./build/keypairs_tests
	./build/pair_mux_tests
	./build/crypto_binding_tests
kernel:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules
clean:
	rm -rf build
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
# Cross-keypair fragment isolation coverage (four independent workspaces).
build/pair_mux_tests: src/aupe_pair_mux.c src/aupe_dispatch.c src/aupe_keypairs.c src/aupe_replay.c src/aupe_gate.c src/aupe_mux.c src/aupe_frag.c tests/pair_mux_tests.c
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude src/aupe_pair_mux.c src/aupe_dispatch.c src/aupe_keypairs.c src/aupe_replay.c src/aupe_gate.c src/aupe_mux.c src/aupe_frag.c tests/pair_mux_tests.c -o $@

build/crypto_binding_tests: src/aupe_crypto_binding.c include/aupe_crypto_binding.h tests/crypto_binding_tests.c
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude src/aupe_crypto_binding.c tests/crypto_binding_tests.c -o $@

# Optional real ChaCha20-Poly1305 validation using OpenSSL; the kernel
# itself has no libcrypto dependency. This is NOT a Noise/WG E2E test.
build/crypto_aead_integration_tests: tests/crypto_aead_integration_tests.c src/aupe_crypto_binding.c src/aupe_dispatch.c
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude tests/crypto_aead_integration_tests.c src/aupe_crypto_binding.c src/aupe_keypairs.c src/aupe_replay.c src/aupe_dispatch.c src/aupe_gate.c src/aupe_mux.c src/aupe_frag.c src/aupe_pair_mux.c -lcrypto -o $@
test-crypto: build/crypto_aead_integration_tests
	./build/crypto_aead_integration_tests

endif
