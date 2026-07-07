# E2E firmware for the VP_FX1_Full_SoC ROM-code boot flow
# (docs/romcode_boot_hw_plan.md, Phase 1 step 3).
#
# Builds:
#   bootrom.elf  ROM code, entry 0x0            -> VP --fw
#   app.bin      raw app image for IFLASH        -> VP --int-flash
#
# Run:
#   vp_fx1_full_soc --fw bootrom.elf --int-flash app.bin --boot-pin low|high

TOOLDIR ?= /opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin
CROSS   ?= $(TOOLDIR)/riscv-none-elf-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy
OBJDUMP := $(CROSS)objdump

ARCH    := -march=rv32imac_zicsr -mabi=ilp32
CFLAGS  := $(ARCH) -Os -ffreestanding -nostdlib -fno-pic -Wall -Wextra
LDFLAGS := -Wl,--gc-sections

all: bootrom.elf app.bin

bootrom.elf: bootrom/start.S bootrom/main.c bootrom/linker.ld
	$(CC) $(CFLAGS) -T bootrom/linker.ld $(LDFLAGS) bootrom/start.S bootrom/main.c -o $@
	$(OBJDUMP) -d $@ > bootrom.dis

app.elf: app/start.S app/main.c app/linker.ld
	$(CC) $(CFLAGS) -T app/linker.ld $(LDFLAGS) app/start.S app/main.c -o $@
	$(OBJDUMP) -d $@ > app.dis

app.bin: app.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f bootrom.elf app.elf app.bin bootrom.dis app.dis

.PHONY: all clean
