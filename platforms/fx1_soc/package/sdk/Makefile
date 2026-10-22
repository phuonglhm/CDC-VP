# SPDX-License-Identifier: Apache-2.0
#
# VP_FX1_V3.0: the FreeRTOS CLI firmware of the FX1 SoC VP, built without
# CDC-VP or CMake. Run from the SDK root or from this directory:
#
#   make -C sw/bootloader/sources/VP_FX1_V3.0 [TOOLDIR=<toolchain>/bin] [-j]
#
#   TOOLDIR  directory of riscv-none-elf-gcc (default: found on PATH)
#   CROSS    tool prefix (default riscv-none-elf-)
#   OUT      output directory (default sw/bootloader/build/VP_FX1_V3.0)
#
# Outputs in $(OUT): freertos_cli.elf, .dis (disassembly), .map, .sections.txt,
# and the objects in obj/. None of them depends on where the SDK is checked out
# or where the toolchain is installed (-ffile-prefix-map; link, objdump and size
# run in $(OUT) on relative names; the toolchain path in the map is normalized):
# a rebuild gives every packaged output byte for byte with the same compiler.
#
#   make ddr3   DDR3 firmware-init examples (BootROM/SRAM images, ddr3/README.md)
#               in $(OUT)/ddr3; run them with test/VP_FX1_V3.0/run_vp.sh ddr3
#   make camera camera examples (CSI-2 RX, I2C0, sensor; camera -> ISP through
#               the camera manager; doc camera/README.md) in $(OUT)/camera; run
#               them with test/VP_FX1_V3.0/run_vp.sh camera / camera-isp
# Same compiler flags, include order and sources as the CDC-VP build that the
# package was tested with (fw/fx1_soc/CMakeLists.txt). Do not change -march /
# -mabi: the VP harts are RV32IMAFC and the BSP, the kernel port and every
# object must agree on ilp32f.

VERSION := VP_FX1_V3.0
HERE    := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
OUT     ?= $(abspath $(HERE)/../../build/$(VERSION))
CROSS   ?= riscv-none-elf-
TOOLDIR ?=
TOOL     = $(if $(TOOLDIR),$(TOOLDIR)/,)$(CROSS)
CC       = $(TOOL)gcc
OBJDUMP  = $(TOOL)objdump
SIZE     = $(TOOL)size

KERNEL  := $(HERE)/third_party/FreeRTOS-Kernel
ARCH    := -march=rv32imafc_zicsr_zifencei -mabi=ilp32f -mcmodel=medany
# --param=min-pagesize=0: MMIO lives at low addresses; without it GCC treats a
# constant address below 4 KiB as null + offset. -Wno-int-to-pointer-cast: the
# upstream RV32 port casts the 64-bit mtimecmp base to a pointer.
CFLAGS  := $(ARCH) -O2 -g -std=c11 -ffreestanding -fno-builtin --param=min-pagesize=0 \
           -ffunction-sections -fdata-sections -Wall -Wextra -Werror \
           -I$(HERE)/bsp -I$(HERE)/include \
           -I$(HERE)/app -I$(KERNEL)/include -I$(KERNEL)/portable/GCC/RISC-V \
           -I$(HERE)/isp_selftest -I$(HERE)/demo \
           -Wno-int-to-pointer-cast
# The CLI with the camera (CSI-2 M6b): `camera` commands, boot check of the
# CSI-2 RX / I2C0 / sensor; without fx1_soc --camera they report it absent.
APPCAM  := -DFX1_FW_HAS_CAMERA -I$(HERE)/drivers/camera -I$(HERE)/camera_isp
# Source paths in __FILE__ (configASSERT) and debug info relative to this
# directory, so the image does not depend on where the SDK is checked out: a
# rebuild anywhere, with the same compiler release, gives the packaged image
# byte for byte.
CFLAGS  += -ffile-prefix-map=$(HERE)=. -ffile-prefix-map=$(HERE)/=
# Same for the toolchain's own headers (lib/gcc/.../include), whose install
# path would otherwise end up in the debug info.
TOOLROOT := $(patsubst %/bin/,%,$(dir $(realpath $(shell command -v $(CC) 2>/dev/null))))
CFLAGS  += $(if $(TOOLROOT),-ffile-prefix-map=$(TOOLROOT)=riscv-none-elf-gcc)
# The linker map names the libraries it searched by their install path.
MAP_FIX := $(if $(TOOLROOT),sed -i -e 's|$(TOOLROOT)/bin/\.\./|riscv-none-elf-gcc/|g' -e 's|$(TOOLROOT)|riscv-none-elf-gcc|g',true)
LDFLAGS := -nostdlib -nostartfiles -Wl,--gc-sections -Wl,--no-warn-rwx-segments

