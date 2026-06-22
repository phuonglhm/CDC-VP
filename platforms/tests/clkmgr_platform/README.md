# clkmgr_platform

Small SoC platform for OpenTitan Clock Manager (`clkmgr`) TLM verification with
a Bremen `riscv-vp` RV32 CPU.

## Memory Map

The clkmgr block uses the ADC placeholder address until the final clkmgr memory
map is assigned.

| Region | Base | Size | End | Notes |
|---|---:|---:|---:|---|
| CLINT | `0x0200_0000` | `0x0001_0000` | `0x0200_FFFF` | local MSIP/MTIP |
| PLIC | `0x0C00_0000` | `0x0040_0000` | `0x0C3F_FFFF` | retained from ADC reference |
| UART0 | `0x1000_0000` | `0x0000_1000` | `0x1000_0FFF` | UART TX console |
| CLKMGR0 | `0x1006_0000` | `0x0000_1000` | `0x1006_0FFF` | clkmgr register window, ADC placeholder |
| RAM | `0x8000_0000` | `0x0010_0000` | `0x800F_FFFF` | firmware text/data/heap/stack |

## OpenTitan Register Basis

The firmware uses the OpenTitan clkmgr register map:

| Register | Offset | Firmware Use |
|---|---:|---|
| `EXTCLK_CTRL_REGWEN` | `0x04` | reset and lock check |
| `EXTCLK_CTRL` | `0x08` | MuBi4 external/internal clock switching |
| `EXTCLK_STATUS` | `0x0C` | external clock acknowledge check |
| `CLK_ENABLES` | `0x18` | software gateable peripheral clocks |
| `CLK_HINTS` | `0x1C` | transactional clock hint writes |
| `CLK_HINTS_STATUS` | `0x20` | transactional clock status readback |
| `RECOV_ERR_CODE` | `0x50` | reset value check |
| `FATAL_ERR_CODE` | `0x54` | reset value check |

The current standalone VP sets the clkmgr life-cycle state to `DEV` because no
life-cycle controller is present and the external-clock software path is only
effective when debug functions are enabled.

## IRQ Map

| Source | Signal | Destination |
|---:|---|---|
| 1 | `clkmgr0.irq_out` | PLIC source 1 -> CPU MEIP |

`clkmgr_tlm` currently has no interrupt output pin, so the VP ties this retained
ADC-reference source low.

## Build And Run

```bash
# 1. Setup
./tools/third_party/setup_third_party.sh
source ./tools/third_party/setup_env.sh

# 2. Run Demo
cd /CDC-VP
make -C fw/clkmgr_test_riscv

cmake -S . -B build/bremen -G Ninja \
  -DCMAKE_C_COMPILER=/usr/bin/gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCDC_CPU_BACKEND=riscv_vp \
  -DCDC_BUILD_MINI_TLM=OFF \
  -DCDC_BUILD_CPU_EVAL=OFF \
  -DCDC_BUILD_CUSTOM_SOC=ON

cmake --build build/bremen --target clkmgr_platform

./build/bremen/platforms/tests/clkmgr_platform/clkmgr_platform \
  -c platforms/tests/clkmgr_platform/configs/default.yaml \
  --fw fw/clkmgr_test_riscv/clkmgr_test.elf \
  --sim-ms 5
```

Expected successful firmware output ends with:

```text
CLKMGR TEST PASS
```
