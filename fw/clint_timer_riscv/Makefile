# Cross-compile the CLINT timer-interrupt demo for the riscv_custom_soc platform.
# Requires the RISC-V toolchain on PATH (source tools/setup_env.sh).

CROSS   ?= riscv-none-elf-
CC      := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump

ARCH    := -march=rv32imac_zicsr -mabi=ilp32
CFLAGS  := $(ARCH) -Os -ffreestanding -nostdlib -fno-pic -Wall -Wextra
LDFLAGS := -T linker.ld -Wl,--gc-sections

SRCS   := startup.S src/main.c
TARGET := clint_timer.elf

all: $(TARGET)

$(TARGET): $(SRCS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) startup.S src/main.c -o $@
	$(OBJDUMP) -d $@ > clint_timer.dis

clean:
	rm -f $(TARGET) clint_timer.dis

.PHONY: all clean
