# I2C Verification Platform

## Overview

This platform verifies the I2C TLM IP integrated into CDC-VP.

The platform contains:

* RISC-V CPU (Bremen RV32 backend)
* Bus Router
* RAM
* UART
* CLINT
* PLIC
* I2C TLM IP

Firmware accesses the I2C registers through MMIO and verifies the basic functionality of the IP.

---

## Requirements

The platform was tested with:

* SystemC 2.3.4
* Ninja
* GCC / G++
* RISC-V bare-metal toolchain (`riscv64-unknown-elf-gcc`)

Before building:

```bash
export SYSTEMC_HOME=/path/to/systemc-2.3.4
```

Example:

```bash
export SYSTEMC_HOME=/opt/systemc-2.3.4
```

No SystemC path is hard-coded in the build commands below.

---

## Memory Map

| Peripheral | Base Address |
| ---------- | ------------ |
| CLINT      | 0x02000000   |
| PLIC       | 0x0C000000   |
| UART0      | 0x10000000   |
| I2C0       | 0x10010000   |
| RAM        | 0x80000000   |

---

## Tested Features

### Test 1: CTRL / STATUS Register Access

* Enable I2C host mode
* Read CTRL register
* Read STATUS register
* Verify MMIO access

Expected:

```text
[PASS] CTRL/STATUS
```

### Test 2: Interrupt State Generation

* Enable CMD_COMPLETE interrupt
* Send commands through FDATA FIFO
* Verify interrupt generation
* Verify interrupt clear operation

Expected:

```text
[PASS] I2C INTR_STATE / IRQ POLL
```

### Test 3: FDATA FIFO Command Path

* Send START command
* Send DATA byte
* Send STOP command
* Verify command processing
* Verify CMD_COMPLETE interrupt

Expected:

```text
[PASS] FDATA FIFO COMMAND
```

---

## Build Platform

From CDC-VP root:

```bash
export SYSTEMC_HOME=/opt/systemc-2.3.4

rm -rf build/bremen

cmake -S . -B build/bremen -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCDC_CPU_BACKEND=riscv_vp \
  -DCDC_BUILD_MINI_TLM=OFF \
  -DCDC_BUILD_CPU_EVAL=OFF \
  -DCDC_BUILD_CUSTOM_SOC=ON \
  -DSYSTEMC_INCLUDE_DIR=${SYSTEMC_HOME}/include \
  -DSYSTEMC_LIBRARY=${SYSTEMC_HOME}/lib/libsystemc.so

cmake --build build/bremen --target i2c_platform
```

---

## Build Firmware

```bash
cd fw/i2c_irq_riscv

make clean
make
```

Generated files:

```text
i2c_irq.elf
i2c_irq.dis
```

---

## Run Test

From CDC-VP root:

```bash
./build/bremen/platforms/tests/i2c_platform/i2c_platform \
  -c platforms/tests/i2c_platform/configs/default.yaml \
  --fw fw/i2c_irq_riscv/i2c_irq.elf \
  --sim-ms 5
```

---

## Expected Result

```text
[PASS] CTRL/STATUS
[PASS] I2C INTR_STATE / IRQ POLL
[PASS] FDATA FIFO COMMAND

I2C FULL PASS
```

---

## Notes

The verification confirms:

* MMIO register access
* CTRL register functionality
* STATUS register functionality
* FDATA FIFO command handling
* Interrupt generation
* Interrupt clear behavior

The current test validates interrupt generation through interrupt-state polling and confirms correct I2C IP functionality.
