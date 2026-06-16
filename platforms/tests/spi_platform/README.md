# spi_platform

## Overview & Memory Map

`spi_platform` is a SystemC/TLM virtual prototype test platform for the SPI
peripheral. It uses a Bremen `riscv-vp` RV32 CPU, a shared TLM bus router, RAM,
CLINT, PLIC, UART0 for firmware logging, and the cleaned-up upstream
`cdc::components::spi_tlm` peripheral model.

The SPI interrupt output is routed to PLIC source ID 3. UART0 is preserved at
`0x1000_0000` so bare-metal firmware can print progress and pass/fail messages.

| Region | Base | Size | End | Notes |
|---|---:|---:|---:|---|
| CLINT | `0x0200_0000` | `0x10000` | `0x0200_FFFF` | RISC-V local software/timer interrupts |
| PLIC | `0x0C00_0000` | `0x400000` | `0x0C3F_FFFF` | External interrupt controller |
| UART0 | `0x1000_0000` | `0x1000` | `0x1000_0FFF` | UART TX logging |
| SPI0 | `0x1002_0000` | `0x1000` | `0x1002_0FFF` | SPI register window, PLIC source ID 3 |
| RAM | `0x8000_0000` | `0x100000` | `0x800F_FFFF` | Firmware text/data/heap/stack |

## Build and Run Instructions

Run these commands from this directory:

```bash
cd /home/hoangquan/workspace/CDC-VP/platforms/tests/spi_platform
```

Before building, source the repository toolchain environment in the current
terminal:

```bash
source ../../../tools/third_party/setup_env.sh
```

### Step 1: Build the bare-metal firmware

```bash
make -C ../../../fw/spi_test_riscv clean
make -C ../../../fw/spi_test_riscv
```

This produces:

```text
../../../fw/spi_test_riscv/spi_test.elf
```

### Step 2: Build the hardware platform

```bash
cmake --build ../../../build/bremen --target spi_platform
```

If the `build/bremen` tree has not been configured yet, configure it once from
the repository root:

```bash
cd /home/hoangquan/workspace/CDC-VP
 cmake -S . -B build/bremen -G Ninja \                     
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCDC_CPU_BACKEND=riscv_vp \
  -DCDC_BUILD_MINI_TLM=OFF \
  -DCDC_BUILD_CPU_EVAL=OFF \
  -DCDC_BUILD_CUSTOM_SOC=ON \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so
```

Then return to this directory before using the run command below.

### Step 3: Run the simulation

```bash
./../../../build/bremen/platforms/tests/spi_platform/spi_platform \
  -c configs/default.yaml \
  --fw ../../../fw/spi_test_riscv/spi_test.elf \
  --sim-ms 5
```

Expected firmware output includes UART log lines ending with either:

```text
SPI PASS
```

or:

```text
SPI FAIL
```

## Test Explanation

### Hardware Loopback

The SPI IP exposes two TLM sockets:

| Socket | Direction | Platform Binding |
|---|---|---|
| `from_apb_socket` | target | Connected to the bus router at `0x1002_0000` |
| `to_peri_socket` | initiator | Connected to a local `spi_loopback_target` |

There is no physical SPI slave model in this platform yet. To keep the test
self-contained, `spi.to_peri_socket` is bound to a local loopback placeholder
called `spi_loopback_target`.

When firmware writes data to the SPI data register, the SPI model moves that
data through its TX path and sends a TLM transaction through `to_peri_socket`.
The loopback target accepts the transaction and leaves the payload unchanged.
From the firmware point of view, data transmitted on MOSI is echoed back on
MISO and becomes available in the SPI RX FIFO.

This makes the platform useful for checking the basic integration path:

- CPU MMIO access to SPI registers
- SPI TX FIFO behavior
- SPI RX FIFO behavior
- SPI interrupt assertion
- PLIC routing to the RISC-V CPU
- UART0 reporting from bare-metal firmware

### The "Queue 4 Words" Mechanism

The upstream SPI IP has an RX FIFO depth of 8 entries. Its RX interrupt logic
asserts the RX interrupt condition only when the RX FIFO is at least half full:

```text
rx_fifo.num_available() >= FIFO_SIZE / 2
```

With `FIFO_SIZE = 8`, the RX threshold is 4 entries. For that reason, the
firmware intentionally writes 4 words to the SPI data register instead of only
one. The current test pattern starts at `0xAA`, so the transmitted values are:

```text
0xAA, 0xAB, 0xAC, 0xAD
```

Each write enters the SPI TX FIFO. The SPI model transmits each value through
the loopback target and places the echoed value into the RX FIFO. When the RX
FIFO reaches 4 items, the SPI interrupt line is asserted.

The firmware uses 16-bit MMIO accesses for SPI registers because the SPI model
accepts register transactions up to 2 bytes wide.

### Interrupt & Verification Flow

The firmware configures PLIC source ID 3 for SPI:

- It sets the priority for source 3.
- It enables bit 3 in the PLIC enable register.
- It sets the PLIC threshold to `0`.
- It enables machine external interrupts through `mie.MEIE`.
- It enables global machine interrupts through `mstatus.MIE`.

After SPI is configured, firmware writes the 4-word test pattern to the SPI
data register. Once the RX FIFO threshold is reached, SPI asserts `intr`. The
platform wires this signal to PLIC source ID 3, so the PLIC raises MEIP to the
CPU.

The CPU trap handler then:

1. Reads `mcause` and checks for machine external interrupt, `mcause == 11`.
2. Reads the PLIC claim register.
3. Verifies that the claimed interrupt ID is `3`.
4. Reads the SPI status register `SR`.
5. Reads the SPI data register `DR` if RX-not-empty is set.
6. Compares the looped-back RX value against the expected TX value.
7. Clears SPI interrupt state through the `ICR` register.
8. Completes the PLIC interrupt by writing claim ID `3` back to the claim/complete register.
9. Sets a volatile completion flag so `main` can print the final result.

If the looped-back data matches the expected value, firmware prints:

```text
SPI PASS
```

Otherwise it prints:

```text
SPI FAIL
```
