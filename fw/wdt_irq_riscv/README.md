# CDC-VP WDT Interrupt Demo

This demo verifies the WDT (Watchdog Timer) TLM peripheral inside a small RISC-V virtual
platform. The test uses the Bremen `riscv-vp` CPU backend and open-source
Accellera SystemC 2.3.4.

The verified path is:

```text
RISC-V firmware
  -> CPU TLM initiator
  -> bus_router
  -> WDT MMIO registers
  -> WDT interrupt output
  -> PLIC source 1
  -> CPU machine external interrupt
  -> firmware trap handler
```

## One-Time Setup

From the repository root:

```bash
./tools/third_party/setup_third_party.sh
source ./tools/third_party/setup_env.sh
```

`setup_third_party.sh` clones the external CPU model repositories into
`third_party/`, including Bremen `riscv-vp`.

`setup_env.sh` prepares the shell environment for this repo. It selects the
host compilers and adds the RISC-V bare-metal toolchain to `PATH`.

## Build And Run

Build the bare-metal RISC-V firmware:

```bash
make -C fw/wdt_irq_riscv
```

This produces:

```text
fw/wdt_irq_riscv/wdt_irq.elf
fw/wdt_irq_riscv/wdt_irq.dis
```

Configure the virtual platform build:

```bash
cmake -S . -B build/bremen -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCDC_CPU_BACKEND=riscv_vp \
  -DCDC_BUILD_MINI_TLM=OFF \
  -DCDC_BUILD_CPU_EVAL=OFF \
  -DCDC_BUILD_CUSTOM_SOC=ON \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib-linux64/libsystemc.so
```

Build the WDT platform executable:

```bash
cmake --build build/bremen --target wdt_platform
```

Run the simulation:

```bash
./build/bremen/platforms/tests/wdt_platform/wdt_platform \
  -c platforms/tests/wdt_platform/configs/default.yaml \
  --fw fw/wdt_irq_riscv/wdt_irq.elf \
  --sim-ms 5
```

## What The Demo Builds

The `wdt_platform` executable is a SystemC virtual platform containing:

```text
Bremen RISC-V CPU
RAM
UART
CLINT
PLIC
WDT TLM model (wdt_tlm)
bus_router
```

The platform memory map is:

| Region | Base | Purpose |
|---|---:|---|
| RAM | `0x8000_0000` | Firmware code/data/stack |
| UART | `0x1000_0000` | Firmware console output |
| CLINT | `0x0200_0000` | Local timer/software interrupts |
| PLIC | `0x0C00_0000` | External interrupt controller |
| WDT | `0x1004_0000` | WDT MMIO register window |

The WDT interrupt line is connected to PLIC source 1, and the PLIC raises the
CPU machine external interrupt.

## What The Firmware Does

`fw/wdt_irq_riscv/src/main.c` runs on the simulated RISC-V CPU. It:

1. Sets the machine trap vector.
2. Configures PLIC source 1 for the WDT interrupt.
3. Enables machine external interrupts.
4. **Tests Lock Mechanism**: Verifies that `WDT_LOAD` is read-only when locked.
5. **Unlocks and Loads**: Unlocks the registers and sets a timeout value (10,000 ticks).
6. **Verifies Countdown**: Polls `WDT_VALUE` to confirm the timer is decrementing.
7. **Waits for Interrupt**: Uses `wfi` to wait for the watchdog timeout.
8. **Handles the Interrupt**: Claims PLIC source 1, clears the WDT interrupt at the source (`WDT_INTCLR`), and completes the PLIC interrupt.
9. Prints `WDT TEST PASS` if the interrupt was successfully triggered and cleared.

Expected successful output includes:

```text
WDT platform start
--- Testing Lock Mechanism ---
SUCCESS: WDT_LOAD write blocked while locked.
--- Unlocking and Loading ---
SUCCESS: WDT_LOAD write allowed after unlock.
--- Starting WDT ---
Waiting for WDT Interrupt...
WDT IRQ triggered!
WDT TEST PASS
```

The will continue to run even after the test has passed because as to model watchdog's continuous
behaviour. To quickly verify if the test has passed, do:

```bash
./build/bremen/platforms/tests/wdt_platform/wdt_platform \
  -c platforms/tests/wdt_platform/configs/default.yaml \
  --fw fw/wdt_irq_riscv/wdt_irq.elf \
  --sim-ms 5 | grep "PASS"
```

## Notes

Use open-source Accellera SystemC 2.3.4 for this demo:

```text
/opt/systemc-2.3.4
```

The `wdt_tlm` component is a standardized model of the DesignWare Watchdog Timer, adapted to match the project's TLM conventions.
