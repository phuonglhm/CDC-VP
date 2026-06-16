# uart_platform

## Overview & Memory Map

`uart_platform` is a SystemC/TLM virtual prototype integration test platform for
the UART peripheral. It uses a Bremen RV32 RISC-V CPU, a shared TLM bus router,
RAM, CLINT, PLIC, and the `uart2_tlm` (PL011-compatible) UART model.

| Region | Base | Size | End | Notes |
|---|---:|---:|---:|---|
| CLINT | `0x0200_0000` | `0x10000` | `0x0200_FFFF` | RISC-V local software/timer interrupts |
| PLIC | `0x0C00_0000` | `0x400000` | `0x0C3F_FFFF` | External interrupt controller |
| UART0 | `0x1000_0000` | `0x1000` | `0x1000_0FFF` | UART TX logging and test target |
| RAM | `0x8000_0000` | `0x100000` | `0x800F_FFFF` | Firmware text/data/heap/stack |

## Build and Run Instructions

Run these commands from the repository root.

### 1. Source the toolchain environment

```bash
source tools/third_party/setup_env.sh
```

### 2. Build the bare-metal firmware

```bash
make -C fw/uart_test_riscv clean
make -C fw/uart_test_riscv
```

This produces:

```text
fw/uart_test_riscv/uart_test.elf
```

### 3. Build the hardware platform

#### Step 3a: Configure the project

SystemC version 2.3.4 is strictly required for this project. If `SYSTEMC_HOME`
is already set, CMake uses it; otherwise the command defaults to
`/opt/systemc-2.3.4`.

```bash
cmake -S . -B build/bremen -G Ninja \
  -DCDC_CPU_BACKEND=riscv_vp \
  -DCDC_BUILD_MINI_TLM=OFF \
  -DCDC_BUILD_CPU_EVAL=OFF \
  -DCDC_BUILD_CUSTOM_SOC=ON \
  -DSYSTEMC_INCLUDE_DIR="${SYSTEMC_HOME:-/opt/systemc-2.3.4}/include" \
  -DSYSTEMC_LIBRARY="${SYSTEMC_HOME:-/opt/systemc-2.3.4}/lib/libsystemc.so"
```

#### Step 3b: Compile the target

```bash
cmake --build build/bremen --target uart_platform
```

### 4. Run the simulation

```bash
./build/bremen/platforms/tests/uart_platform/uart_platform \
  -c platforms/tests/uart_platform/configs/default.yaml \
  --fw fw/uart_test_riscv/uart_test.elf \
  --sim-ms 5
```

Expected firmware output:

```text
UART platform start
UART TX test
UART PASS
```

## Test Scenarios

### Phase 1: TX Data Test

The firmware writes a sequence of ASCII characters and a test byte (`0x55`) to
the UART data register. The platform monitors the UART `tx` signal and prints
each transmitted byte to the console.

The data path is:

```text
firmware write to UART_DR
  -> UART TX FIFO
  -> uart2_tlm tx signal
  -> monitor_tx method (platform)
  -> console output
```

The firmware prints `UART PASS` if transmission completes without error.

## UART Register Map

| Register | Offset | Notes |
|---|---:|---|
| `UARTDR` | `0x000` | Data register (TX write / RX read) |
| `UARTFR` | `0x018` | Flag register (TX full, RX empty, busy) |
| `UARTLCR_H` | `0x02C` | Line control register |
| `UARTCR` | `0x030` | Control register |
| `UARTIMSC` | `0x038` | Interrupt mask set/clear |
| `UARTRIS` | `0x03C` | Raw interrupt status |
| `UARTMIS` | `0x040` | Masked interrupt status |
| `UARTICR` | `0x044` | Interrupt clear register |