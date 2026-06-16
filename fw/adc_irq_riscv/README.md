# CDC-VP ADC Interrupt Demo

This demo verifies the ADC TLM peripheral inside a small RISC-V virtual
platform. The test uses the Bremen `riscv-vp` CPU backend and open-source
Accellera SystemC 2.3.4.

The verified path is:

```text
RISC-V firmware
  -> CPU TLM initiator
  -> bus_router
  -> ADC MMIO registers
  -> ADC interrupt output
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
make -C fw/adc_irq_riscv
```

This produces:

```text
fw/adc_irq_riscv/adc_irq.elf
fw/adc_irq_riscv/adc_irq.dis
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

Build the ADC platform executable:

```bash
cmake --build build/bremen --target adc_platform
```

Run the simulation:

```bash
./build/bremen/platforms/tests/adc_platform/adc_platform \
  -c platforms/tests/adc_platform/configs/default.yaml \
  --fw fw/adc_irq_riscv/adc_irq.elf \
  --sim-ms 5
```

## What The Demo Builds

The `adc_platform` executable is a SystemC virtual platform containing:

```text
Bremen RISC-V CPU
RAM
UART
CLINT
PLIC
ADC TLM model
bus_router
```

The platform memory map is:

| Region | Base | Purpose |
|---|---:|---|
| RAM | `0x8000_0000` | Firmware code/data/stack |
| UART | `0x1000_0000` | Firmware console output |
| CLINT | `0x0200_0000` | Local timer/software interrupts |
| PLIC | `0x0C00_0000` | External interrupt controller |
| ADC | `0x1006_0000` | ADC MMIO register window |

The ADC interrupt line is connected to PLIC source 1, and the PLIC raises the
CPU machine external interrupt.

## What The Firmware Does

`fw/adc_irq_riscv/src/main.c` runs on the simulated RISC-V CPU. It:

1. Sets the machine trap vector.
2. Configures PLIC source 1 for the ADC interrupt.
3. Enables machine external interrupts.
4. Enables the ADC end-of-conversion interrupt.
5. Starts an ADC conversion by writing the ADC control register.
6. Waits for interrupt using `wfi`.
7. Handles the interrupt, claims PLIC source 1, reads ADC data, and completes
   the interrupt.
8. Prints `ADC PASS` if the ADC status/data result is valid.

Expected successful output includes:

```text
ADC platform start
ADC IRQ
ADC sample=...
ADC PASS
```

## Notes

Use open-source Accellera SystemC 2.3.4 for this demo:

```text
/opt/systemc-2.3.4
```

Do not use the Arm Fast Models SystemC library for this build. Also avoid
SystemC 3.x for the Bremen backend in this repo because the pinned Bremen
`riscv-vp` code uses older SystemC process macros.
