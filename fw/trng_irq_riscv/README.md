author: linhtk55-fpt
# CDC-VP TRNG Interrupt Demo
This demo verifies the TRNG inside a small RISC-V virtual
platform. The test uses the Bremen `riscv-vp` CPU backend and Accellera
SystemC 2.3.4.

The verified path is:

```text
RISC-V firmware
  -> CPU TLM initiator
  -> bus_router
  -> TRNG MMIO registers (0x1470_0000)
  -> TRNG interrupt output
  -> PLIC source 1
  -> CPU machine external interrupt
  -> firmware trap handler
```

## One-Time Setup

From the repository root:

```bash
./tools/third_party/setup_third_party.sh
source ./tools/third_party/setup_env.sh
```

`setup_third_party.sh` clones the external CPU model repositories into
`third_party/`, including Bremen `riscv-vp`.

`setup_env.sh` prepares the shell environment for this repo. It selects the
host compilers and adds the RISC-V bare-metal toolchain to `PATH`.

## Build And Run

Build the bare-metal RISC-V firmware:

```bash
make -C fw/trng_irq_riscv
```
This produces:

```text
fw/trng_irq_riscv/timer_irq.elf
fw/trng_irq_riscv/timer_irq.dis
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
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib-linux64/libsystemc.so
```

Build the timer platform executable:

```bash
cmake --build build/bremen --target timer_platform
```

Run the simulation:

```bash
./build/bremen/platforms/tests/timer_platform/timer_platform \
  -c platforms/tests/timer_platform/configs/default.yaml \
  --fw fw/trng_irq_riscv/timer_irq.elf \
  --sim-ms 5
```

## What The Demo Builds


The `timer_platform` executable is a SystemC virtual platform containing:

```text
Bremen RISC-V CPU
RAM
UART
CLINT
PLIC
Timer TLM model (`timer_tlm`)
bus_router
```

The platform memory map is:

| Region | Base | Purpose |
|---|---:|---|
| RAM | `0x8000_0000` | Firmware code/data/stack |
| UART | `0x1000_0000` | Firmware console output |
| CLINT | `0x0200_0000` | Local timer/software interrupts |
| PLIC | `0x0C00_0000` | External interrupt controller |
| Timer | `0x1003_0000` | Timer MMIO register window |

The timer interrupt line is connected to PLIC source 1, and the PLIC raises the
CPU machine external interrupt.

## What The Firmware Does

`fw/trng_irq_riscv/src/main.c` runs on the simulated RISC-V CPU. It:

1. Sets the machine trap vector.
2. Configures PLIC source 1 for the timer interrupt and enables it.
3. Enables machine external interrupts.
4. Unmasks the TRNG's internal interrupt logic by writing 0 to the Interrupt Mask Register (TRNG_IMR).
5. Triggers the entropy generation engine by writing 1 to the Source Enable register (TRNG_SRC_EN).
6. Waits for the interrupt using `wfi`.
7. In the trap handler, claims the PLIC source 1, clears the TRNG interrupt status thought the Interrupt Clear Register (TRNG_ICR), and reports success.
8. After reporting success, the firmware parks the CPU.

Expected successful output includes:

```text
trng_platform config: platforms/tests/trng_platform/configs/default.yaml
cpu backend: riscv_vp (Bremen rv32)
memory map: RAM=0x80000000 UART=0x10000000 CLINT=0x02000000 PLIC=0x0C000000 trng=0x14700000
irq map: trng -> PLIC source 1 -> MEIP
Starting TRNG generation testing... 
Triggering TRNG random number generation...
W[TRAP] mcause=0x8000000B claim=0x00000001
[TRAP] TRNG interrupt received!
aiting for TRNG interrupt (WFI)...
```

## Notes

Use Accellera SystemC 2.3.4 for this demo. Adjust `SYSTEMC_INCLUDE_DIR` and
`SYSTEMC_LIBRARY` in the CMake command above to match your SystemC installation.

The `trng_tlm` component is a TRNG model used by the demo and
exposes a 4-register MMIO window at `0x1470_0000`