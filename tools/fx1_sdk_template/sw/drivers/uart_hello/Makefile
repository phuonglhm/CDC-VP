# Build the uart_hello smoke-test driver against the VP_FX1 BSP.
BSP := ../../../../bsp/VP_FX1_SOC
include $(BSP)/bsp.mk

BUILD_DIR := ../../../build/VP_FX1_SOC
TARGET    := $(BUILD_DIR)/uart_hello.elf
SRCS      := main.c

all: $(TARGET)

$(TARGET): $(SRCS) $(BSP_SRCS) $(BSP)/link/riscv.ld
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(LDFLAGS) $(BSP_SRCS) $(SRCS) -o $@
	$(OBJDUMP) -d $@ > $(basename $@).dis

clean:
	rm -f $(TARGET) $(basename $(TARGET)).dis

.PHONY: all clean
