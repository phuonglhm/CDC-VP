# VP_FX1 SoC SDK (firmware handover)

Virtual-platform delivery of the **VP_FX1 Full SoC** for firmware/driver
development. Treat the VP as the chip: you run it, you write drivers against its
fixed register/IRQ ABI. See `vp/VERSION` for the exact CDC-VP source it was built from.

The tree is organised per-SoC under a `VP_FX1_SOC` namespace so additional SoC
targets can be added later without disturbing this one. Sources, build outputs,
and run scripts are kept separate (`sources/` → `build/`, launched from `test/`).

## Layout

```
FX1/
  vp/
    bin/VP_FX1_SOC/                self-contained VP executable (rpath=$ORIGIN)
                                   + bundled libsystemc.so* (no host install needed)
    configs/VP_FX1_SOC/            SoC memory/IRQ configuration (default.yaml)
    doc/VP_FX1_SOC/                SoC-level docs (memory map, interrupt policy,
                                   BOOTFLOW_GUIDE.md: SoC quick reference
                                   + ROM-code boot contract)
    licenses/                      third-party notices (SystemC, riscv-vp, SoftFloat)
    src/VP_FX1_SOC/                (reserved: per-SoC VP source, if ever shipped)
    VERSION                        CDC-VP git SHA + build date + ABI
  sw/
    bsp/VP_FX1_SOC/                SoC ABI + HAL (shared by all VP_FX1 firmware)
      include/soc/                 soc_memory_map.h, soc_irq_map.h  (the ABI)
      include/soc/regs/            soc_regs_<ip>.h  (clean, model-verified offsets)
      include/hal/                 mmio.h, uart.h
      regref/<ip>_tlm/             per-IP register + behaviour docs (read-only)
      src/uart.c                   polled console HAL
      startup/startup_riscv.S      reset entry (stack, BSS clear, call main)
      link/riscv.ld                link base 0x80000000
      bsp.mk                       shared build flags (RV32IMAC / ilp32)
      toolchain.md                 toolchain + ABI notes
    drivers/
      sources/VP_FX1_SOC/          driver source trees
        uart_hello/                end-to-end smoke test / driver template
      build/VP_FX1_SOC/            driver build outputs (*.elf, *.dis)
    bootloader/
      sources/VP_FX1_SOC/          bootloader source trees (your work)
      build/VP_FX1_SOC/            bootloader build outputs
      scripts/VP_FX1_SOC/          bootloader helper scripts
      test/VP_FX1_SOC/run_vp.sh    VP launcher (loads a firmware/driver ELF)
```

## Prerequisites

- Linux x86-64 host (glibc / libstdc++ from the distro).
- RISC-V bare-metal toolchain `riscv-none-elf-` on PATH (see `sw/bsp/VP_FX1_SOC/toolchain.md`).
- No SystemC install required - the runtime is bundled next to the VP binary.

## 60-second bring-up

Run from the FX1 root:

```bash
# 0. Toolchain on PATH (adjust to your install)
export PATH=/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin:$PATH

# 1. Build the smoke-test driver  (source -> sw/drivers/build/VP_FX1_SOC/uart_hello.elf)
make -C sw/drivers/sources/VP_FX1_SOC/uart_hello

# 2. Run it on the VP  (no arg = load the default uart_hello.elf built above)
sw/bootloader/test/VP_FX1_SOC/run_vp.sh
# Expect on the console:
#   VP_FX1 SDK: UART console up
#   driver template OK
```

`run_vp.sh` usage:

```bash
run_vp.sh                 # load the default uart_hello ELF from build/VP_FX1_SOC
run_vp.sh path/to/app.elf # load a specific ELF (entry = _start)
run_vp.sh app.elf --sim-ms 20   # extra args pass straight through to the VP
run_vp.sh --no-fw         # elaboration smoke run (SoC banner only, no firmware)

# ROM-code boot flow (details: vp/doc/VP_FX1_SOC/BOOTFLOW_GUIDE.md):
run_vp.sh bootrom.elf --int-flash app.bin --boot-pin low     # strap LOW: boot IFLASH app
run_vp.sh bootrom.elf --boot-pin high --uart0-socket 5577 --uart0-wait --sim-ms 60000
run_vp.sh bootrom.elf --boot-pin high --spi-flash image.bin --sim-ms 300
```

