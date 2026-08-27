# SPDX-License-Identifier: Apache-2.0
#
# MB1 ReLU, the first standalone NEO-CORE microbenchmark image (G2).
#
# Same freestanding environment as the Phase 2 smoke image and the Phase 7
# pipeline image (D4): no libc, no newlib startup, no heap. One image serves
# all ten frozen cases and both implementations; what varies arrives in the
# parameter block the runner stages, not in a compile-time define — so the ELF
# hash recorded in a result row identifies the code that ran rather than the
# case it ran.

TOOLDIR ?= /opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin
CROSS   ?= $(TOOLDIR)/riscv-none-elf-

CC      := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump
READELF := $(CROSS)readelf

ARCH := rv32gcv_zvl512b
ABI  := ilp32d

COMMON := ../common

# The chip/core indices are D27's retained address-map slot. They are build
# inputs rather than literals so the same source can be pointed at another slot
# without a second copy of the map; the runner asserts whatever they produce
# against `tpu_v3/address_map.h` before it runs the image.
BENCH_CHIP_ID ?= 0
BENCH_CORE_ID ?= 0

# -O1 for the same reason the other images use it: the benchmark reads back
# what it just wrote, and -O2 is free to prove those reads redundant. Every
# MMIO and buffer access goes through a volatile pointer, so the optimiser
# cannot elide the traffic itself.
#
# -O0 would be worse than either: it would make the scalar baseline a
# measurement of the compiler's spill behaviour rather than of the machine.
CFLAGS := -march=$(ARCH) -mabi=$(ABI) \
          -O1 -g -std=c11 \
          -ffreestanding -nostdlib -nostartfiles \
          -fno-builtin -fno-common \
          -Wall -Wextra -Werror \
          -DNEO_BENCH_CHIP_ID=$(BENCH_CHIP_ID)u \
          -DNEO_BENCH_CORE_ID=$(BENCH_CORE_ID)u \
          -I$(COMMON) -I.

LDFLAGS := -march=$(ARCH) -mabi=$(ABI) \
           -nostdlib -nostartfiles -static \
           -T $(COMMON)/link.ld -Wl,--build-id=none -Wl,--gc-sections \
           -Wl,--no-warn-rwx-segments

ELF ?= neo_bench_relu.elf

SOURCES := $(COMMON)/crt0.S main.c
DEPS    := $(SOURCES) $(COMMON)/link.ld $(COMMON)/bench_map.h

.PHONY: all clean verify
all: $(ELF) verify

$(ELF): $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(COMMON)/crt0.S main.c
	$(OBJDUMP) -d $@ > $@.dis
	@echo "built $@"

# The image is only evidence if it contains what it claims to. Checked at build
# time rather than in the runner so a toolchain that silently dropped the
# vector instructions fails here, where the cause is visible, instead of
# presenting later as an RVV run whose numbers match the scalar one.
verify: $(ELF)
	@$(READELF) -h $(ELF) | grep -q "RISC-V" || { echo "not a RISC-V ELF"; exit 1; }
	@grep -qE "vsetvli" $(ELF).dis || { echo "no vector configuration instruction"; exit 1; }
	@grep -q "vle32.v" $(ELF).dis || { echo "no vector load"; exit 1; }
	@grep -q "vmax.vx" $(ELF).dis || { echo "no vector maximum -- the RVV ReLU is not in the image"; exit 1; }
	@grep -q "vse32.v" $(ELF).dis || { echo "no vector store"; exit 1; }
	@echo "$(ELF) verify PASS"

clean:
	rm -f $(ELF) $(ELF).dis
