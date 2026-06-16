//Author: QuanNH107
//Verified by: HoangV11


# spi_platform

## Overview & Memory Map

`spi_platform` is a SystemC/TLM virtual prototype integration test platform for
the SPI peripheral. It uses a Bremen RV32 RISC-V CPU, a shared TLM bus router,
RAM, CLINT, PLIC, UART0 for firmware logging, and the upstream
`cdc::components::spi_tlm` model.

The SPI interrupt output is routed through PLIC source ID 3. UART0 remains at
`0x1000_0000` so bare-metal firmware can print progress and pass/fail messages.
The SPI `reset_n` pin is actively driven low at simulation time 0 and released
after 100 ns, matching the reset style used by the ADC reference platform.

| Region | Base | Size | End | Notes |
|---|---:|---:|---:|---|
| CLINT | `0x0200_0000` | `0x10000` | `0x0200_FFFF` | RISC-V local software/timer interrupts |
| PLIC | `0x0C00_0000` | `0x400000` | `0x0C3F_FFFF` | External interrupt controller |
| UART0 | `0x1000_0000` | `0x1000` | `0x1000_0FFF` | UART TX logging |
| SPI0 | `0x1002_0000` | `0x1000` | `0x1002_0FFF` | SPI register window, PLIC source ID 3 |
| RAM | `0x8000_0000` | `0x100000` | `0x800F_FFFF` | Firmware text/data/heap/stack |

## Build and Run Instructions

Run these commands from the repository root.

### 1. Source the toolchain environment

```bash
source tools/third_party/setup_env.sh
```

### 2. Build the bare-metal firmware

```bash
make -C fw/spi_test_riscv clean
make -C fw/spi_test_riscv
```

This produces:

```text
fw/spi_test_riscv/spi_test.elf
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
cmake --build build/bremen --target spi_platform
```

### 4. Run the simulation

```bash
./build/bremen/platforms/tests/spi_platform/spi_platform \
  -c platforms/tests/spi_platform/configs/default.yaml \
  --fw fw/spi_test_riscv/spi_test.elf \
  --sim-ms 5
```

Expected firmware output includes phase logs ending with either:

```text
SPI PASS
```

or:

```text
SPI FAIL
```

## Test Scenarios

### Phase 1: Power-On Reset Defaults

Before configuring the SPI controller, firmware reads these registers:

- `CR0`
- `CR1`
- `SR`
- `IMSC`
- `RIS`
- `MIS`

The expected reset defaults come from the current `spi_tlm` model:

| Register | Expected Value | Meaning |
|---|---:|---|
| `CR0` | `0x0000` | Serial format disabled/default |
| `CR1` | `0x0000` | SPI disabled |
| `SR` | `0x0003` | TX FIFO empty and not full (`TFE | TNF`) |
| `IMSC` | `0x0000` | All SPI interrupt masks disabled |
| `RIS` | `0x0008` | TX raw status is level-sensitive while TX FIFO is empty |
| `MIS` | `0x0000` | No masked interrupt is visible to the PLIC |

The firmware prints `PHASE 1 PASS` only if all reset checks match.

### Phase 2: Loopback Data Test

There is no physical SPI slave model in this platform yet. To keep the test
self-contained, the SPI initiator socket `to_peri_socket` is connected to a
local target called `spi_loopback_target`.

The data path is:

```text
firmware write to SPI_DR
  -> SPI TX FIFO
  -> spi_tlm to_peri_socket
  -> spi_loopback_target
  -> unchanged data returned as MISO
  -> SPI RX FIFO
```

The upstream SPI IP has an RX FIFO depth of 8 entries. Its RX raw interrupt bit
is asserted when the RX FIFO is at least half full, so the firmware writes four
8-bit test words:

```text
0xAA, 0xAB, 0xAC, 0xAD
```

When the fourth looped-back word reaches the RX FIFO, SPI asserts its `irq`
output. The platform wires this signal to PLIC source ID 3. The RISC-V trap
handler claims source 3, drains the four RX words, and compares them with the
transmitted pattern.

The firmware prints `PHASE 2 PASS` only if:

- PLIC claim returns source ID 3.
- Four RX words are available.
- The RX data exactly matches `0xAA`, `0xAB`, `0xAC`, and `0xAD`.
- The ISR sees no SPI-side error flags.

### Phase 3: IRQ Clear and Status Verification

After the ISR drains the RX FIFO, it writes `RORIC | RTIC` to the SPI `ICR`
register and completes the PLIC interrupt by writing source ID 3 back to the
PLIC claim/complete register.

The main loop then verifies the post-ISR state:

- `MIS == 0x0000`, proving no masked SPI interrupt remains asserted to the PLIC.
- `RIS == 0x0008`, the model's idle raw status with only TX raw status set.
- The RX/error raw bits in `RIS` are clear.
- `SR.RNE == 0`, proving the RX FIFO was drained by the ISR.

Note that `RIS` is not expected to become literal zero in the current upstream
SPI model. The TX raw interrupt bit is level-sensitive and remains set whenever
the TX FIFO is empty or at/below the TX threshold. Because the firmware does not
enable the TX interrupt mask, this raw TX status does not assert the external
interrupt line. The key deassertion check is therefore `MIS == 0`.

## Interrupt Flow

The firmware configures PLIC source ID 3 as follows:

- Set source 3 priority to `1`.
- Enable source 3 in the PLIC enable register.
- Set the PLIC threshold to `0`.
- Enable machine external interrupts through `mie.MEIE`.
- Enable global machine interrupts through `mstatus.MIE`.

When SPI asserts `irq`, the PLIC raises MEIP to the CPU. The machine trap
handler checks `mcause == 11`, claims the interrupt, handles source ID 3, clears
the SPI-side interrupt state, completes the PLIC interrupt, and returns to
`main`.
