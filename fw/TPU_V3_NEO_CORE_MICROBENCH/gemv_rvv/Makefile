# SPDX-License-Identifier: Apache-2.0
TOOLDIR ?= /opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin
CROSS ?= $(TOOLDIR)/riscv-none-elf-
CC := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump
READELF := $(CROSS)readelf
ARCH := rv32gcv_zvl512b
ABI := ilp32d
COMMON := ../common
BENCH_CHIP_ID ?= 0
BENCH_CORE_ID ?= 0
CFLAGS := -march=$(ARCH) -mabi=$(ABI) -O1 -g -std=c11 \
 -ffreestanding -nostdlib -nostartfiles -fno-builtin -fno-common \
 -Wall -Wextra -Werror -DNEO_BENCH_CHIP_ID=$(BENCH_CHIP_ID)u \
 -DNEO_BENCH_CORE_ID=$(BENCH_CORE_ID)u -I$(COMMON) -I.
LDFLAGS := -march=$(ARCH) -mabi=$(ABI) -nostdlib -nostartfiles -static \
 -T $(COMMON)/link.ld -Wl,--build-id=none -Wl,--gc-sections \
 -Wl,--no-warn-rwx-segments
ELF ?= neo_bench_gemv_rvv.elf
SOURCES := $(COMMON)/crt0.S main.c
DEPS := $(SOURCES) $(COMMON)/link.ld $(COMMON)/bench_map.h
.PHONY: all clean verify
all: $(ELF) verify
$(ELF): $(DEPS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SOURCES)
	$(OBJDUMP) -d $@ > $@.dis
verify: $(ELF)
	@$(READELF) -h $(ELF) | grep -q "RISC-V" || { echo "not a RISC-V ELF"; exit 1; }
	@grep -q "vlse8.v" $(ELF).dis || { echo "no strided INT8 vector load"; exit 1; }
	@grep -q "vsext.vf2" $(ELF).dis || { echo "no signed widening"; exit 1; }
	@grep -q "vwmul.vv" $(ELF).dis || { echo "no widening vector multiply"; exit 1; }
	@grep -q "vredsum.vs" $(ELF).dis || { echo "no vector reduction"; exit 1; }
	@echo "$(ELF) verify PASS"
clean:
	rm -f $(ELF) $(ELF).dis
