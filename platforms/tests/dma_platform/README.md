# DMA Platform

Small Bremen/RISC-V SoC for verifying the `dma_tlm` TLM peripheral, following
the same structure as `adc_platform`.

The verified path is:

```text
RISC-V firmware
  -> writes a DMA channel program into RAM
  -> writes DBGINST0/DBGINST1/DBGCMD to launch the channel
  -> CPU TLM initiator        -> bus_router -> DMA registers (0x10070000)
  -> DMA master_socket (bus master, shares the same bus_router as the CPU)
                               -> bus_router -> RAM (fetch instructions, read/write data)
  -> DMA raises completion event -> INTEN/INT_EVENT_RIS/INTMIS -> irq
  -> firmware polls CSR0/FTR and verifies completion, data, and fault behavior
```

## Why the DMA Shares the CPU's Bus

Unlike simpler peripherals (UART, ADC, PWM), the DMA is itself a **bus
master** — it doesn't just expose registers for the CPU to poke, it actively
reads/writes memory on its own to perform transfers. To model this, `dma_tlm`
exposes two sockets:

- `target_socket` — register interface, the CPU writes control registers
  here (bound to the bus as a normal peripheral at `0x10070000`).
- `master_socket` — the DMA's own initiator port, used to fetch its program
  and move data. This is bound to an **extra upstream port** on the same
  `bus_router` the CPU uses (`num_initiators=2`), so DMA SAR/DAR/CPC values
  use the **same `0x80000000`-based addresses** as CPU code/data, and any
  region reachable by the CPU (RAM, etc.) is also reachable by the DMA.

## One-Time Setup

From the repository root:

```bash
./tools/third_party/setup_third_party.sh
source ./tools/third_party/setup_env.sh
```

## Build And Run

Build the bare-metal RISC-V firmware:

```bash
make -C fw/dma_riscv
```

This produces:

```text
fw/dma_riscv/dma_test.elf
fw/dma_riscv/dma_test.dis
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
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so
```

Build the DMA platform executable:

```bash
cmake --build build/bremen --target dma_platform
```

Run the simulation:

```bash
./build/bremen/platforms/tests/dma_platform/dma_platform \
  -c platforms/tests/dma_platform/configs/default.yaml \
  --fw fw/dma_riscv/dma_test.elf \
  --sim-ms 10
```

## What The Platform Builds

The `dma_platform` executable is a SystemC virtual platform containing:

```text
Bremen RISC-V CPU
RAM
UART
CLINT
PLIC
DMA TLM model (dma_tlm)
bus_router (2 upstream ports: CPU + DMA master, 5 downstream targets)
```

The platform memory map is:

| Region | Base | Purpose |
|---|---:|---|
| RAM | `0x8000_0000` | Firmware code/data/stack, DMA program/source/dest buffers |
| UART | `0x1000_0000` | Firmware console output |
| CLINT | `0x0200_0000` | Local timer/software interrupts |
| PLIC | `0x0C00_0000` | External interrupt controller (unused by this test) |
| DMA | `0x1007_0000` | DMA register window |

> **Note:** The `configs/default.yaml` file is currently informational only
> — it is printed to the console at startup but its contents are not parsed
> into the memory map. The memory map above is hardcoded as C++ constants in
> `src/dma_platform_top.cpp`. This matches the current state of
> `adc_platform` and other platforms in this repo; none of them read their
> YAML config yet.

## What The Firmware Does

`fw/dma_riscv/src/main.c` runs on the simulated RISC-V CPU. See
`fw/dma_riscv/README.md` for the full breakdown of what each test section
verifies (buffer setup, DMA program build, debug launch, completion
wait, result verification, DMAKILL, and undefined-opcode fault handling).

Expected successful output ends with:

```text
DMA PASS
```

## Notes

- DMA buffers (program, source, destination) are placed at `0x8008_0000`
  and above — 512 KiB into the 1 MiB RAM region — to avoid overlapping
  firmware code, data, and stack, which live at the start of RAM
  (`0x8000_0000`).
- `DMAGO` (a manager instruction) takes its channel number from byte 1 of
  the instruction itself (`DBGINST0` bits `[31:24]`), not from `DBGINST0`
  bits `[10:8]`. Those bits only select the channel for channel-thread
  debug instructions like `DMAKILL`, `DMANOP`, and `DMASEV` issued directly
  to a channel.