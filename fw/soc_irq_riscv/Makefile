# Cross-compile the custom-SoC interrupt demo (CLINT timer + PLIC external).
# Requires the RISC-V toolchain on PATH (source tools/setup_env.sh).

CROSS   ?= riscv-none-elf-
CC      := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump

ARCH    := -march=rv32imac_zicsr -mabi=ilp32
CFLAGS  := $(ARCH) -Os -ffreestanding -nostdlib -fno-pic -Wall -Wextra
LDFLAGS := -T linker.ld -Wl,--gc-sections

SRCS   := startup.S src/main.c
TARGET := soc_irq.elf

all: $(TARGET)

$(TARGET): $(SRCS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) startup.S src/main.c -o $@
	$(OBJDUMP) -d $@ > soc_irq.dis

clean:
	rm -f $(TARGET) soc_irq.dis

.PHONY: all clean
