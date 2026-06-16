# Cross-compile the ADC SoC interrupt verification firmware.

CROSS   ?= riscv-none-elf-
CC      := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump

ARCH    := -march=rv32imac_zicsr -mabi=ilp32
CFLAGS  := $(ARCH) -Os -ffreestanding -nostdlib -fno-pic -Wall -Wextra
LDFLAGS := -T linker.ld -Wl,--gc-sections

SRCS   := startup.S src/main.c
TARGET := adc_irq.elf

all: $(TARGET)

$(TARGET): $(SRCS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) startup.S src/main.c -o $@
	$(OBJDUMP) -d $@ > adc_irq.dis

clean:
	rm -f $(TARGET) adc_irq.dis

.PHONY: all clean
