CROSS   ?= riscv-none-elf-
CC      := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump

ARCH    := -march=rv32imac_zicsr -mabi=ilp32
CFLAGS  := $(ARCH) -Os -ffreestanding -nostdlib -fno-pic -Wall -Wextra \
           -ffunction-sections -fdata-sections
LDFLAGS := -T linker.ld -Wl,--gc-sections

TARGET := edu_timer_demo.elf
SOURCES := startup.S src/main.c

all: $(TARGET)

$(TARGET): $(SOURCES) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(SOURCES) -o $@
	$(OBJDUMP) -d $@ > edu_timer_demo.dis

clean:
	rm -f $(TARGET) edu_timer_demo.dis

.PHONY: all clean

