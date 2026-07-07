# bsp.mk - shared build settings for VP_FX1 firmware/drivers.
#
# A driver Makefile sets BSP to the bsp/VP_FX1_SOC/ directory and includes
# this file (path below is from sw/drivers/sources/VP_FX1_SOC/<driver>/):
#
#     BSP := ../../../../bsp/VP_FX1_SOC
#     include $(BSP)/bsp.mk
#     SRCS := main.c
#     TARGET := my_driver.elf
#     $(TARGET): $(SRCS) $(BSP_SRCS)
#     	$(CC) $(CFLAGS) $(LDFLAGS) $(BSP_SRCS) $(SRCS) -o $@
#
# ABI is fixed by the SoC CPU model (Bremen riscv_vp = RV32IMAC / ilp32).
# Changing -march/-mabi will link or crash silently on the VP. Do not.

CROSS   ?= riscv-none-elf-
CC      := $(CROSS)gcc
OBJDUMP := $(CROSS)objdump

# _zicsr: GCC >= 12 no longer implies the CSR instructions in the base ISA
# string; IRQ code (csrw mtvec/mie/mstatus) needs it spelled out. Codegen ABI
# is unchanged - this does not relax the "do not change -march/-mabi" rule.
ARCH    := -march=rv32imac_zicsr -mabi=ilp32
CFLAGS  := $(ARCH) -Os -ffreestanding -nostdlib -fno-pic -Wall -Wextra \
           -I$(BSP)/include/soc -I$(BSP)/include/hal
LDFLAGS := -T $(BSP)/link/riscv.ld -Wl,--gc-sections

# Startup + HAL sources every firmware image links against.
BSP_SRCS := $(BSP)/startup/startup_riscv.S $(BSP)/src/uart.c
