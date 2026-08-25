# SPDX-License-Identifier: Apache-2.0
#
# The Phase 7 NEO-CORE pipeline image: DMA -> Transform (Im2Col) -> MXU -> RVV.
#
# Same freestanding environment as the Phase 2 smoke image (decision record
# D4): no libc, no newlib startup, no heap. What differs is the linker script —
# this one is laid out against the real TPU_V3 address map rather than a flat
# probe memory, because the image runs on a composed NEO-CORE.

TOOLDIR ?= /opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin
CROSS   ?= $(TOOLDIR)/riscv-none-elf-

CC      := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump
READELF := $(CROSS)readelf

ARCH := rv32gcv_zvl512b
ABI  := ilp32d

# The historical Phase 7 gate uses 1/1. D27 reuses the same image source with
# 0/0 so the standalone baseline has mhartid=0 and the chip-0/core-0 aperture.
PIPELINE_CHIP_ID ?= 1
PIPELINE_CORE_ID ?= 1

# -O1 for the same reason the smoke image uses it: the driver reads back what
# the engines wrote, and -O2 is free to prove those reads redundant. Every MMIO
# access goes through a volatile pointer, so the optimiser cannot elide the
# register traffic itself.
CFLAGS := -march=$(ARCH) -mabi=$(ABI) \
          -O1 -g -std=c11 \
          -ffreestanding -nostdlib -nostartfiles \
          -fno-builtin -fno-common \
          -Wall -Wextra -Werror \
          -DNEO_PIPELINE_CHIP_ID=$(PIPELINE_CHIP_ID) \
          -DNEO_PIPELINE_CORE_ID=$(PIPELINE_CORE_ID) \
          -I.

LDFLAGS := -march=$(ARCH) -mabi=$(ABI) \
           -nostdlib -nostartfiles -static \
           -T link.ld -Wl,--build-id=none -Wl,--gc-sections \
           -Wl,--no-warn-rwx-segments

ELF ?= neo_core_pipeline.elf

.PHONY: all clean verify
all: $(ELF) verify

$(ELF): crt0.S main.c link.ld neo_core_map.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ crt0.S main.c
	$(OBJDUMP) -d $@ > $@.dis
	@echo "built $@"

# The image is only evidence if it contains what it claims to. Checked here
# rather than in the C++ harness so a toolchain that silently dropped the
# vector instructions fails at build time.
verify: $(ELF)
	@$(READELF) -h $(ELF) | grep -q "RISC-V" || { echo "not a RISC-V ELF"; exit 1; }
	@grep -qE "vsetvli|vsetivli" $(ELF).dis || { echo "no vector configuration instruction"; exit 1; }
	@grep -q "vle32.v" $(ELF).dis || { echo "no vector load"; exit 1; }
	@grep -q "vredsum.vs" $(ELF).dis || { echo "no vector reduction"; exit 1; }
	@echo "$(ELF) verify PASS"

clean:
	rm -f $(ELF) $(ELF).dis
