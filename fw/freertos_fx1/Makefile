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
CXX     := $(CROSS)g++
OBJDUMP := $(CROSS)objdump
OBJCOPY := $(CROSS)objcopy

FREERTOS ?= ../../third_party/FreeRTOS-Kernel
PORT_DIR := $(FREERTOS)/portable/GCC/RISC-V
CHIP_EXT := $(PORT_DIR)/chip_specific_extensions/RISCV_MTIME_CLINT_no_extensions

NPU ?= 1
TFLM ?= 0
EXTRA_CFLAGS ?=
TARGET ?= freertos_fx1.elf
LINKER_SCRIPT ?= linker.ld
DISASSEMBLY ?= freertos_fx1.dis

ARCH    := -march=rv32imac_zicsr -mabi=ilp32
INCS    := -I. -Iinclude -Imodels -I../common/include \
           -I$(FREERTOS)/include -I$(PORT_DIR) -I$(CHIP_EXT)
CFLAGS  := $(ARCH) -Os -g -ffreestanding -nostdlib -fno-pic -Wall -Wextra \
           -ffunction-sections -fdata-sections $(INCS) $(EXTRA_CFLAGS)
ifeq ($(NPU),1)
CFLAGS  += -DDEMO_NPU=1
endif

TFLM_ROOT   ?= ../../third_party/tflite-micro
TFLM_GENDIR := $(TFLM_ROOT)/gen/riscv32_generic_rv32imac_zicsr_default_gcc
TFLM_LIB    := $(TFLM_GENDIR)/lib/libtensorflow-microlite.a
TFLM_DL     := $(TFLM_ROOT)/tensorflow/lite/micro/tools/make/downloads
TFLM_MODEL  := $(TFLM_ROOT)/tensorflow/lite/micro/examples/hello_world/models/hello_world_int8.tflite
TFLM_BUILD  := build/tflm
TFLM_APP_OBJ          := $(TFLM_BUILD)/tflm_cpu.o
TFLM_MODEL_OBJ        := $(TFLM_BUILD)/hello_world_model.o
TFLM_SIMPLE_MODEL_OBJ := $(TFLM_BUILD)/secda_simple_model_data.o
TFLM_NPU_CONV_OBJ     := $(TFLM_BUILD)/tflm_npu_conv.o

ifeq ($(TFLM),1)
CFLAGS += -DDEMO_TFLM=1 -DTF_LITE_STATIC_MEMORY \
          -DTF_LITE_STRIP_ERROR_STRINGS -DNDEBUG -I$(TFLM_ROOT) \
          -I$(TFLM_DL)/flatbuffers/include -I$(TFLM_DL)/gemmlowp \
          -I$(TFLM_DL)/ruy
TFLM_OBJS := $(TFLM_APP_OBJ) $(TFLM_MODEL_OBJ) $(TFLM_SIMPLE_MODEL_OBJ)
ifeq ($(NPU),1)
TFLM_OBJS += $(TFLM_NPU_CONV_OBJ)
endif
TFLM_LINK := $(TFLM_LIB) -lm
endif

CXXFLAGS := $(filter-out -ffreestanding,$(CFLAGS)) -std=c++17 \
            -fno-exceptions -fno-rtti -fno-threadsafe-statics \
            -fno-use-cxa-atexit -Wno-unused-parameter
LDFLAGS := -T $(LINKER_SCRIPT) -Wl,--gc-sections

KERNEL_SRCS := $(FREERTOS)/tasks.c $(FREERTOS)/list.c $(FREERTOS)/queue.c \
               $(FREERTOS)/portable/MemMang/heap_4.c \
               $(PORT_DIR)/port.c
ASM_SRCS    := startup/start.S $(PORT_DIR)/portASM.S
APP_SRCS    ?= src/main.c src/cli.c src/hw_diag.c src/libc_min.c \
               bsp/uart.c bsp/plic.c
ifeq ($(NPU),1)
APP_SRCS    += bsp/npu_v4.c
endif

all: $(TARGET)

$(TFLM_APP_OBJ): src/tflm_cpu.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TFLM_MODEL_OBJ): $(TFLM_MODEL)
	@mkdir -p $(TFLM_BUILD)
	cp $< $(TFLM_BUILD)/hello_world_int8.tflite
	cd $(TFLM_BUILD) && $(OBJCOPY) -I binary -O elf32-littleriscv -B riscv \
	    --set-section-alignment .data=16 \
	    --rename-section .data=.rodata,alloc,load,readonly,data,contents \
	    hello_world_int8.tflite $(notdir $@)

$(TFLM_SIMPLE_MODEL_OBJ): models/secda_simple_model_model_data.cc \
                          models/secda_simple_model_model_data.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TFLM_NPU_CONV_OBJ): src/tflm_npu_conv.cpp include/npu_v4.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TFLM_LIB):
	@echo "ERROR: TFLM library missing - run tools/third_party/build_tflm_riscv.sh"
	@false

$(TARGET): $(ASM_SRCS) $(KERNEL_SRCS) $(APP_SRCS) $(TFLM_OBJS) \
           $(TFLM_LINK) $(LINKER_SCRIPT) FreeRTOSConfig.h
	@test -f $(FREERTOS)/tasks.c || \
	    { echo "ERROR: FreeRTOS kernel missing - run tools/third_party/setup_third_party.sh"; exit 1; }
	$(CC) $(CFLAGS) $(LDFLAGS) $(ASM_SRCS) $(KERNEL_SRCS) $(APP_SRCS) \
	    $(TFLM_OBJS) $(TFLM_LINK) -lgcc -o $@
	$(OBJDUMP) -d $@ > $(DISASSEMBLY)

clean:
	rm -rf $(TARGET) $(DISASSEMBLY) build

.PHONY: all clean
