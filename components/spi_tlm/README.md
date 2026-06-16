# ARM PrimeCell SSP (PL022)-based SPI Controller TLM-2.0 Model

This project provides a **Programmer's View (PV)** model of a **SPI controller**, implemented using **SystemC** and **TLM-2.0**. The controller follows the ARM PrimeCell SSP (PL022 r1p4) specification.

## Features
- **TLM-2.0 Blocking Transport:** Supports 20ns decoupled register access via an APB-compatible target socket.
- **Full-Duplex Simulation:** Models concurrent bit-shifting timing based on `CPSR` and `SCR` register settings.
- **Hardware FIFOs:** Includes 8-location deep TX and RX FIFOs with accurate Status Register (`SR`) behavior.
- **Bit-Perfect Masking:** Automatically masks data to the configured word size (4 to 16 bits).
- **Interrupt Logic:** Implements Raw Interrupt Status (`RIS`) and Masked Interrupt Status (`MIS`) with a physical `irq` pin.
- **Reset Mechanism:** Supports active-low hardware reset via the `reset_n` pin.

## Architecture Interfaces

### Sockets
| Socket | Type | Description |
| --- | --- | --- |
| `from_apb_socket` | TLM Target | Used by the CPU/Bus Master to read/write registers. |
| `to_peri_socket` | TLM Initiator | Sends MOSI data to a peripheral and receives MISO data in return. |

### Ports
| Port | Direction | Type | Description |
| --- | --- | --- | --- |
| `irq` | Output | `sc_out<bool>` | High when a masked interrupt condition is met. |
| `reset_n` | Input | `sc_in<bool>` | Active Low. Resets all registers and clears both FIFOs. |

## Build and Unit Testing

This component uses **CMake** for its build and verification.

### 1. Configure the Build

```bash
cmake -S . -B build/bremen -G Ninja \
  -DCDC_BUILD_TESTS=ON \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4
```

### 2. Build the Unit Test

```bash
cmake --build build/bremen --target test_spi_tlm
```

### 3. Run the Unit Test

```bash
./build/bremen/components/spi_tlm/tests/test_spi_tlm
```

## Verification Details
The included `tests/test_spi_tlm.cpp` performs an exhaustive stress test suite:
1. **Bitrate Validation:** Verifies accurate simulation time delays for different clock settings.
2. **Word Size Masking:** Confirms 4-bit mode correctly truncates 16-bit data.
3. **FIFO Overrun:** Validates that the model rejects writes when the TX FIFO is full.
4. **Interrupt Clearing:** Ensures the `ICR` register correctly resets the `irq` pin.
