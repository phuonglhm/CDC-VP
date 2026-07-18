# SPDX-License-Identifier: Apache-2.0
#
# FreeRTOS bring-up firmware for VP_FX1_Full_SoC (Bremen rv32 backend).
#
# The kernel comes from third_party/FreeRTOS-Kernel (pinned by
# tools/third_party/setup_third_party.sh); nothing from the kernel tree is
# copied into the repo.
#
#   make            # includes the NPU demo task (needs an NPU-enabled VP build)
#   make NPU=0      # scheduler/tick/PLIC demo only (public VP build)

CROSS   ?= riscv-none-elf-
CC      := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump

FREERTOS ?= ../../third_party/FreeRTOS-Kernel
PORT_DIR := $(FREERTOS)/portable/GCC/RISC-V
CHIP_EXT := $(PORT_DIR)/chip_specific_extensions/RISCV_MTIME_CLINT_no_extensions

NPU ?= 1

ARCH    := -march=rv32imac_zicsr -mabi=ilp32
INCS    := -I. -Iinclude -I../common/include \
           -I$(FREERTOS)/include -I$(PORT_DIR) -I$(CHIP_EXT)
CFLAGS  := $(ARCH) -Os -g -ffreestanding -nostdlib -fno-pic -Wall -Wextra \
           -ffunction-sections -fdata-sections $(INCS)
ifeq ($(NPU),1)
CFLAGS  += -DDEMO_NPU=1
endif
LDFLAGS := -T linker.ld -Wl,--gc-sections

KERNEL_SRCS := $(FREERTOS)/tasks.c $(FREERTOS)/list.c $(FREERTOS)/queue.c \
               $(FREERTOS)/portable/MemMang/heap_4.c \
               $(PORT_DIR)/port.c
ASM_SRCS    := startup/start.S $(PORT_DIR)/portASM.S
APP_SRCS    := src/main.c src/libc_min.c bsp/uart.c bsp/plic.c
ifeq ($(NPU),1)
APP_SRCS    += bsp/npu_v4.c
endif

TARGET := freertos_fx1.elf

all: $(TARGET)

$(TARGET): $(ASM_SRCS) $(KERNEL_SRCS) $(APP_SRCS) linker.ld FreeRTOSConfig.h
	@test -f $(FREERTOS)/tasks.c || \
	    { echo "ERROR: FreeRTOS kernel missing - run tools/third_party/setup_third_party.sh"; exit 1; }
	$(CC) $(CFLAGS) $(LDFLAGS) $(ASM_SRCS) $(KERNEL_SRCS) $(APP_SRCS) -lgcc -o $@
	$(OBJDUMP) -d $@ > freertos_fx1.dis

clean:
	rm -f $(TARGET) freertos_fx1.dis

.PHONY: all clean
