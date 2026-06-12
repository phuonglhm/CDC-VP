# Cross-compile the bare-metal hello for the riscv_cpu_eval platform.
# Requires the RISC-V toolchain on PATH (source tools/setup_env.sh).

CROSS   ?= riscv-none-elf-
CC      := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump

# RV32IMAC / ilp32 matches the mariusmm core (CDC_CPU_BACKEND=riscv_tlm).
ARCH    := -march=rv32imac -mabi=ilp32
CFLAGS  := $(ARCH) -Os -ffreestanding -nostdlib -fno-pic -Wall -Wextra
LDFLAGS := -T linker.ld -Wl,--gc-sections

SRCS := startup.S src/main.c
TARGET := hello.elf

all: $(TARGET)

$(TARGET): $(SRCS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) startup.S src/main.c -o $@
	$(OBJDUMP) -d $@ > hello.dis

clean:
	rm -f $(TARGET) hello.dis

.PHONY: all clean
