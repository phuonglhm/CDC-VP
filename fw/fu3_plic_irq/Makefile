# FU3 PLIC IRQ test trên CVA6: TIMER1 -> AP_PLIC ID 6 -> CVA6 irq_i[0] (mcause 11).
# Dùng linker/startup của fu3_cva6_smoke (CVA6_ISRAM 0x0001_0000, DSRAM 0x0100_0000).
# Máy chỉ có riscv64-unknown-elf thì: make CROSS=riscv64-unknown-elf-

CROSS   ?= riscv-none-elf-
CC      := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump

FU3     := ../../components/FU3_Components
ARCH    := -march=rv32imc_zicsr -mabi=ilp32
CFLAGS  := $(ARCH) -O2 -g -ffreestanding -nostdlib -nostartfiles -fno-pic \
           -msmall-data-limit=0 -Wall -Wextra \
           -I$(FU3)/timer/include -I$(FU3)/plic/include
LDFLAGS := -T ../fu3_cva6_smoke/linker.ld -Wl,--gc-sections

SRCS    := ../timer_irq_riscv/startup.S src/main.c
TARGET  := fu3_plic_irq.elf

all: $(TARGET)

$(TARGET): $(SRCS) ../fu3_cva6_smoke/linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@
	$(OBJDUMP) -d $@ > fu3_plic_irq.dis

clean:
	rm -f $(TARGET) fu3_plic_irq.dis

.PHONY: all clean
