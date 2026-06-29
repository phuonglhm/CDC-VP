# QSPI Controller TLM Model

## Overview

`qspi_tlm` is a simplified SystemC/TLM model of a memory-mapped QSPI controller.
It exposes a CPU-facing APB-like register target socket and drives a serial flash
device through an initiator socket.

The intended integration is:

```text
CPU / bus_router -> qspi_tlm MMIO registers -> flash_nor_tlm
```

This model is useful for early platform integration, firmware driver smoke
tests, interrupt handling checks, and simple QSPI boot-flow experiments.

## Directory Layout

```text
qspi_tlm/
├── CMakeLists.txt
├── include/
│   └── qspi_tlm.h
├── src/
│   └── qspi_tlm.cpp
├── tests/
│   ├── CMakeLists.txt
│   └── test_qspi_tlm.cpp
└── README.md
```

## Interfaces

### CPU-Facing Target Socket

```cpp
tlm_utils::simple_target_socket<qspi_tlm> from_apb_socket;
```

Bind this socket to the SoC bus at the QSPI MMIO base address.

### Flash-Facing Initiator Socket

```cpp
tlm_utils::simple_initiator_socket<qspi_tlm> to_flash_socket;
```

Bind this socket to `flash_nor_tlm::from_qspi_socket` or another serial flash
model.

### Ports

```cpp
sc_core::sc_out<bool> irq;
sc_core::sc_in<bool> reset_n;
```

- `irq` asserts when a transfer completes and `QSPI_INT_DONE` is enabled.
- `reset_n` is active-low. A falling edge clears controller state and FIFO data.

## Register Map

All CPU transactions must be 32-bit accesses. Unsupported offsets return
`TLM_ADDRESS_ERROR_RESPONSE`.

| Offset | Register | Access | Description |
|---:|---|---|---|
| `0x00` | `QSPI_CTRL` | R/W | bit0 `QSPI_CTRL_EN`. Controller enable. |
| `0x04` | `QSPI_STATUS` | R | bit0 `BUSY`, bit1 `RXNE`. |
| `0x08` | `QSPI_CMD` | R/W | Flash opcode, low 8 bits used. |
| `0x0C` | `QSPI_ADDR` | R/W | 24-bit flash address. |
| `0x10` | `QSPI_LEN` | R/W | Number of response bytes requested. |
| `0x14` | `QSPI_CFG` | R/W | Low 3 bits select lane count; bits `[15:8]` encode dummy cycles. |
| `0x18` | `QSPI_DATA` | R | Pops one byte from RX FIFO, returned in low 8 bits. |
| `0x1C` | `QSPI_INT_EN` | R/W | bit0 enables transfer-done interrupt. |
| `0x20` | `QSPI_INT_STATUS` | R/W1C | bit0 transfer-done status. |
| `0x24` | `QSPI_START` | W | Write bit0=1 to execute a transfer when enabled. |

## Supported Flash Commands

`qspi_tlm` constructs command frames for these opcode classes:

| Opcode | Name | Address phase | Dummy handling |
|---:|---|---|---|
| `0x03` | `READ` | 24-bit address | No forced dummy byte. |
| `0x0B` | `FAST_READ` | 24-bit address | At least one dummy byte. |
| `0x6B` | `QUAD_READ` | 24-bit address | At least one dummy byte. |
| `0xEB` | `QUAD_IO_READ` | 24-bit address | At least one dummy byte. |
| `0x05` | `RDSR` | none | No address phase. |
| `0x9F` | `RDID` | none | No address phase. |

For addressed commands, the controller sends:

```text
opcode + addr[23:16] + addr[15:8] + addr[7:0] + dummy bytes + response bytes
```

The response bytes are pushed into the RX FIFO after the command/address/dummy
prefix.

## Transfer Flow

Firmware should use this sequence:

```text
1. Write QSPI_CTRL.EN = 1.
2. Optionally enable QSPI_INT_EN.DONE.
3. Write QSPI_CMD.
4. Write QSPI_ADDR for addressed commands.
5. Write QSPI_LEN with requested response byte count.
6. Write QSPI_CFG for lane/dummy configuration.
7. Write QSPI_START = 1.
8. Poll QSPI_STATUS.RXNE or wait for irq.
9. Read QSPI_DATA until RX FIFO is empty.
10. Clear QSPI_INT_STATUS.DONE with W1C.
```

## Example Integration

```cpp
#include <flash_nor_tlm.h>
#include <qspi_tlm.h>

cdc::components::qspi_tlm qspi("qspi");
cdc::components::flash_nor_tlm flash("flash", 16 * 1024 * 1024);
sc_core::sc_signal<bool> reset_n;
sc_core::sc_signal<bool> qspi_irq;

qspi.to_flash_socket.bind(flash.from_qspi_socket);
qspi.reset_n(reset_n);
qspi.irq(qspi_irq);

reset_n.write(true);
```

In a full SoC, connect:

- `from_apb_socket` to `bus_router` at `QSPI0_BASE`.
- `to_flash_socket` to `flash_nor_tlm`.
- `irq` to the assigned PLIC source.
- `reset_n` to the SoC reset signal.

## SoC Memory Map

The current SoC-level spec assigns:

```text
QSPI0 MMIO base : 0x100C_0000
QSPI0 size      : 0x0000_1000
PLIC source     : 14
```

The NOR flash behind QSPI has no CPU MMIO window in the current model. A future
XIP aperture is reserved in `docs/peripheral_memory_map.md`, but it should not
be bound until memory-mapped read mode is implemented.

## Build and Test

From the repository parent directory:

```bash
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH

cmake -S CDC-VP -B CDC-VP/build-platforms-local \
  -DCDC_CPU_BACKEND=riscv_vp \
  -DCDC_BUILD_CPU_EVAL=OFF \
  -DCDC_BUILD_CUSTOM_SOC=ON \
  -DCDC_BUILD_MINI_TLM=OFF \
  -DCDC_BUILD_TESTS=ON

cmake --build CDC-VP/build-platforms-local --target \
  qspi_tlm flash_nor_tlm test_qspi_tlm -j$(nproc)

ctest --test-dir CDC-VP/build-platforms-local \
  -R '^qspi_tlm$' --output-on-failure
```

## Test Coverage

`tests/test_qspi_tlm.cpp` currently verifies:

- Binding QSPI to `flash_nor_tlm`.
- Standard read opcode `0x03` from pre-loaded flash data.
- RX FIFO behavior through repeated `QSPI_DATA` reads.
- Transfer-done interrupt assertion and W1C clear.
- JEDEC ID read opcode `0x9F`.
- Address-error response for unsupported register offsets.

## Current Limitations

- No write/program/erase command path.
- No memory-mapped XIP read aperture.
- RX FIFO is a simple byte queue.
- `QSPI_DATA` returns one byte per 32-bit read.
- No chip-select timing, DDR timing, or bit-level SPI waveform modeling.
- Lane count only affects approximate transfer delay.
- No command queue or DMA integration.

These limitations are acceptable for current CDC-VP goals: RTOS driver bring-up,
interrupt verification, flash read-path checks, and eventual bootrom/QSPI boot
experiments.

