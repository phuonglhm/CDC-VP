# Cross-compile the VP_FX1 SAURIA NPU v4 end-to-end firmware test.

CROSS   ?= riscv-none-elf-
CC      := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump

ARCH    := -march=rv32imac_zicsr -mabi=ilp32
CFLAGS  := $(ARCH) -Os -ffreestanding -nostdlib -fno-pic -Wall -Wextra \
           -I../common/include
LDFLAGS := -T linker.ld -Wl,--gc-sections

SRCS   := startup.S src/main.c
TARGET := npu_v4_irq.elf

all: $(TARGET)

$(TARGET): $(SRCS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) startup.S src/main.c -o $@
	$(OBJDUMP) -d $@ > npu_v4_irq.dis

clean:
	rm -f $(TARGET) npu_v4_irq.dis

.PHONY: all clean
