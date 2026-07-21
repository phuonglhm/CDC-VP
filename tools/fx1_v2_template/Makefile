# SPDX-License-Identifier: Apache-2.0
#
# Relocatable FreeRTOS/TFLM firmware build for the VP_FX1_V2.0 handover.

PACKAGE_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
OUTPUT_DIR  ?= $(abspath $(PACKAGE_DIR)/../../build/VP_FX1_V2.0)

CROSS   ?= riscv-none-elf-
CC      := $(CROSS)gcc
CXX     := $(CROSS)g++
OBJDUMP := $(CROSS)objdump
OBJCOPY := $(CROSS)objcopy

FREERTOS ?= $(PACKAGE_DIR)/third_party/FreeRTOS-Kernel
PORT_DIR := $(FREERTOS)/portable/GCC/RISC-V
CHIP_EXT := $(PORT_DIR)/chip_specific_extensions/RISCV_MTIME_CLINT_no_extensions

NPU  ?= 1
TFLM ?= 1

ARCH := -march=rv32imac_zicsr -mabi=ilp32
INCS := -I$(PACKAGE_DIR) -I$(PACKAGE_DIR)/include -I$(PACKAGE_DIR)/models \
        -I$(PACKAGE_DIR)/common/include -I$(FREERTOS)/include \
        -I$(PORT_DIR) -I$(CHIP_EXT)
CFLAGS := $(ARCH) -Os -g -ffreestanding -nostdlib -fno-pic -Wall -Wextra \
          -ffunction-sections -fdata-sections $(INCS)
ifeq ($(NPU),1)
CFLAGS += -DDEMO_NPU=1
endif

TFLM_ROOT   ?= $(PACKAGE_DIR)/third_party/tflite-micro
TFLM_LIB    ?= $(TFLM_ROOT)/prebuilt/libtensorflow-microlite.a
TFLM_DL     := $(TFLM_ROOT)/tensorflow/lite/micro/tools/make/downloads
TFLM_MODEL  := $(TFLM_ROOT)/tensorflow/lite/micro/examples/hello_world/models/hello_world_int8.tflite
TFLM_BUILD  := $(OUTPUT_DIR)/obj/tflm
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

TARGET := $(OUTPUT_DIR)/freertos_fx1.elf
DIS    := $(OUTPUT_DIR)/freertos_fx1.dis
BIN    := $(OUTPUT_DIR)/freertos_fx1.bin
MAP    := $(OUTPUT_DIR)/freertos_fx1.map
SECTIONS := $(OUTPUT_DIR)/freertos_fx1.sections.txt
LDFLAGS := -T $(PACKAGE_DIR)/linker.ld -Wl,--gc-sections,-Map,$(MAP)

KERNEL_SRCS := $(FREERTOS)/tasks.c $(FREERTOS)/list.c $(FREERTOS)/queue.c \
               $(FREERTOS)/portable/MemMang/heap_4.c $(PORT_DIR)/port.c
ASM_SRCS := $(PACKAGE_DIR)/startup/start.S $(PORT_DIR)/portASM.S
APP_SRCS := $(PACKAGE_DIR)/src/main.c $(PACKAGE_DIR)/src/cli.c \
            $(PACKAGE_DIR)/src/hw_diag.c $(PACKAGE_DIR)/src/libc_min.c \
            $(PACKAGE_DIR)/bsp/uart.c $(PACKAGE_DIR)/bsp/plic.c
ifeq ($(NPU),1)
APP_SRCS += $(PACKAGE_DIR)/bsp/npu_v4.c
endif

all: $(TARGET)

$(TFLM_APP_OBJ): $(PACKAGE_DIR)/src/tflm_cpu.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TFLM_MODEL_OBJ): $(TFLM_MODEL)
	@mkdir -p $(dir $@)
	cp $< $(TFLM_BUILD)/hello_world_int8.tflite
	cd $(TFLM_BUILD) && $(OBJCOPY) -I binary -O elf32-littleriscv -B riscv \
	    --set-section-alignment .data=16 \
	    --rename-section .data=.rodata,alloc,load,readonly,data,contents \
	    hello_world_int8.tflite $(notdir $@)

$(TFLM_SIMPLE_MODEL_OBJ): $(PACKAGE_DIR)/models/secda_simple_model_model_data.cc \
                          $(PACKAGE_DIR)/models/secda_simple_model_model_data.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TFLM_NPU_CONV_OBJ): $(PACKAGE_DIR)/src/tflm_npu_conv.cpp \
                      $(PACKAGE_DIR)/include/npu_v4.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(ASM_SRCS) $(KERNEL_SRCS) $(APP_SRCS) $(TFLM_OBJS) \
           $(TFLM_LINK) $(PACKAGE_DIR)/linker.ld $(PACKAGE_DIR)/FreeRTOSConfig.h
	@test -f $(FREERTOS)/tasks.c || \
	    { echo "ERROR: packaged FreeRTOS source is missing"; exit 1; }
	@if [ "$(TFLM)" = "1" ] && [ ! -f "$(TFLM_LIB)" ]; then \
	    echo "ERROR: packaged TFLM library is missing: $(TFLM_LIB)"; exit 1; \
	fi
	@mkdir -p $(OUTPUT_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(ASM_SRCS) $(KERNEL_SRCS) $(APP_SRCS) \
	    $(TFLM_OBJS) $(TFLM_LINK) -lgcc -o $@
	$(OBJDUMP) -d $@ > $(DIS)
	$(OBJCOPY) -O binary $@ $(BIN)
	$(OBJDUMP) -h -t $@ > $(SECTIONS)

clean:
	rm -rf $(OUTPUT_DIR)/obj $(TARGET) $(DIS) $(BIN) $(MAP) $(SECTIONS)

.PHONY: all clean
