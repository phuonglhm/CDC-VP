//author: linhtk55-fpt
//verified: hoangv11

# dmic_tlm

DMIC TLM model — a TLM-2.0 component that models a digital microphone (PDM/DMIC) interface for SystemC simulations. This repository contains a lightweight model and a testbench to exercise it. It uses Cascaded Integrator Comb (CIC) to filter PDM signals to PCM.

- Simple TLM-2.0 component exposing a DMIC interface for test and simulation.
- Includes a small testbench and a sample `main` to build and run simulations.
- PDM frequency: 3 MHz
- PCM frequency: 48 kHz

---

# DMIC Specifications

### Memory Map Placement

The peripheral is located in the **Host Master Exp 0** region of the FVP memory map.

* **Bus Interface:** PVBus (TLM-2.0)
* **Base Address:** `0x00_4200_0000` (Uses 40-bit Host CPU addressing)
* **Data Width:** 32-bit accesses only

### Interrupt Routing

The DMIC generates a single hardware interrupt to notify the Host CPU when audio data is ready or if an error occurs.

* **Target Controller:** GIC-400
* **Signal Name:** `dmic_rx_intr`
* **GIC IRQ ID:** 150
* **SPI Index:** 118 *(Formula: GIC ID - 32)*

## Register Map

All registers are 32-bit and accessed via word-aligned offsets from the Base Address (`0x00_4200_0000`).

| Offset | Register Name | R/W | Reset | Description |
| --- | --- | --- | --- | --- |
| `0x00` | **DMIC_CTRL** | R/W | `0x0000` | Main control register (Enable, Interrupts, Decimation). |
| `0x04` | **DMIC_STATUS** | RO | `0x0001` | Read-only status flags (FIFO states, Errors). |
| `0x08` | **DMIC_FIFO_DATA** | RO | `0x0000` | Reading this pops the oldest PCM sample from the FIFO. |
| `0x0C` | **DMIC_FIFO_WM** | R/W | `0x0010` | Watermark level. Triggers IRQ when FIFO reaches this depth. |
| `0x10` | **DMIC_INT_CLR** | WO | `0x0000` | Write `1` to specific bits to clear pending interrupts. |

## Register Bitfields

### `0x00` DMIC_CTRL (Control Register)

| Bits | Name | Description |
| --- | --- | --- |
| 15:8 | `DECIMATION` | **Decimation Factor.** Default is 64. Defines the downsampling ratio of the CIC filter. |
| 7:2 | - | *Reserved* |
| 1 | `INT_EN` | **Interrupt Enable.** `1` = Allows hardware to assert `dmic_rx_intr`. `0` = Mask interrupts. |
| 0 | `EN` | **Peripheral Enable.** `1` = Start PDM processing. `0` = Stop processing and ignore stream. |

### `0x04` DMIC_STATUS (Status Register)

| Bits | Name | Description |
| --- | --- | --- |
| 31:4 | - | *Reserved* |
| 3 | `WM` | **Watermark Reached.** `1` = FIFO contains $\ge$ `DMIC_FIFO_WM` samples. |
| 2 | `OE` | **Overrun Error.** `1` = FIFO overflowed; the newest audio data was lost. |
| 1 | `FF` | **FIFO Full.** `1` = FIFO is completely full (maximum depth). |
| 0 | `FE` | **FIFO Empty.** `1` = FIFO is completely empty. |

### `0x10` DMIC_INT_CLR (Interrupt Clear Register)

| Bits | Name | Description |
| --- | --- | --- |
| 31:2 | - | *Reserved* |
| 1 | `CLR_WM` | Write `1` to manually clear the Watermark flag (usually cleared automatically by reading FIFO). |
| 0 | `CLR_OE` | Write `1` to acknowledge and clear the Overrun Error flag. |

# Macros

```c
// Base Address
#define DMIC_BASE_ADDR      0x0042000000ULL

// Register Offsets
#define DMIC_CTRL_REG       0x00
#define DMIC_STATUS_REG     0x04
#define DMIC_FIFO_DATA_REG  0x08
#define DMIC_FIFO_WM_REG    0x0C
#define DMIC_INT_CLR_REG    0x10

// DMIC_CTRL Bit Masks
#define DMIC_CTRL_EN        (1 << 0)
#define DMIC_CTRL_INT_EN    (1 << 1)
#define DMIC_CTRL_DEC_SHIFT 8
#define DMIC_CTRL_DEC_MASK  (0xFF << DMIC_CTRL_DEC_SHIFT)

// DMIC_STATUS Bit Masks
#define DMIC_STATUS_FE      (1 << 0) // FIFO Empty
#define DMIC_STATUS_FF      (1 << 1) // FIFO Full
#define DMIC_STATUS_OE      (1 << 2) // Overrun Error
#define DMIC_STATUS_WM      (1 << 3) // Watermark Reached

// DMIC_INT_CLR Bit Masks
#define DMIC_INT_CLR_OE     (1 << 0) // Clear Overrun
#define DMIC_INT_CLR_WM     (1 << 1) // Clear Watermark

```
Requirements:
- A C++17 toolchain (`g++` or `clang++`).
- SystemC installed (set `SYSTEMC_HOME` in the `Makefile` or to an installation path).

## Usage

- Edit the top of the `Makefile` if needed and set `SYSTEMC_HOME` to your SystemC installation path.

- Build the project:

```
make
```

- Make and run the built executable:

```
make main 
```

The default target builds `out/main` (see `Makefile`).
