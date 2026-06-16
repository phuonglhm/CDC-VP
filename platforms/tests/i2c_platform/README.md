# I2C Verification Platform

## Overview

This platform is used to verify the I2C TLM IP integrated into the CDC-VP framework.

The test platform instantiates:

* RISC-V CPU (Bremen rv32 backend)
* Bus Router
* RAM
* UART
* CLINT
* PLIC
* I2C TLM IP

The firmware accesses I2C registers through MMIO and verifies the basic functionality of the I2C peripheral.

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

The firmware verifies:

### Test 1: CTRL / STATUS Register Access

* Enable I2C host mode
* Read back CTRL register
* Read STATUS register
* Verify correct MMIO operation

Expected result:

```text
[PASS] CTRL/STATUS
```

---

### Test 2: Interrupt State Generation

* Enable CMD_COMPLETE interrupt
* Send I2C command sequence through FDATA FIFO
* Verify interrupt state is generated
* Verify interrupt status can be cleared

Expected result:

```text
[PASS] I2C INTR_STATE / IRQ POLL
```

---

### Test 3: FDATA FIFO Command Path

* Push START command
* Push DATA byte
* Push STOP command
* Verify command processing
* Verify CMD_COMPLETE interrupt generation

Expected result:

```text
[PASS] FDATA FIFO COMMAND
```

---

## Build Platform

From CDC-VP root directory:

```bash
rm -rf build/bremen

cmake -S . -B build/bremen -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCDC_CPU_BACKEND=riscv_vp \
  -DCDC_BUILD_MINI_TLM=OFF \
  -DCDC_BUILD_CPU_EVAL=OFF \
  -DCDC_BUILD_CUSTOM_SOC=ON \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so

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

From CDC-VP root directory:

```bash
./build/bremen/platforms/tests/i2c_platform/i2c_platform \
  -c platforms/tests/i2c_platform/configs/default.yaml \
  --fw fw/i2c_irq_riscv/i2c_irq.elf \
  --sim-ms 5
```

---

## Expected Output

```text
[PASS] CTRL/STATUS
[PASS] I2C INTR_STATE / IRQ POLL
[PASS] FDATA FIFO COMMAND

I2C FULL PASS
```

---

## Notes

The I2C IP successfully:

* Handles MMIO register accesses
* Processes FDATA FIFO commands
* Generates CMD_COMPLETE interrupt status
* Supports interrupt enable and clear operations

The current verification flow validates the interrupt state through polling and confirms correct I2C IP functionality.
