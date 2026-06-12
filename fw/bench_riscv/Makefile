# Cross-compile the compute-bound benchmark firmware.
# Requires the RISC-V toolchain on PATH (source tools/setup_env.sh).

CROSS   ?= riscv-none-elf-
CC      := $(CROSS)gcc

ARCH    := -march=rv32imac -mabi=ilp32
CFLAGS  := $(ARCH) -O2 -ffreestanding -nostdlib -fno-pic -Wall -Wextra
LDFLAGS := -T linker.ld -Wl,--gc-sections

TARGET := bench.elf

all: $(TARGET)

$(TARGET): startup.S src/main.c linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) startup.S src/main.c -o $@

clean:
	rm -f $(TARGET)

.PHONY: all clean
