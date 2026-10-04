# Firmware example for HasNpuTop programs through a system TLM wrapper (docs/SW_INTEGRATION_GUIDE.md, section 10.5).
#
#   make PROGRAM=<insts dir> FW_INC=<platform firmware include dir> STARTUP=<platform startup.S>
#
# PROGRAM : a program directory (mmio.txt, dram_init.bin, dram_golden.bin), for example $FE_WORK/has/rce_gate
# COUNT   : optional, only the first COUNT instructions
# FW_INC  : directory that holds soc/soc_memory_map.h of the platform
# STARTUP : the platform's startup code (sets the stack pointer to _stack_top and calls main)

CROSS   ?= riscv-none-elf-
CC      := $(CROSS)gcc
ARCH    ?= -march=rv32imac_zicsr -mabi=ilp32
CFLAGS  := $(ARCH) -Os -ffreestanding -nostdlib -fno-pic -Wall -Wextra -I. -I$(FW_INC) $(EXTRA_CFLAGS)
LDFLAGS := -T linker_example.ld -Wl,--gc-sections
# The engine needs no library routine on RV32; add -lgcc here if the platform code does.
LIBS    ?=
PYTHON  ?= python3
COUNT   ?= -1

TARGET := npu_has_example.elf

all: $(TARGET)

program.S: $(PROGRAM)/mmio.txt
	$(PYTHON) make_fw_program.py $(PROGRAM) program --count $(COUNT) --asm

$(TARGET): program.S fw_main_example.c npu_has_fw.c npu_has_fw.h linker_example.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(STARTUP) fw_main_example.c npu_has_fw.c program.S $(LIBS) -o $@

clean:
	rm -f $(TARGET) program.S program.nhp

.PHONY: all clean
