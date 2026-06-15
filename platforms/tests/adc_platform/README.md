# adc_platform

Small SoC platform for ADC IP verification with a Bremen `riscv-vp` RV32 CPU.

## Memory Map

| Region | Base | Size | End | Notes |
|---|---:|---:|---:|---|
| CLINT | `0x0200_0000` | `0x0001_0000` | `0x0200_FFFF` | local MSIP/MTIP |
| PLIC | `0x0C00_0000` | `0x0040_0000` | `0x0C3F_FFFF` | external IRQ to MEIP |
| UART0 | `0x1000_0000` | `0x0000_1000` | `0x1000_0FFF` | UART TX console |
| ADC0 | `0x1006_0000` | `0x0000_1000` | `0x1006_0FFF` | ADC register window |
| RAM | `0x8000_0000` | `0x0010_0000` | `0x800F_FFFF` | firmware text/data/heap/stack |

## IRQ Map

| Source | Signal | Destination |
|---:|---|---|
| 1 | `adc.irq_out` | PLIC source 1 -> CPU MEIP |

## Run

```bash
cd /CDC-VP

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
  -DCDC_BUILD_CUSTOM_SOC=ON

cmake --build build/bremen --target adc_platform

./build/bremen/platforms/tests/adc_platform/adc_platform \
  -c platforms/tests/adc_platform/configs/default.yaml \
  --fw fw/adc_irq_riscv/adc_irq.elf \
  --sim-ms 5
```
