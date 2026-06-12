# ARM PrimeCell SSP (PL022)-based SPI Controller TLM-2.0 Model

This repository contains a **Programmer's View (PV)** model of a **SPI controller**, implemented using **SystemC** and **TLM-2.0**. The controller is modelled using the same specification of ARM PL022 r1p4.

## Features
- **TLM-2.0 Blocking Transport:** Supports 20ns decoupled register access via an APB-compatible target socket.
- **Full-Duplex Simulation:** Models concurrent bit-shifting timing based on `CPSR` and `SCR` register settings.
- **Hardware FIFOs:** Includes 8-location deep TX and RX FIFOs with accurate Status Register (`SR`) behavior.
- **Bit-Perfect Masking:** Automatically masks data to the configured word size (4 to 16 bits) during transmission and reception.
- **Interrupt Logic:** Implements Raw Interrupt Status (`RIS`) and Masked Interrupt Status (`MIS`) with a physical `intr` pin.
- **Reset Mechanism:** Supports both power-on initialization and mid-operation hardware reset via the `reset` pin.

## Architecture interfaces

### Sockets
| Socket | Type | Description |
| --- | --- | --- |
| `from_apb_socket` | TLM Target | Used by the CPU/Bus Master to read/write registers. |
| `to_peri_socket` | TLM Initiator | Sends MOSI data to a peripheral and receives MISO data in return. |

### Ports
| Port | Direction | Type | Description |
| --- | --- | --- | --- |
| `intr` | Output | `sc_out<bool>` | High when a masked interrupt condition (e.g., RX Overrun, TX Empty) is met. |
| `reset` | Input | `sc_in<bool>` | Active Low. Resets all registers and clears both FIFOs. |

## Register Map
The model implements the standard PL022 register offsets:
- `0x000 (CR0)`: Control Register 0 (Data size, Phase, Polarity, Bitrate).
- `0x004 (CR1)`: Control Register 1 (Enable, Master/Slave mode).
- `0x008 (DR)`: Data Register (Read/Write FIFO).
- `0x00C (SR)`: Status Register (Read Only: BSY, RNE, TNF, TFE).
- `0x010 (CPSR)`: Clock Prescale Register.
- `0x014 (IMSC)`: Interrupt Mask Set/Clear.
- ... and more (RIS, MIS, ICR, DMACR).

## How to Compile & Run
Ensure you have SystemC installed (standard path is `/opt/systemc`).

```bash
g++ -I. -I/opt/systemc/include main.cpp spi.cpp -L/opt/systemc/lib -lsystemc -o spi_sim
./spi_sim
```

## Verification
The included `main.cpp` contains an exhaustive **Stress Test Suite**:
1. **Bitrate Validation:** Verifies that slow clock configurations result in accurate simulation time delays.
2. **Word Size Masking:** Confirms that 4-bit mode correctly truncates 16-bit input.
3. **FIFO Overrun:** Validates that the model rejects writes when the TX FIFO is full and returns a TLM error.
4. **Interrupt Clearing:** Ensures the `ICR` register correctly resets the physical interrupt pin.
