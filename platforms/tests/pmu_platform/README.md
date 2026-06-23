# pmu_platform

Small SoC platform for PMU/PWRMGR IP verification with a Bremen `riscv-vp` RV32 CPU.

This platform connects:

```text
CPU -> bus_router -> RAM / UART / CLINT / PLIC / PMU
```

The PMU/PWRMGR IP is memory-mapped at `0x1007_0000`.

The PMU interrupt output `wakeup_irq` is connected to PLIC source 1, then forwarded to the CPU as MEIP.

## Memory Map

| Region |          Base |          Size |           End | Notes                         |
| ------ | ------------: | ------------: | ------------: | ----------------------------- |
| CLINT  | `0x0200_0000` | `0x0001_0000` | `0x0200_FFFF` | local MSIP/MTIP               |
| PLIC   | `0x0C00_0000` | `0x0040_0000` | `0x0C3F_FFFF` | external IRQ to MEIP          |
| UART0  | `0x1000_0000` | `0x0000_1000` | `0x1000_0FFF` | UART TX console               |
| PMU0   | `0x1007_0000` | `0x0000_1000` | `0x1007_0FFF` | PMU/PWRMGR register window    |
| RAM    | `0x8000_0000` | `0x0010_0000` | `0x800F_FFFF` | firmware text/data/heap/stack |

## IRQ Map

| Source | Signal           | Destination               |
| -----: | ---------------- | ------------------------- |
|      1 | `pmu.wakeup_irq` | PLIC source 1 -> CPU MEIP |

## PMU Boot Environment

The platform provides simple boot input signals for the PMU/PWRMGR model.

| Signal          | Behavior                                             |
| --------------- | ---------------------------------------------------- |
| `por_rst_n`     | active-low reset, asserted at 0ns, released at 100ns |
| `otp_done`      | asserted after boot delay                            |
| `lc_done`       | asserted after boot delay                            |
| `rom_done`      | asserted after boot delay                            |
| `rom_good`      | tied to `true`                                       |
| `flash_idle`    | tied to `true`                                       |
| `lc_test_state` | tied to `true`                                       |
| `main_pok`      | tied to `true`                                       |
| `esc_clk_alive` | tied to `true`                                       |
| `core_sleeping` | initial value `false`                                |
| `wakeups`       | initial value `0`                                    |
| `rstreqs`       | initial value `0`                                    |
| `ndmreset_req`  | initial value `false`                                |
| `sw_rst_req`    | initial value `false`                                |
| `esc_rx`        | initial value `false`                                |

## Register Base

Firmware should access PMU registers from:

```c
#define PMU_BASE 0x10070000
```

Example register offsets:

```c
#define PMU_CONTROL_OFFSET    0x14
#define PMU_WAKEUP_EN_OFFSET  0x20
#define PMU_WAKE_STATUS_OFFSET 0x24
#define PMU_RESET_EN_OFFSET   0x2c
#define PMU_RESET_STATUS_OFFSET 0x30
```

Example access:

```c
#define REG32(addr) (*(volatile unsigned int *)(addr))

#define PMU_CONTROL    REG32(PMU_BASE + PMU_CONTROL_OFFSET)
#define PMU_WAKEUP_EN  REG32(PMU_BASE + PMU_WAKEUP_EN_OFFSET)
#define PMU_WAKE_STATUS REG32(PMU_BASE + PMU_WAKE_STATUS_OFFSET)
```

## Run

From the root of the repository:

```bash
cd ~/CDC-VP

export SHLVL=1
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/opt/toolchains/riscv-none-elf/bin:/usr/bin:/bin:$PATH

cmake -S . -B build/bremen -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCDC_CPU_BACKEND=riscv_vp \
  -DCDC_BUILD_MINI_TLM=OFF \
  -DCDC_BUILD_CPU_EVAL=OFF \
  -DCDC_BUILD_CUSTOM_SOC=ON \
  -DSYSTEMC_INCLUDE_DIR=/opt/systemc-2.3.4/include \
  -DSYSTEMC_LIBRARY=/opt/systemc-2.3.4/lib/libsystemc.so

cmake --build build/bremen --target pmu_platform
```

Run without firmware:

```bash
./build/bremen/platforms/tests/pmu_platform/pmu_platform \
  -c platforms/tests/pmu_platform/configs/default.yaml \
  --sim-ms 5
```

Expected platform messages:

```text
pmu_platform config: platforms/tests/pmu_platform/configs/default.yaml
cpu backend: riscv_vp
memory map: RAM=0x80000000 UART=0x10000000 CLINT=0x02000000 PLIC=0x0C000000 PMU=0x10070000
irq map: PMU wakeup_irq -> PLIC source 1 -> MEIP
boot env: por_rst_n release at 100ns, otp_done/lc_done/rom_done asserted after boot delay
```

## Run With Firmware

If PMU firmware is available:

```bash
./build/bremen/platforms/tests/pmu_platform/pmu_platform \
  -c platforms/tests/pmu_platform/configs/default.yaml \
  --fw fw/pmu_riscv/pmu.elf \
  --sim-ms 5
```

## Notes

This platform only instantiates and connects the PMU/PWRMGR IP into a small RISC-V SoC.

It does not automatically print `TEST PASSED` unless the firmware or platform test code explicitly checks PMU behavior and prints that result.

For standalone PMU testing, use the `pmu_tlm` testbench:

```bash
cd ~/CDC-VP/components/pmu_tlm

rm -rf build
cmake -S . -B build
cmake --build build
./build/tb
```

Expected standalone testbench output:

```text
CONTROL before = 0x180
WAKEUP_EN after write = 0x1
TEST PASSED
```
