<!-- Author: hoangv11 -->

# CDC-VP DMIC Test Demo

This demo verifies the DMIC (Digital Microphone) TLM peripheral inside a small RISC-V virtual
platform. The test uses the Bremen `riscv-vp` CPU backend and open-source
Accellera SystemC 2.3.4.

The verified path is:

```text
RISC-V firmware
  -> CPU TLM initiator
  -> bus_router
  -> DMIC MMIO registers
  -> DMIC watermark interrupt output
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
make -C fw/dmic_test_riscv
```

This produces:

```text
fw/dmic_test_riscv/dmic_test.elf
fw/dmic_test_riscv/dmic_test.dis
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

Build the DMIC platform executable:

```bash
cmake --build build/bremen --target dmic_platform
```

Run the simulation:

```bash
./build/bremen/platforms/tests/dmic_platform/dmic_platform \
  -c platforms/tests/dmic_platform/configs/default.yaml \
  --fw fw/dmic_test_riscv/dmic_test.elf \
  --sim-ms 5
```

## What The Demo Builds

The `dmic_platform` executable is a SystemC virtual platform containing:

```text
Bremen RISC-V CPU
RAM
UART
CLINT
PLIC
DMIC TLM model (DmicTLM)
bus_router
```

The platform memory map is:

| Region | Base | Purpose |
|---|---:|---|
| RAM | `0x8000_0000` | Firmware code/data/stack |
| UART | `0x1000_0000` | Firmware console output |
| CLINT | `0x0200_0000` | Local timer/software interrupts |
| PLIC | `0x0C00_0000` | External interrupt controller |
| DMIC | `0x1006_0000` | DMIC MMIO register window |

The DMIC interrupt line is connected to PLIC source 1, and the PLIC raises the
CPU machine external interrupt.

## What The Firmware Does

`fw/dmic_test_riscv/src/main.c` runs on the simulated RISC-V CPU. It:

1. Sets the machine trap vector.
2. Configures PLIC source 1 for the DMIC interrupt.
3. Enables machine external interrupts.
4. **Watermark Configuration**: Sets the DMIC FIFO watermark to 1.
5. **Enables DMIC**: Configures decimation to 64, enables interrupts, and powers on DMIC (`DMIC_CONTROL`).
6. **Waits for Interrupt**: Uses `wfi` to wait for the watermark interrupt triggered by the audio data incoming from the platform PDM stream.
7. **Handles the Interrupt**: Claims PLIC source 1, reads the digitized PCM sample from the DMIC FIFO, clears the watermark interrupt source (`DMIC_INT_CLR`), disables DMIC interrupts to avoid CPU starvation, and completes the PLIC interrupt.
8. Prints `DMIC PASS` if the sample is successfully captured and processed.

Expected successful output includes:

```text
DMIC platform start
WRITE PLIC_PRIORITY1 [0x0C000004] <= 0x00000001
WRITE PLIC_ENABLE [0x0C002000] <= 0x00000002
WRITE PLIC_THRESHOLD [0x0C200000] <= 0x00000000
WRITE DMIC_FIFO_WM [0x1006000C] <= 0x00000001
WRITE DMIC_CONTROL [0x10060000] <= 0x00004003
READ  PLIC_CLAIM [0x0C200004] => 0x00000001
READ  DMIC_DATA [0x10060008] => 0x00000008
DMIC IRQ triggered
DMIC sample=0x00000008
WRITE DMIC_INT_CLR [0x10060010] <= 0x00000002
...
DMIC PASS
```

To quickly verify if the test has passed, do:

```bash
./build/bremen/platforms/tests/dmic_platform/dmic_platform \
  -c platforms/tests/dmic_platform/configs/default.yaml \
  --fw fw/dmic_test_riscv/dmic_test.elf \
  --sim-ms 5 | grep "PASS"
```

## Notes

Use open-source Accellera SystemC 2.3.4 for this demo:

```text
/opt/systemc-2.3.4
```
