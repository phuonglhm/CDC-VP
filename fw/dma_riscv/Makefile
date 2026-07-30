CROSS   ?= riscv-none-elf-
CC      := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump
ARCH    := -march=rv32imac_zicsr -mabi=ilp32
# `EXTRA_CFLAGS` is the hook for platform-specific defines. `noc_soc` follows
# docs/peripheral_memory_map.md, where DMA0 is at 0x1006_0000, so it needs
# `make EXTRA_CFLAGS=-DDMA_BASE=0x10060000u`. The default suits VP_FX1_Full_SoC.
CFLAGS  := $(ARCH) -Os -ffreestanding -nostdlib -fno-pic -Wall -Wextra $(EXTRA_CFLAGS)
LDFLAGS := -T linker.ld -Wl,--gc-sections
SRCS   := startup.S src/main.c
TARGET := dma_test.elf
all: $(TARGET)
$(TARGET): $(SRCS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) startup.S src/main.c -o $@
	$(OBJDUMP) -d $@ > dma_test.dis
clean:
	rm -f $(TARGET) dma_test.dis
.PHONY: all clean