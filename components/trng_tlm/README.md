// Author: hoangv11
//verified: linhtk55-fpt

# True Random Number Generator (TRNG) TLM-2.0 Model

This repository contains a **Programmer's View (PV)** model of a **True Random Number Generator (TRNG)**, implemented using **SystemC** and **TLM-2.0**. The model is based on a standard TRNG register specification.

## Features
- **TLM-2.0 Blocking Transport:** Supports register access via a 32-bit aligned target socket with configurable access latency.
- **Backdoor Debug Interface:** Provides side-effect-free backdoor access using `transport_dbg` for testbench inspection.
- **Comprehensive Register Interface:** Implements the full register map of the TRNG, including configuration, control, data, and BIST counters.
- **Interrupt Handling:** Models the unmasked/masked interrupt status and clear operations via `RNG_IMR`, `RNG_ISR`, and `RNG_ICR` registers.
- **Reset Logic:** Supports asynchronous hardware reset via `reset_n` and software-triggered resets via `TRNG_SW_RESET` and `RST_BITS_COUNTER` registers.

## Architecture Interfaces

### Sockets
| Socket | Type | Description |
| --- | --- | --- |
| `socket` | TLM Target | Used by the CPU/Bus Master to read and write TRNG registers. |

### Ports
| Port | Direction | Type | Description |
| --- | --- | --- | --- |
| `clk` | Input | `sc_in<bool>` | TRNG engine clock signal. |
| `reset_n` | Input | `sc_in<bool>` | Active Low. Asynchronous hardware reset signal. |
| `irq_out` | Output | `sc_out<bool>` | High when a masked interrupt condition (e.g. EHR_VALID) is met. |

## Register Map
The model implements the standard TRNG register offsets:
- `0x100 (RNG_IMR)`: Interrupt Mask Register (R/Ws)
- `0x104 (RNG_ISR)`: Interrupt Status Register (RO)
- `0x108 (RNG_ICR)`: Interrupt Clear Register (WO)
- `0x10C (TRNG_CONFIG)`: Configuration Register (RW)
- `0x110 (TRNG_VALID)`: Valid Register (RO)
- `0x114 - 0x128 (EHR_DATA0-5)`: Entropy Holding Register Data (RO)
- `0x12C (RND_SOURCE_ENABLE)`: Random Source Enable Register (RW)
- `0x130 (SAMPLE_CNT1)`: Sample Count Register (RW)
- `0x134 (AUTOCORR_STATISTIC)`: Autocorrelation Register (R/Ws)
- `0x138 (TRNG_DEBUG_CONTROL)`: Debug Control Register (RO)
- `0x140 (TRNG_SW_RESET)`: Reset Register (WO)
- `0x1B8 (TRNG_BUSY)`: Busy Register (RO)
- `0x1BC (RST_BITS_COUNTER)`: Reset Bits Counter Register (WO)
- `0x1E0 - 0x1E8 (RNG_BIST_CNTR0-2)`: BIST Counter Registers (RO)

## Build and Unit Testing
### 1. Configure the Build

```bash
cmake -S . -B build/bremen -G Ninja \
  -DCDC_BUILD_TESTS=ON \
  -DSYSTEMC_HOME=/opt/systemc-2.3.4
```

### 2. Build the Unit Test

```bash
cmake --build build/bremen --target test_trng_tlm
```

### 3. Run the Unit Test

```bash
./build/bremen/components/trng_tlm/tests/test_trng_tlm
```

## Note
- TRNG's scan mode is not supported for this tlm, so there is no scan signal input.
- This TLM uses rand() for simple modelling since simulating real inverters is slow, and thus
  defeats the point of using TLM in the first place.