## Writing a driver

1. Copy `sw/drivers/sources/VP_FX1_SOC/uart_hello/` as a starting point.
2. Include `soc_memory_map.h` / `soc_irq_map.h` - **never hard-code addresses**.
3. Access registers via `hal/mmio.h` (`mmio_read32/write32`, all 32-bit LE).
4. For interrupts: enable `mstatus.MIE` + the relevant `mie` bit, install
   `mtvec`, then use the CLINT (timer) or PLIC claim/complete flow. PLIC source
   IDs and interrupt cause codes are in `soc_irq_map.h`.
5. Build with `make -C <your-driver-dir>` (output lands in
   `sw/drivers/build/VP_FX1_SOC/`), then run with `run_vp.sh <elf>`.

Each driver Makefile points `BSP` at `sw/bsp/VP_FX1_SOC` and includes `bsp.mk`,
which fixes the ABI (`-march=rv32imac -mabi=ilp32`) and adds the startup + HAL
sources. Do not change `-march`/`-mabi`: a mismatch links or crashes silently
on the VP.

## What is and isn't modeled

- **Present & driver-ready:** UARTx2, I2Cx2, SPIx2, TIMERx2, WDT, PWM, DMA,
  TRNG, CMU, PMU, DMIC, OTP, QSPI(+NOR flash), RTC, ADC, GPIO, CLINT, PLIC.
- **Boot hardware:** BOOTROM 64 KiB @ `0x0` (read-only, ROM-code entry 0x0),
  IFLASH 4 MiB @ `0x0400_0000` (read-only XIP window), GPIO0 pin 1 boot strap,
  NOR flash behind SPI0 (SW chip-select via `SSPCSR @ +0x28`), UART0 host
  bridge (`--uart0-socket` / `--uart0-rx-file`).
- **Reserved (no model, no IRQ):** ISP0/VPU0/NPU0 windows. Do not write drivers
  that expect them to run.
- **Functional, not timing-accurate.** Loosely-timed model: validate register
  semantics, IRQ ordering, and data movement - not cycle timing or WCET.
- Single hart, M-mode, no MMU, no caches.

## Per-IP register map

Two levels, both under `sw/bsp/VP_FX1_SOC` - one to **read**, one to **compile**:

- **Behaviour docs (read)** `regref/<ip>_tlm/README.md` - what each register and
  bit-field does, for every IP. Documentation only; you read these to understand
  the hardware, you do not `#include` them.
- **Clean C headers (compile)** `include/soc/regs/soc_regs_<ip>.h` - the same
  offsets + bit-fields as plain macros (`CDC_<IP>_<REG>`) that your driver
  `#include`s and compiles against, so there are no magic numbers and one place
  to change if the SoC moves a register. Every offset is verified against the VP
  model when the SDK is packed, so a clean header cannot silently drift from the
  chip. Shipped so far: `timer`, `i2c`, `dma` (UART is covered by the HAL).

So: read `regref/` to learn the register, then use `soc_regs_<ip>.h` to program it.

```c
#include "soc_memory_map.h"      /* CDC_I2C0_BASE          */
#include "regs/soc_regs_i2c.h"   /* CDC_I2C_CTRL, fields   */
#include "mmio.h"

mmio_write32(CDC_I2C0_BASE, CDC_I2C_CTRL, CDC_I2C_CTRL_ENABLEHOST);
```

IPs that only have a `regref/` doc today (SPI, QSPI, WDT, TRNG, RTC, ADC, PWM,
DMIC, OTP, PMU, CMU, ...) have no clean header yet: read the doc and either
request a `soc_regs_<ip>.h` from the VP team or define the offsets locally in
your driver.

## Third-party licenses

The VP executable statically links the Bremen riscv-vp ISS (MIT) and Berkeley
SoftFloat-3 (BSD-3-Clause), and ships the Accellera SystemC runtime
(Apache-2.0). Keep `vp/licenses/` with the tree whenever this SDK is passed on.
