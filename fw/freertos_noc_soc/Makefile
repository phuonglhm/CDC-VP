# SPDX-License-Identifier: Apache-2.0
#
# Step 12 FreeRTOS build profile for platforms/noc_soc.
#
# The proven Bremen RV32 FreeRTOS port and BSP remain in ../freertos_fx1.
# This profile deliberately changes only the platform contract:
#   * NPU and TFLM are disabled;
#   * the final 4 KiB of noc_soc RAM is excluded by this linker's MEMORY region;
#   * the output has its own platform-specific name and ELF contract check.

CROSS ?= riscv-none-elf-
NOC_STEP ?= 128

# Level 125 is the Step 12.7 measurement profile: level 124 plus an RTOS tick
# hook. The hook is opt-in so no signed acceptance image carries it.
ifeq ($(NOC_STEP),125)
STEP_CFLAGS := -DconfigUSE_TICK_HOOK=1
else
STEP_CFLAGS :=
endif

FX1_FW := ../freertos_fx1
TARGET := $(CURDIR)/freertos_noc_soc.elf
DISASSEMBLY := $(CURDIR)/freertos_noc_soc.dis
LINKER_SCRIPT := $(CURDIR)/linker.ld
NOC_MAIN := $(CURDIR)/src/main.c
NOC_APP_SRCS := $(NOC_MAIN) src/libc_min.c bsp/uart.c bsp/plic.c
ifeq ($(NOC_STEP),128)
STEP_CFLAGS += -DFREERTOS_NOC_SOC=1
NOC_APP_SRCS += $(CURDIR)/src/cli.c $(abspath $(FX1_FW))/src/hw_diag.c
endif

all: check

$(TARGET): FORCE
	$(MAKE) -C $(FX1_FW) \
	    CROSS="$(CROSS)" \
	    NPU=0 TFLM=0 \
	    TARGET="$(TARGET)" \
	    DISASSEMBLY="$(DISASSEMBLY)" \
	    LINKER_SCRIPT="$(LINKER_SCRIPT)" \
	    EXTRA_CFLAGS="-DNOC_FREERTOS_STEP=$(NOC_STEP) $(STEP_CFLAGS)" \
	    APP_SRCS="$(NOC_APP_SRCS)"

check: $(TARGET) check_elf_contract.sh
	./check_elf_contract.sh "$(TARGET)" "$(CROSS)readelf"

clean:
	$(RM) "$(TARGET)" "$(DISASSEMBLY)"

FORCE:

.PHONY: all check clean FORCE
