author: linhtk55-fpt

# timer_platform
Small SoC platform for Timer IP verification with a Bremen `riscv-vp` RV32 CPU.

## Memory Map

| Region | Base | Size | End | Notes |
|---|---:|---:|---:|---|
| CLINT | `0x0200_0000` | `0x0001_0000` | `0x0200_FFFF` | local MSIP/MTIP |
| PLIC | `0x0C00_0000` | `0x0040_0000` | `0x0C3F_FFFF` | external IRQ to MEIP |
| UART0 | `0x1000_0000` | `0x0000_1000` | `0x1000_0FFF` | UART TX console |
| TIMER0 | `0x1003_0000` | `0x0000_1000` | `0x1003_0FFF` | Timer MMIO register window |
| RAM | `0x8000_0000` | `0x0010_0000` | `0x800F_FFFF` | firmware text/data/heap/stack |

## IRQ Map

| Source | Signal | Destination |
|---:|---|---|
| 1 | `timer.irq_out` | PLIC source 1 -> CPU MEIP |

## Run

From the repository root, configure the build (adjust SystemC paths as needed):

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

Build the platform executable:

```bash
cmake --build build/bremen --target timer_platform
```

Run the simulation using the timer firmware ELF:

```bash
./build/bremen/platforms/tests/timer_platform/timer_platform \
  -c platforms/tests/timer_platform/configs/default.yaml \
  --fw fw/timer2_irq_riscv/timer_irq.elf \
  --sim-ms 5
```

Expected runtime output (successful test):

```text
Timer initiated.
timer_platform config: platforms/tests/timer_platform/configs/default.yaml
cpu backend: riscv_vp (Bremen rv32)
memory map: RAM=0x80000000 UART=0x10000000 CLINT=0x02000000 PLIC=0x0C000000 timer=0x10030000
Starting generic timer testing...
Arming timer with 50,000 ticks...
Waiting for interrupt (WFI)...
[TRAP] mcause=0x8000000B claim=0x00000001
[TRAP] Timer interrupt received!
SUCCESS: Timer test passed!
```

To quickly check for success, filter the run output for `SUCCESS`:

```bash
./build/bremen/platforms/tests/timer_platform/timer_platform \
  -c platforms/tests/timer_platform/configs/default.yaml \
  --fw fw/timer2_irq_riscv/timer_irq.elf \
  --sim-ms 5 | grep "SUCCESS"
```

Adjust paths and toolchain variables above to match your environment.
