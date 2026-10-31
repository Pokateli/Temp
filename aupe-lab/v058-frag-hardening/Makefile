ifneq ($(KERNELRELEASE),)
ccflags-y += -I$(src)/include -Werror
obj-m += aupe_frag_lab.o
aupe_frag_lab-y := src/aupe_frag.o kernel/lab.o
else
CC ?= gcc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Werror -pedantic -pthread
KDIR ?= /lib/modules/6.12.107+deb13-amd64/build
.PHONY: test kernel clean
build/frag_tests: src/aupe_frag.c include/aupe_frag.h tests/frag_tests.c
	mkdir -p build
	$(CC) $(CFLAGS) -Iinclude src/aupe_frag.c tests/frag_tests.c -o $@
test: build/frag_tests
	./build/frag_tests
kernel:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules
clean:
	rm -rf build
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
endif