SOURCES := $(HERE)/bsp/crt0.S $(HERE)/bsp/fx1_fw.c \
           $(addprefix $(HERE)/app/,main.c rtos_bsp.c rtos_libc.c cli.c hwcheck.c cmd_fpu.c cmd_dma.c \
                                    cmd_isp.c cmd_camera.c probe.S) \
           $(KERNEL)/tasks.c $(KERNEL)/queue.c $(KERNEL)/list.c $(KERNEL)/portable/MemMang/heap_4.c \
           $(KERNEL)/portable/GCC/RISC-V/port.c $(KERNEL)/portable/GCC/RISC-V/portASM.S \
           $(HERE)/isp_selftest/isp_common.c $(HERE)/drivers/isp/fx1_isp_drv.c \
           $(addprefix $(HERE)/drivers/camera/,fx1_camera.c fx1_isp_hal_mmio.c fx1_i2c.c fx1_ccs_sensor.c \
                                               fx1_csi2_hal_mmio.c) \
           $(HERE)/drivers/csi2/fx1_csi2_drv.c
HEADERS := $(wildcard $(HERE)/bsp/*.h $(HERE)/app/*.h $(HERE)/include/*/*.h $(HERE)/isp_selftest/*.h \
                      $(HERE)/demo/*.h $(KERNEL)/include/*.h $(KERNEL)/portable/GCC/RISC-V/*.h \
                      $(HERE)/drivers/camera/*.h $(HERE)/camera_isp/*.h)

ELF  := $(OUT)/freertos_cli.elf
LDS  := $(OUT)/fx1.ld
# One object per source, so no compiler temporary name reaches the symbol table.
OBJS := $(patsubst $(HERE)/%,$(OUT)/obj/%.o,$(SOURCES))

.PHONY: all clean toolchain
all: $(ELF) $(OUT)/freertos_cli.dis $(OUT)/freertos_cli.sections.txt

toolchain:
	@command -v $(CC) >/dev/null 2>&1 || { echo "error: $(CC) not found; set TOOLDIR=<toolchain>/bin" >&2; exit 1; }
	@$(CC) --version | head -n 1

$(LDS): $(HERE)/bsp/fx1.ld.S $(HERE)/include/fx1/fx1_memory_map.h | toolchain
	@mkdir -p $(OUT)
	$(CC) -E -P -x c -DFX1_LINKER_SCRIPT -I$(HERE)/include $< -o $@

$(OUT)/obj/%.o: $(HERE)/% $(HEADERS) | toolchain
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(APPCAM) -c $< -o $@

$(ELF): $(OBJS) $(LDS)
	cd $(OUT) && $(CC) $(CFLAGS) $(LDFLAGS) -T fx1.ld -Wl,-Map,freertos_cli.map \
		$(patsubst $(OUT)/%,%,$(OBJS)) -lgcc -o freertos_cli.elf
	$(MAP_FIX) $(OUT)/freertos_cli.map

$(OUT)/freertos_cli.dis: $(ELF)
	cd $(OUT) && $(OBJDUMP) -d freertos_cli.elf > freertos_cli.dis

$(OUT)/freertos_cli.sections.txt: $(ELF)
	cd $(OUT) && $(SIZE) -A -x freertos_cli.elf > freertos_cli.sections.txt

# ── DDR3 firmware-init examples (BootROM/SRAM; ddr3/README.md) ───────────────
# Code and read-only data in BootROM, data and stacks in SRAM (bsp/crt0_rom.S,
# bsp/fx1_rom.ld.S); they bring the DDR3 controller up themselves. Run them with
# `fx1_soc --ddr-boot firmware` (run_vp.sh ddr3). The fault schedule of
# fault_recovery.c must equal the --ddr3-fault options in run_vp.sh.
D3OUT    := $(OUT)/ddr3
ROM_LDS  := $(D3OUT)/fx1_rom.ld
D3CFLAGS := $(filter-out -Wno-int-to-pointer-cast,$(CFLAGS)) -I$(HERE)/ddr3
D3COMMON := $(HERE)/bsp/crt0_rom.S $(HERE)/bsp/fx1_fw.c $(HERE)/ddr3/ddr3_fw.c $(HERE)/ddr3/ddr3_probe.S
D3HDRS   := $(wildcard $(HERE)/bsp/*.h $(HERE)/ddr3/*.h $(HERE)/include/*/*.h)
D3ELFS   := $(addprefix $(D3OUT)/,ddr3_fw_init.elf ddr3_fault_recovery.elf ddr3_power_recovery.elf boot_freertos.elf)

