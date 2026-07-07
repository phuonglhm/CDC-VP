# QSPI NOR Flash TLM Model

## Overview

`flash_nor_tlm` is a lightweight SystemC/TLM model of a serial NOR flash device
intended to sit behind `qspi_tlm` — or, since the ROM-code boot-flow work,
behind a plain `spi_tlm` master.

This model is not memory-mapped directly on the CPU bus. The CPU accesses it by
programming the QSPI controller registers, and the QSPI controller sends serial
command frames through `flash_nor_tlm::from_qspi_socket`.

A second, independent face `from_spi_socket` speaks `spi_tlm`'s frame protocol
(one full-duplex frame per `b_transport`: TLM write, 2-byte payload, low byte =
MOSI, MISO written back into the low byte; 8-bit frames only). A state machine
decodes NOR READ (`0x03` + 3 address bytes → data out); any other opcode makes
the face return `0xFF` until chip-select deasserts. Chip-select is a C++ call,
`spi_cs(bool selected)` — the platform wires it from the SPI controller's
`cs_n` line (deassert resets the command state machine; while deselected the
flash leaves the frame untouched, i.e. the master sees its own bytes echoed,
matching the legacy loopback dummy-sink behavior). Both sockets are optional:
bind whichever face(s) the instance serves.

The current implementation is a programmer's-view / transaction-level model for
early platform integration, firmware bring-up, and QSPI boot-flow experiments.

## Directory Layout

```text
flash_nor_tlm/
├── CMakeLists.txt
├── include/
│   └── flash_nor_tlm.h
├── src/
│   └── flash_nor_tlm.cpp
├── tests/
│   ├── CMakeLists.txt
│   └── test_flash_nor_tlm.cpp
└── README.md
```

## Interfaces

### TLM Target Socket

```cpp
tlm_utils::simple_target_socket<flash_nor_tlm> from_qspi_socket;
```

This socket receives one full serial command frame per TLM transaction from a
QSPI controller model.

The frame is passed through the payload data buffer. The first byte is the flash
opcode. Address and dummy bytes follow depending on opcode.

### Public API

```cpp
flash_nor_tlm(sc_core::sc_module_name name, std::size_t size_bytes);

void load(const std::uint8_t* data,
          std::size_t len,
          std::uint64_t offset = 0);

std::size_t size() const;
```

Use `load()` to pre-load boot images, test patterns, firmware blobs, or model
data before simulation starts.

## Supported Opcodes

| Opcode | Name | Behavior |
|---:|---|---|
| `0x9F` | `RDID` | Returns JEDEC ID bytes after the opcode byte. |
| `0x05` | `RDSR` | Returns status byte `0x00`, meaning WIP=0 / idle. |
| `0x03` | `READ` | 24-bit address read. Data starts after opcode + 3 address bytes. |
| `0x0B` | `FAST_READ` | 24-bit address read with one dummy byte. |
| `0x6B` | `QUAD_READ` | Modeled like fast read. |
| `0xEB` | `QUAD_IO_READ` | Modeled like fast read. |

Unsupported opcodes complete with `TLM_OK_RESPONSE` and return zero-filled data.

## JEDEC ID

The model currently reports:

```text
Manufacturer ID : 0xEF
Memory type     : 0x40
Capacity code   : 0x18
```

This corresponds to a common 16 MiB-class SPI NOR identification pattern.

## Memory Behavior

- Backing storage is a byte vector initialized to `0xFF`.
- Out-of-range reads return `0xFF`.
- `load()` performs bounds checking and reports a SystemC error on invalid input.
- Current model is read-oriented. It does not implement erase/program commands,
  write enable latch, block protection, status register state, or timing-accurate
  flash program/erase latency.

## Example Integration

```cpp
#include <flash_nor_tlm.h>
#include <qspi_tlm.h>

cdc::components::qspi_tlm qspi("qspi");
cdc::components::flash_nor_tlm flash("flash", 16 * 1024 * 1024);

qspi.to_flash_socket.bind(flash.from_qspi_socket);

std::vector<std::uint8_t> image = load_image_somehow();
flash.load(image.data(), image.size(), 0);
```

In the full SoC memory map, the CPU should see only the QSPI controller MMIO
window. The flash itself has no CPU address window until an explicit XIP bridge
or memory-mapped read mode is implemented.

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
  flash_nor_tlm test_flash_nor_tlm -j$(nproc)

ctest --test-dir CDC-VP/build-platforms-local \
  -R '^flash_nor_tlm$' --output-on-failure
```

## Test Coverage

`tests/test_flash_nor_tlm.cpp` currently verifies:

- JEDEC ID read through opcode `0x9F`.
- Standard 24-bit address read through opcode `0x03`.
- Fast-read style access through opcode `0x0B`.
- Pre-loaded flash contents through `load()`.

## Current Limitations

- No program/erase command support.
- No write-enable latch.
- No status register state beyond idle `0x00`.
- No block protection/security region modeling.
- No XIP / CPU-memory-mapped flash aperture.
- No timing-accurate serial bit-level protocol.

These limitations are intentional for the current virtual platform stage. The
model is sufficient for QSPI read-path verification, boot image fetch modeling,
and early firmware tests.

