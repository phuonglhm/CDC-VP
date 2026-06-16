# Cross-compile the SPI SoC interrupt verification firmware.
#Author: QuanNH107
#Verified by: HoangV11


CROSS   ?= riscv-none-elf-
CC      := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump

ARCH    := -march=rv32imac_zicsr -mabi=ilp32
CFLAGS  := $(ARCH) -Os -ffreestanding -nostdlib -fno-pic -Wall -Wextra
LDFLAGS := -T linker.ld -Wl,--gc-sections

SRCS   := startup.S src/main.c
TARGET := spi_test.elf

all: $(TARGET)

$(TARGET): $(SRCS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) startup.S src/main.c -o $@
	$(OBJDUMP) -d $@ > spi_test.dis

clean:
	rm -f $(TARGET) spi_test.dis

.PHONY: all clean