.PHONY: ddr3
ddr3: $(D3ELFS)

$(ROM_LDS): $(HERE)/bsp/fx1_rom.ld.S $(HERE)/include/fx1/fx1_memory_map.h | toolchain
	@mkdir -p $(D3OUT)
	$(CC) -E -P -x c -DFX1_LINKER_SCRIPT -I$(HERE)/include $< -o $@

# rom_image(<elf>, <sources>, <extra flags>)
define rom_image
	cd $(D3OUT) && $(CC) $(D3CFLAGS) $(3) $(LDFLAGS) -T fx1_rom.ld $(D3COMMON) $(2) -lgcc -o $(notdir $(1))
endef

$(D3OUT)/ddr3_fw_init.elf: $(HERE)/ddr3/examples/fw_init.c $(D3COMMON) $(D3HDRS) $(ROM_LDS)
	$(call rom_image,$@,$<,)
$(D3OUT)/ddr3_fault_recovery.elf: $(HERE)/ddr3/examples/fault_recovery.c $(D3COMMON) $(D3HDRS) $(ROM_LDS)
	$(call rom_image,$@,$<,-DFAULT_CE_MS=1000 -DFAULT_UE_MS=1010 -DFAULT_TIMEOUT_MS=1020 -DFAULT_FATAL_MS=2000)
$(D3OUT)/ddr3_power_recovery.elf: $(HERE)/ddr3/examples/power_recovery.c $(D3COMMON) $(D3HDRS) $(ROM_LDS)
	$(call rom_image,$@,$<,)
# BootROM loader with freertos_cli.elf (stripped) embedded: DDR3 bring-up, copy, start.
$(D3OUT)/freertos_app.elf: $(ELF) | toolchain
	@mkdir -p $(D3OUT)
	$(TOOL)objcopy --strip-all $< $@
$(D3OUT)/app_image.S: | toolchain
	@mkdir -p $(D3OUT)
	printf '    .section .app_image, "a"\n    .balign 8\n    .global fx1_app_image, fx1_app_image_end\nfx1_app_image:\n    .incbin "freertos_app.elf"\nfx1_app_image_end:\n' > $@
$(D3OUT)/boot_freertos.elf: $(HERE)/ddr3/examples/boot_loader.c $(D3OUT)/app_image.S $(D3OUT)/freertos_app.elf $(D3COMMON) $(D3HDRS) $(ROM_LDS)
	$(call rom_image,$@,$< app_image.S,)

# ── camera examples (CSI-2 RX + D-PHY + sensor, fx1_soc --camera) ───────────
# DDR images like the CLI (bsp/crt0.S, fx1.ld). Same sources and defines as the
# CDC-VP camera smokes (fw/fx1_soc/CMakeLists.txt).
CAMOUT    := $(OUT)/camera
CAMCFLAGS := $(filter-out -Wno-int-to-pointer-cast,$(CFLAGS)) -I$(HERE)/drivers/camera -I$(HERE)/camera
CAMCOMMON := $(HERE)/bsp/crt0.S $(HERE)/bsp/fx1_fw.c $(HERE)/camera/camera.c $(HERE)/drivers/camera/fx1_i2c.c \
             $(HERE)/drivers/camera/fx1_ccs_sensor.c $(HERE)/drivers/camera/fx1_csi2_hal_mmio.c \
             $(HERE)/drivers/csi2/fx1_csi2_drv.c
CAMHDRS   := $(wildcard $(HERE)/bsp/*.h $(HERE)/camera/*.h $(HERE)/drivers/camera/*.h $(HERE)/include/*/*.h)
CAMELFS   := $(addprefix $(CAMOUT)/,camera_raw10.elf camera_raw12.elf camera_recover.elf camera_decerr.elf \
                                    camera_slverr.elf camera_i2c_irq.elf camera_i2c_timeout.elf \
                                    camera_isp_raw12.elf camera_isp_raw10.elf)
