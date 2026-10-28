# SPDX-License-Identifier: Apache-2.0
#
# Example firmware of the VP_FU3_V1.0 package (FU3 SoC virtual platform, R1).
# VP verification firmware, not a product Boot ROM: it shows the boot contract
# of FU3_VP_ABI_1 (include/fu3_vp_abi.h) on the VP. The product Boot ROM and
# firmware belong to FW/SW.
#
#   make                  build every image into OUTPUT_DIR
#   make TOOLDIR=<dir>    another riscv-none-elf toolchain (default: xPack 15.2)
#   make clean
#
# Images (ROM stub common/ibex_rom.S at 0x80 -> image in IBEX_ISRAM; CVA6
# images at 0x0001_0000, released by the Ibex image):
#   t20_console.elf              Ibex: console, straps, reset cause, console input, NOR via XIP
#   t19_ibex.elf + t19_cva6.elf  software reset, CVA6 handoff, single-precision FP (GDB demo)
#   t21_ibex.elf + t21_cva6.elf  WFI wakes on both CPUs, SWIRQ, watchdog reset during traffic
#   vp_debug_blob.bin            VP placeholder of the debug-partition blob (FU3_VP_ABI_1 §6)

PACKAGE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
OUTPUT_DIR  ?= $(abspath $(PACKAGE_DIR)/../../build/VP_FU3_V1.0)

TOOLDIR ?= /opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin
CROSS   ?= $(TOOLDIR)/riscv-none-elf-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump

# One ISA per CPU (isa.md of the package).
IBEX_ARCH := -march=rv32imc_zcb_zicsr_zifencei -mabi=ilp32
CVA6_ARCH := -march=rv32imfc_zicsr_zifencei -mabi=ilp32f

# -ffile-prefix-map keeps the build machine's directory out of the images.
CFLAGS := -O1 -g -ffile-prefix-map=$(PACKAGE_DIR)=VP_FU3_V1.0 -ffreestanding -nostdlib -nostartfiles \
          -msmall-data-limit=0 -Wall -Wextra -Werror -I$(PACKAGE_DIR)/common -I$(PACKAGE_DIR)/include

C := $(PACKAGE_DIR)/common
E := $(PACKAGE_DIR)/examples
RUNTIME := $(C)/fw_trap.S $(C)/fw_runtime.c
IBEX_START := $(C)/ibex_rom.S $(C)/ibex_start.S
CVA6_START := $(C)/cva6_start.S
DEPS := $(RUNTIME) $(C)/fw_common.h $(PACKAGE_DIR)/include/fu3_vp_abi.h

IMAGES := t20_console t19_ibex t19_cva6 t21_ibex t21_cva6

all: $(addprefix $(OUTPUT_DIR)/,$(addsuffix .elf,$(IMAGES))) $(OUTPUT_DIR)/vp_debug_blob.bin

$(OUTPUT_DIR):
	mkdir -p $@

# Ibex images: ROM stub + start-up + program, linked for ROM and IBEX_ISRAM.
$(OUTPUT_DIR)/t20_console.elf $(OUTPUT_DIR)/t19_ibex.elf $(OUTPUT_DIR)/t21_ibex.elf: \
		$(OUTPUT_DIR)/%.elf: $(E)/%.c $(IBEX_START) $(C)/ibex.ld $(DEPS) | $(OUTPUT_DIR)
	$(CC) $(IBEX_ARCH) $(CFLAGS) $(RUNTIME) $(IBEX_START) $< -T $(C)/ibex.ld -Wl,-Map=$(@:.elf=.map) -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.dis)

# CVA6 images: start-up + program, linked for CVA6_ISRAM (code) and DSRAM (data).
$(OUTPUT_DIR)/t19_cva6.elf $(OUTPUT_DIR)/t21_cva6.elf: \
		$(OUTPUT_DIR)/%.elf: $(E)/%.c $(CVA6_START) $(C)/cva6.ld $(DEPS) | $(OUTPUT_DIR)
	$(CC) $(CVA6_ARCH) $(CFLAGS) $(RUNTIME) $(CVA6_START) $< -T $(C)/cva6.ld -Wl,-Map=$(@:.elf=.map) -o $@
	$(OBJDUMP) -d $@ > $(@:.elf=.dis)

$(OUTPUT_DIR)/vp_debug_blob.bin: $(E)/vp_debug_blob.S | $(OUTPUT_DIR)
	$(CC) -march=rv32i_zicsr -mabi=ilp32 -c $< -o $(OUTPUT_DIR)/vp_debug_blob.o
	$(OBJCOPY) -O binary -j .rodata.vp_debug_blob $(OUTPUT_DIR)/vp_debug_blob.o $@

clean:
	rm -f $(OUTPUT_DIR)/*.elf $(OUTPUT_DIR)/*.map $(OUTPUT_DIR)/*.dis $(OUTPUT_DIR)/*.o $(OUTPUT_DIR)/*.bin

.PHONY: all clean