# Camera -> ISP through the camera manager (CDC-VP camera_isp_raw12/raw10).
CISPCOMMON := $(HERE)/bsp/crt0.S $(HERE)/bsp/fx1_fw.c $(HERE)/camera_isp/cam_isp.c \
              $(addprefix $(HERE)/drivers/camera/,fx1_camera.c fx1_isp_hal_mmio.c fx1_i2c.c fx1_ccs_sensor.c \
                                                  fx1_csi2_hal_mmio.c) \
              $(HERE)/drivers/csi2/fx1_csi2_drv.c $(HERE)/drivers/isp/fx1_isp_drv.c
CISPHDRS   := $(CAMHDRS) $(wildcard $(HERE)/camera_isp/*.h)

.PHONY: camera
camera: $(CAMELFS)

# cam_image(<elf>, <sources>, <extra flags>)
define cam_image
	@mkdir -p $(CAMOUT)
	cd $(CAMOUT) && $(CC) $(CAMCFLAGS) $(3) $(LDFLAGS) -T $(LDS) $(2) -lgcc -o $(notdir $(1))
endef

$(CAMOUT)/camera_raw10.elf: $(HERE)/camera/raw.c $(CAMCOMMON) $(CAMHDRS) $(LDS)
	$(call cam_image,$@,$(CAMCOMMON) $<,-DCAM_BITS=10 -DCAM_LANES=4 -DCAM_RAW16=0)
$(CAMOUT)/camera_raw12.elf: $(HERE)/camera/raw.c $(CAMCOMMON) $(CAMHDRS) $(LDS)
	$(call cam_image,$@,$(CAMCOMMON) $<,-DCAM_BITS=12 -DCAM_LANES=2 -DCAM_RAW16=1)
$(CAMOUT)/camera_%.elf: $(HERE)/camera/%.c $(CAMCOMMON) $(CAMHDRS) $(LDS)
	$(call cam_image,$@,$(CAMCOMMON) $<,)
$(CAMOUT)/camera_isp_raw12.elf: $(HERE)/camera_isp/stream.c $(CISPCOMMON) $(CISPHDRS) $(LDS)
	$(call cam_image,$@,$< $(CISPCOMMON),-I$(HERE)/camera_isp -DCAM_BITS=12)
$(CAMOUT)/camera_isp_raw10.elf: $(HERE)/camera_isp/stream.c $(CISPCOMMON) $(CISPHDRS) $(LDS)
	$(call cam_image,$@,$< $(CISPCOMMON),-I$(HERE)/camera_isp -DCAM_BITS=10)
$(CAMOUT)/camera_i2c_irq.elf: $(HERE)/camera/i2c_irq.c $(CAMHDRS) $(LDS)
	$(call cam_image,$@,$(HERE)/bsp/crt0.S $(HERE)/bsp/fx1_fw.c $(HERE)/drivers/camera/fx1_i2c.c \
	                    $(HERE)/drivers/camera/fx1_ccs_sensor.c $<,)
$(CAMOUT)/camera_i2c_timeout.elf: $(HERE)/camera/i2c_timeout.c $(CAMHDRS) $(LDS)
	$(call cam_image,$@,$(HERE)/bsp/crt0.S $(HERE)/bsp/fx1_fw.c $(HERE)/drivers/camera/fx1_i2c.c \
	                    $(HERE)/drivers/camera/fx1_ccs_sensor.c $<,)

clean:
	rm -rf $(OUT)/obj $(D3OUT) $(CAMOUT)
	rm -f $(ELF) $(LDS) $(OUT)/freertos_cli.dis $(OUT)/freertos_cli.map $(OUT)/freertos_cli.sections.txt

# Portable policy checks with the host compiler, no VP or cross compiler needed.
HOST_CC ?= /usr/bin/gcc
.PHONY: check-isp-auto
check-isp-auto:
	@mkdir -p $(OUT)
	$(HOST_CC) -std=c99 -Wall -Wextra -Werror -I$(HERE)/app $(HERE)/tests/isp_auto_policy_test.c -o $(OUT)/isp_auto_policy_test
	$(OUT)/isp_auto_policy_test
