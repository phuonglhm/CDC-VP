# Bremen SoC Peripheral Memory Map and IRQ Spec

Target integration spec for a complete Bremen `riscv-vp` RV32 platform built
from these TLM IPs:

`UART, I2C, SPI, Timer, Watchdog, PWM, DMA, TRNG, CMU, PMU, DMIC, OTP, QSPI,
flash_nor, RTC, ISP, VPU, NPU`.

This document is the SoC-level assignment. Some existing single-IP test
platforms reuse temporary addresses such as `0x1006_0000`; those are test-local
maps and must not be copied into the final integrated SoC.

## Peripheral Instance Counts

Baseline instance counts for the integrated SoC (an edge media/AI profile):

| IP | Count | Rationale |
|---|---:|---|
| UART | 2 | One console/debug, one for external comms. |
| I2C | 2 | Sensor/PMIC bus plus a separate camera-config (CCI) bus. |
| SPI | 2 | Flash/display plus a sensor link. |
| Timer | 2 | General-purpose timers, separate from the CLINT system tick. |
| DMA | 1 | Already multi-channel; scale to 2 only if media traffic demands it. |
| PWM | 1 | Single block, 6 channels. |
| WDT, TRNG, ADC, DMIC, OTP, CMU, PMU, QSPI, RTC | 1 | Single instance / architectural singletons. |
| ISP, VPU, NPU | 1 | Single-stream pipeline `RAW -> ISP -> VPU -> NPU`. |

UART model selected for the integrated SoC is `uart2_tlm`.

Second instances are appended at the end of the peripheral region; existing
instance-0 bases and their already-locked PLIC source IDs are kept fixed so
current tests and firmware defines do not change.

## CPU and Boot Assumptions

| Item | Spec |
|---|---|
| CPU backend | `cdc::cpu::riscv_vp_cpu`, Bremen RV32 ISS |
| XLEN | 32-bit |
| Bus address width | 32-bit physical addresses |
| External interrupt input | MEIP, driven by PLIC |
| Local interrupts | MSIP/MTIP, driven by CLINT |
| ELF boot behavior | Bremen wrapper starts at the ELF entry point |
| No-firmware fallback | Current wrapper initializes PC to `0x8000_0000` |
| Recommended firmware link base | `0x8000_0000` RAM |
| ROM-code boot flow (VP_FX1) | BOOTROM0 at `0x0000_0000` is a real 64 KiB ROM; the ROM-code ELF is built with entry `0x0` and loaded via `--fw`. It reads the boot strap GPIO0 pin 1: LOW → jump to the application in IFLASH0 (`0x0400_0000`, loaded via `--int-flash`); HIGH → probe UART0 (host tool via `--uart0-socket`/`--uart0-rx-file`) then NOR READ over SPI0 (`--spi-flash`). See `docs/romcode_boot_hw_plan.md`. |

## Address Map

Small control peripherals use a 4 KiB active register window. ISP/VPU/NPU use a
64 KiB MMIO window because they are algorithmic accelerators: the first 4 KiB is
the common control/status block, while the rest is reserved for descriptors,
algorithm parameters, debug counters, and future register expansion.

All peripheral bases remain on 64 KiB boundaries, so a 4 KiB IP can grow to 64
KiB later without changing its base address.

| Region | Base | Size | End | CPU access | Notes |
|---|---:|---:|---:|---|---|
| BOOTROM0 | `0x0000_0000` | `0x0001_0000` | `0x0000_FFFF` | ROM (read-only, executable) | First-stage boot ROM, bound in VP_FX1_Full_SoC (`memory_tlm`, read-only; debug/backdoor loads bypass the write protect). ROM-code ELF entry must be `0x0`. |
| IFLASH0 | `0x0400_0000` | `0x0040_0000` | `0x043F_FFFF` | ROM (read-only, executable) | Internal code flash, 4 MiB XIP window (VP_FX1_Full_SoC). Preloaded from a raw binary via `--int-flash`. Do not reuse `0x2000_0000` (reserved for future QSPI XIP). |
| CLINT0 | `0x0200_0000` | `0x0001_0000` | `0x0200_FFFF` | MMIO | RISC-V local MSIP/MTIP source. |
| PLIC0 | `0x0C00_0000` | `0x0040_0000` | `0x0C3F_FFFF` | MMIO | External interrupt controller, hart0 M-mode context. |
| UART0 | `0x1000_0000` | `0x0000_1000` | `0x1000_0FFF` | MMIO | Console UART. Selected model is `uart2_tlm`. Exposes `sc_out<bool> irq` (combined UARTINTR) for the PLIC. |
| I2C0 | `0x1001_0000` | `0x0000_1000` | `0x1001_0FFF` | MMIO | I2C controller. |
| SPI0 | `0x1002_0000` | `0x0000_1000` | `0x1002_0FFF` | MMIO | SPI controller (PL022-style). Vendor register `SSPCSR` @ `+0x28` (bit0: 1 = assert the active-low chip-select). A dedicated NOR flash (`spi_flash0`, 16 MiB, independent from QSPI0's FLASH0) sits behind it for the ROM-code SPI download; image via `--spi-flash`. |
| TIMER0 | `0x1003_0000` | `0x0000_1000` | `0x1003_0FFF` | MMIO | Peripheral timer, separate from CLINT `mtime/mtimecmp`. |
| WDT0 | `0x1004_0000` | `0x0000_1000` | `0x1004_0FFF` | MMIO | Watchdog timer. |
| PWM0 | `0x1005_0000` | `0x0000_1000` | `0x1005_0FFF` | MMIO | PWM controller. Current model has no IRQ output. |
| DMA0 | `0x1006_0000` | `0x0000_1000` | `0x1006_0FFF` | MMIO + master | DMA control register target plus DMA master socket into the system bus. |
| TRNG0 | `0x1007_0000` | `0x0000_1000` | `0x1007_0FFF` | MMIO | True/random number generator model. This replaces the old test-local `0x1470_0000`. |
| CMU0 | `0x1008_0000` | `0x0000_1000` | `0x1008_0FFF` | MMIO | Clock management unit, implemented by `clkmgr_tlm`. |
| PMU0 | `0x1009_0000` | `0x0000_1000` | `0x1009_0FFF` | MMIO | Power/reset manager, implemented by `pmu_tlm` / `Pwrmgr`. |
| DMIC0 | `0x100A_0000` | `0x0000_1000` | `0x100A_0FFF` | MMIO | Digital microphone bus/control window. PDM stimulus enters through the separate PDM socket. |
| OTP0 | `0x100B_0000` | `0x0000_1000` | `0x100B_0FFF` | MMIO | OTP controller/model. |
| QSPI0 | `0x100C_0000` | `0x0000_1000` | `0x100C_0FFF` | MMIO | QSPI controller register window. |
| ISP0 | `0x100D_0000` | `0x0001_0000` | `0x100D_FFFF` | MMIO + master | Image signal processor. Uses source/destination frame-buffer descriptors in RAM0. |
| VPU0 | `0x100E_0000` | `0x0001_0000` | `0x100E_FFFF` | MMIO + master | Video processing unit. Uses frame-buffer descriptors in RAM0. |
| NPU0 | `0x100F_0000` | `0x0001_0000` | `0x100F_FFFF` | MMIO + master | Neural processing unit. Uses tensor/weight/activation descriptors in RAM0. |
| UART1 | `0x1010_0000` | `0x0000_1000` | `0x1010_0FFF` | MMIO | Second UART instance. Same register model as UART0. |
| I2C1 | `0x1011_0000` | `0x0000_1000` | `0x1011_0FFF` | MMIO | Second I2C controller. |
| SPI1 | `0x1012_0000` | `0x0000_1000` | `0x1012_0FFF` | MMIO | Second SPI controller. |
| TIMER1 | `0x1013_0000` | `0x0000_1000` | `0x1013_0FFF` | MMIO | Second peripheral timer, separate from CLINT and TIMER0. |
| RTC0 | `0x1014_0000` | `0x0000_1000` | `0x1014_0FFF` | MMIO | Real-time clock with alarm. Implemented by `rtc_tlm`. May also drive a PMU wakeup line in future. |
| ADC0 | `0x1015_0000` | `0x0000_1000` | `0x1015_0FFF` | MMIO | ADC controller (`adc_tlm`). Assigned in the VP_FX1 full SoC (resolves the earlier ADC base TBD). |
| GPIO0 | `0x1016_0000` | `0x0000_1000` | `0x1016_0FFF` | MMIO | GPIO block (`gpio_tlm`), 32 pins. Registers: `0x00 VALUE` (RO), `0x04 OUT` (RW), `0x08 DIR` (RW, 1=output, reset all-inputs); 32-bit accesses only. Pin 1 is the ROM-code boot-mode strap (LOW = boot IFLASH app, HIGH = download probe). No IRQ in this revision (PLIC 24 stays reserved). |
| RAM0 | `0x8000_0000` | `0x1000_0000` | `0x8FFF_FFFF` | RAM/DDR | Firmware, heap/stack, frame buffers, tensors, weights, and accelerator scratch space. Final integrated SoC target is 256 MiB. |

## Accelerator Pipeline Buffer Plan

The target data path is:

```text
Input RAW frame -> ISP0 -> VPU0 -> NPU0
```

Use RAM0 as the shared DDR-like memory for frame and tensor exchange. The CPU
programs physical buffer addresses into each accelerator's descriptor registers.
The accelerators should use master sockets when the algorithm model starts doing
real memory traffic.

| Buffer region | Base | Size | End | Producer | Consumer | Notes |
|---|---:|---:|---:|---|---|---|
| FW/RTOS | `0x8000_0000` | `0x0100_0000` | `0x80FF_FFFF` | CPU/loader | CPU | Firmware text/data/heap/stack. |
| RAW_IN0 | `0x8100_0000` | `0x0100_0000` | `0x81FF_FFFF` | sensor/testbench/CPU | ISP0 | Raw Bayer or packed sensor frame input. 16 MiB is enough for typical 4K RAW10/RAW12 test frames. |
| ISP_OUT0 | `0x8200_0000` | `0x0200_0000` | `0x83FF_FFFF` | ISP0 | VPU0 | RGB/YUV frame output, allows 4K RGB888/YUV plus padding. |
| VPU_OUT0 | `0x8400_0000` | `0x0200_0000` | `0x85FF_FFFF` | VPU0 | NPU0 | Scaled/cropped/normalized frame or video pre-processing output. |
| NPU_WEIGHTS0 | `0x8600_0000` | `0x0200_0000` | `0x87FF_FFFF` | loader/CPU | NPU0 | Model weights, constants, and layer descriptors. |
| NPU_WORK0 | `0x8800_0000` | `0x0400_0000` | `0x8BFF_FFFF` | NPU0/CPU | NPU0/CPU | Activations, intermediate tensors, and NPU output tensors. |
| PIPELINE_RSVD | `0x8C00_0000` | `0x0400_0000` | `0x8FFF_FFFF` | reserved | reserved | Future double buffering, multi-stream, larger models, or video surfaces. |

Recommended minimum behavior for the first ISP/VPU/NPU models:

1. CPU writes input/output buffer addresses, dimensions, format, stride, and
   operation mode to the accelerator MMIO registers.
2. CPU writes `START=1`.
3. The IP model reads input bytes from RAM0, runs the algorithm, writes output
   bytes back to RAM0, then sets `DONE=1`.
4. If interrupt enable is set, the IP asserts its PLIC IRQ.
5. CPU clears `DONE/ERROR` with write-1-to-clear status bits.

## ISP/VPU/NPU Common Register Window

The three algorithmic accelerators should share this first-pass register layout
where possible. IP-specific algorithm parameters live in the parameter window.

| Offset range | Name | Access | Notes |
|---:|---|---|---|
| `0x0000` - `0x00FF` | common control/status | R/W | `CTRL`, `STATUS`, `IRQ_ENABLE`, `IRQ_STATUS`, `CFG`, dimensions, formats, strides. |
| `0x0100` - `0x01FF` | buffer descriptors | R/W | 32-bit physical addresses and sizes for source, destination, scratch, weights. |
| `0x0200` - `0x03FF` | job queue / command slots | R/W | Optional ring or multi-job descriptors. |
| `0x0400` - `0x0FFF` | performance/debug counters | R/W or RO | Cycle count, bytes read/written, last error, frame counter. |
| `0x1000` - `0x7FFF` | algorithm parameter bank | R/W | ISP tuning tables, VPU coefficients, NPU layer/tensor config. |
| `0x8000` - `0xFFFF` | implementation reserved | R/W | Future local SRAM aperture, debug, or extra registers. |

Common low-offset registers:

| Offset | Register | Access | Notes |
|---:|---|---|---|
| `0x0000` | `CTRL` | R/W | bit0 `ENABLE`, bit1 `START`, bit2 `SOFT_RESET`, bit3 `IRQ_EN`. |
| `0x0004` | `STATUS` | R/W1C | bit0 `BUSY`, bit1 `DONE`, bit2 `ERROR`, bit3 `IDLE`. |
| `0x0008` | `IRQ_ENABLE` | R/W | bit0 `DONE`, bit1 `ERROR`. |
| `0x000C` | `IRQ_STATUS` | R/W1C | Mirrors interrupt causes. |
| `0x0010` | `SRC_ADDR` | R/W | 32-bit physical source address in RAM0. |
| `0x0014` | `DST_ADDR` | R/W | 32-bit physical destination address in RAM0. |
| `0x0018` | `SCRATCH_ADDR` | R/W | Optional scratch/work buffer in RAM0. |
| `0x001C` | `SRC_SIZE_BYTES` | R/W | Bounds check input reads. |
| `0x0020` | `DST_SIZE_BYTES` | R/W | Bounds check output writes. |
| `0x0024` | `WIDTH` | R/W | Frame/tensor width. |
| `0x0028` | `HEIGHT` | R/W | Frame/tensor height. |
| `0x002C` | `STRIDE` | R/W | Source stride in bytes. |
| `0x0030` | `FORMAT` | R/W | IP-specific pixel/tensor format enum. |
| `0x0034` | `OP_MODE` | R/W | IP-specific algorithm mode. |
| `0x0038` | `WEIGHTS_ADDR` | R/W | NPU weight/model base; reserved for ISP/VPU unless needed. |
| `0x003C` | `PARAM_ADDR` | R/W | Optional RAM0 parameter table pointer. |

## QSPI Flash Addressing

`flash_nor_tlm` is a serial NOR device behind `qspi_tlm`; it is not directly
mapped on the CPU bus in the current model.

| Device | CPU window | Serial address range | Size | Notes |
|---|---:|---:|---:|---|
| FLASH0 behind QSPI0 | none | `0x000000` - `0xFFFFFF` | 16 MiB | Accessed only by programming QSPI0 registers. JEDEC ID modeled as `EF 40 18`. |
| Reserved XIP aperture | `0x2000_0000` | n/a | 16 MiB | Reserved for a future execute-in-place bridge. Do not bind this window until the QSPI model supports memory-mapped read mode. |

Boot-from-flash flow, when implemented:

1. CPU starts in BOOTROM0 at `0x0000_0000`.
2. BOOTROM programs QSPI0 at `0x100C_0000`.
3. QSPI0 reads FLASH0 serial address `0x000000`.
4. BOOTROM copies the application image to RAM0 at `0x8000_0000`.
5. BOOTROM jumps to the application entry in RAM.

For the current Bremen wrapper and tests, direct ELF loading to RAM is simpler:
the ELF loader writes sections through the CPU bus and initializes PC to the ELF
entry point.

## PLIC IRQ Map

PLIC source IDs are 1-based. PLIC source ID 0 is reserved by the RISC-V PLIC
architecture and must not be assigned.

| PLIC Source | Signal | Status | Notes |
|---:|---|---|---|
| 1 | `uart0.irq` | assigned | UART0 (`uart2_tlm`). Level-sensitive combined UARTINTR via `sc_out<bool> irq`. |
| 2 | `i2c0.irq` | assigned | Current I2C model output is `irq`. |
| 3 | `spi0.irq` / `spi0.intr` | assigned | Existing SPI tests already use source 3. |
| 4 | `timer0.irq_out` | assigned | Peripheral timer interrupt. |
| 5 | `wdt0.irq` | assigned | Existing WDT tests already use source 5. |
| 6 | `pwm0.irq` | reserved | Current PWM model has no IRQ output; keep this ID reserved. |
| 7 | `dma0.irq_nonzero` | assigned | Reduce `dma_tlm::irq` vector/nonzero level to one PLIC input. |
| 8 | `dma0.irq_abort` | assigned | DMA abort/fault interrupt. |
| 9 | `trng0.irq_out` | assigned | TRNG interrupt. |
| 10 | `cmu0.irq` | reserved | Current `clkmgr_tlm` has no IRQ output; keep source tied low. |
| 11 | `pmu0.wakeup_irq` | assigned | PMU wakeup/low-power event interrupt. |
| 12 | `dmic0.irq_out` | assigned | DMIC FIFO watermark/overrun interrupt. |
| 13 | `otp0.irq_out` | assigned | OTP operation-done/error interrupt. |
| 14 | `qspi0.irq` | assigned | QSPI transfer-done interrupt. |
| 15 | `isp0.irq` | reserved | Planned ISP interrupt. Tie low until the ISP model exposes an IRQ output. |
| 16 | `vpu0.irq` | reserved | Planned VPU interrupt. Tie low until the VPU model exposes an IRQ output. |
| 17 | `npu0.irq` | reserved | Planned NPU interrupt. Tie low until the NPU model exposes an IRQ output. |
| 18 | `uart1.irq` | assigned | Second UART (`uart2_tlm`). Same `sc_out<bool> irq` contract as UART0. |
| 19 | `i2c1.irq` | assigned | Second I2C interrupt. |
| 20 | `spi1.irq` | assigned | Second SPI interrupt. |
| 21 | `timer1.irq_out` | assigned | Second peripheral timer interrupt. |
| 22 | `rtc0.irq_out` | assigned | RTC alarm interrupt. `rtc_tlm` exposes `sc_out<bool> irq_out`. |
| 23 | `adc0.irq_out` | assigned | ADC0 interrupt (VP_FX1 full SoC). |
| 24-31 | reserved | reserved | Keep free for GPIO, AES, additional instances, or future platform IP. GPIO0 exists (0x1016_0000) but has no IRQ output in this revision; source 24 stays reserved for it. |

Recommended PLIC construction for the integrated platform:

```cpp
constexpr unsigned kNumPlicSources = 31;
cdc::components::plic_tlm plic("plic", cpu, kNumPlicSources);
```

The platform should bind `plic.irq_in[source_id - 1]` for each assigned source.
Reserved sources should be tied low or left unconnected only if the PLIC wrapper
handles default-low signals safely.

## Integration Notes

| IP | Bus socket | IRQ/reset notes |
|---|---|---|
| UART0 | `UartTLM::bus` (`uart2_tlm`) | Selected model: `uart2_tlm`. TX via `sc_out<unsigned char> tx`. IRQ port is `irq` (level-sensitive UARTINTR) to the PLIC. |
| I2C0 | `i2c::socket` | IRQ port is `irq`. |
| SPI0 | `spi_tlm::socket` | IRQ port is `irq` in the component implementation; platform logs may call it `intr`. |
| TIMER0 | `Timer::socket` | IRQ port is `irq_out`. |
| WDT0 | `wdt_tlm::target_socket` | Bind `reset_n`; route `reset_o` to SoC reset controller/PMU if modeled. |
| PWM0 | `PWM::socket` | No current IRQ output. |
| DMA0 | `dma_tlm::target_socket`, `dma_tlm::master_socket` | DMA master must be routed through the same physical address map as the CPU, especially RAM0. |
| TRNG0 | `trng_tlm::socket` | Bind `reset_n`; IRQ port is `irq_out`. |
| CMU0 | `Clkmgr::socket` | Bind AST ack/idle inputs. No current IRQ output. |
| PMU0 | `Pwrmgr::tl_socket` | Requires boot/reset environment signals: `por_rst_n`, OTP/LC/ROM done, wakeups, reset requests, and power-good inputs. |
| DMIC0 | `DmicTLM::bus_target_socket` | `pdm_target_socket` is not CPU MMIO; use it for PDM stimulus/input stream. |
| OTP0 | `otp::socket` | IRQ port is `irq_out`. |
| QSPI0 | `qspi_tlm::from_apb_socket` | Bind `qspi_tlm::to_flash_socket` to `flash_nor_tlm::from_qspi_socket`; bind `reset_n`. |
| FLASH0 | `flash_nor_tlm::from_qspi_socket` | Not a CPU target. Load image with `flash.load(...)` before simulation or from a platform helper. |
| ISP0 | planned target + master sockets | Planned IP. MMIO controls the job; master socket reads RAW_IN0 and writes ISP_OUT0. |
| VPU0 | planned target + master sockets | Planned IP. MMIO controls the job; master socket reads ISP_OUT0 and writes VPU_OUT0. |
| NPU0 | planned target + master sockets | Planned IP. MMIO controls the job; master socket reads VPU_OUT0 and NPU_WEIGHTS0, then writes tensors/results to NPU_WORK0. |
| UART1 | `UartTLM::bus` (`uart2_tlm`) | Second instance; identical socket/IRQ contract to UART0 (`irq` port). |
| I2C1 | `i2c::socket` | IRQ port is `irq`. |
| SPI1 | `spi_tlm::socket` | IRQ port is `irq`. |
| TIMER1 | `Timer::socket` | IRQ port is `irq_out`. |
| RTC0 | `rtc_tlm::socket` | Implemented (`rtc_tlm`). Bind `reset_n`; IRQ port is `irq_out` to the PLIC. Optional future wakeup line to PMU. |
| GPIO0 | `gpio_tlm::socket` | No IRQ in this revision. External pin stimulus (boot strap, testbench, CLI) via the `set_pin()` C++ API. |
| BOOTROM0/IFLASH0 | `memory_tlm::socket` (read-only) | Functional writes fail; `transport_dbg` writes bypass the protect (image-loader backdoor). |
| SPI_FLASH0 | `flash_nor_tlm::from_spi_socket` | NOR behind SPI0 (byte-stream face, 8-bit frames, CMD `0x03` only). Chip-select: bridge `spi_tlm::cs_n` to `flash_nor_tlm::spi_cs()`; command state machine resets on CS deassert. Deselected = frame echoed back (legacy loopback behavior). |

## C/C++ Address Defines

Use these constants in firmware headers and platform top-level code:

```c
#define CDC_BOOTROM_BASE  0x00000000u
#define CDC_CLINT_BASE    0x02000000u
#define CDC_PLIC_BASE     0x0C000000u

#define CDC_UART0_BASE    0x10000000u
#define CDC_I2C0_BASE     0x10010000u
#define CDC_SPI0_BASE     0x10020000u
#define CDC_TIMER0_BASE   0x10030000u
#define CDC_WDT0_BASE     0x10040000u
#define CDC_PWM0_BASE     0x10050000u
#define CDC_DMA0_BASE     0x10060000u
#define CDC_TRNG0_BASE    0x10070000u
#define CDC_CMU0_BASE     0x10080000u
#define CDC_PMU0_BASE     0x10090000u
#define CDC_DMIC0_BASE    0x100A0000u
#define CDC_OTP0_BASE     0x100B0000u
#define CDC_QSPI0_BASE    0x100C0000u
#define CDC_ISP0_BASE     0x100D0000u
#define CDC_VPU0_BASE     0x100E0000u
#define CDC_NPU0_BASE     0x100F0000u

#define CDC_UART1_BASE    0x10100000u
#define CDC_I2C1_BASE     0x10110000u
#define CDC_SPI1_BASE     0x10120000u
#define CDC_TIMER1_BASE   0x10130000u
#define CDC_RTC0_BASE     0x10140000u
#define CDC_ADC0_BASE     0x10150000u
#define CDC_GPIO0_BASE    0x10160000u

/* ROM-code boot flow (docs/romcode_boot_hw_plan.md) */
#define CDC_BOOTROM_SIZE  0x00010000u  /* 64 KiB ROM, ROM-code entry 0x0    */
#define CDC_IFLASH_BASE   0x04000000u  /* 4 MiB internal code flash (XIP)   */
#define CDC_IFLASH_SIZE   0x00400000u
#define CDC_GPIO_VALUE    0x00u        /* RO pin levels                     */
#define CDC_GPIO_OUT      0x04u
#define CDC_GPIO_DIR      0x08u        /* 1=output, reset: all inputs       */
#define CDC_GPIO_BOOT_PIN 1u           /* strap: LOW=app, HIGH=download     */
#define CDC_SPI_CSR       0x28u        /* SPI0 vendor reg: bit0 = CS assert */

#define CDC_ACCEL_MMIO_SIZE 0x00010000u

#define CDC_RAM0_BASE     0x80000000u
#define CDC_RAM0_SIZE     0x10000000u

#define CDC_FW_BASE       0x80000000u
#define CDC_FW_SIZE       0x01000000u
#define CDC_RAW_IN0_BASE  0x81000000u
#define CDC_RAW_IN0_SIZE  0x01000000u
#define CDC_ISP_OUT0_BASE 0x82000000u
#define CDC_ISP_OUT0_SIZE 0x02000000u
#define CDC_VPU_OUT0_BASE 0x84000000u
#define CDC_VPU_OUT0_SIZE 0x02000000u
#define CDC_NPU_WGT0_BASE 0x86000000u
#define CDC_NPU_WGT0_SIZE 0x02000000u
#define CDC_NPU_WORK0_BASE 0x88000000u
#define CDC_NPU_WORK0_SIZE 0x04000000u
```

```c
#define CDC_IRQ_UART0       1u  /* uart2_tlm irq (combined UARTINTR) */
#define CDC_IRQ_I2C0        2u
#define CDC_IRQ_SPI0        3u
#define CDC_IRQ_TIMER0      4u
#define CDC_IRQ_WDT0        5u
#define CDC_IRQ_PWM0        6u  /* reserved until PWM IRQ is modeled */
#define CDC_IRQ_DMA0        7u
#define CDC_IRQ_DMA0_ABORT  8u
#define CDC_IRQ_TRNG0       9u
#define CDC_IRQ_CMU0       10u  /* reserved/tied low */
#define CDC_IRQ_PMU0       11u
#define CDC_IRQ_DMIC0      12u
#define CDC_IRQ_OTP0       13u
#define CDC_IRQ_QSPI0      14u
#define CDC_IRQ_ISP0       15u  /* reserved until ISP IRQ is modeled */
#define CDC_IRQ_VPU0       16u  /* reserved until VPU IRQ is modeled */
#define CDC_IRQ_NPU0       17u  /* reserved until NPU IRQ is modeled */
#define CDC_IRQ_UART1      18u  /* uart2_tlm irq (combined UARTINTR) */
#define CDC_IRQ_I2C1       19u
#define CDC_IRQ_SPI1       20u
#define CDC_IRQ_TIMER1     21u
#define CDC_IRQ_RTC0       22u  /* rtc_tlm irq_out (RTC alarm) */
#define CDC_IRQ_ADC0       23u  /* adc_tlm irq_out */
```

## Platform Build Checklist

1. Instantiate CPU, RAM0, CLINT0, PLIC0, bus router, and all listed IPs.
2. Configure `bus_router` with enough initiator ports for CPU instruction/data,
   DMA master, ISP0 master, VPU0 master, and NPU0 master.
3. Add all address windows from the Address Map table.
4. Bind PLIC sources using the PLIC IRQ Map table.
5. Tie reserved IRQ sources low.
6. Bind all active-low resets (`reset_n`) to a common SoC reset signal unless an
   IP-specific reset is required.
7. Bind QSPI0 to FLASH0 through the QSPI/flash socket pair.
8. Bind ISP0/VPU0/NPU0 target sockets for MMIO and master sockets for RAM0 data
   movement.
9. Route the image/AI pipeline through RAM0 buffers:
   `RAW_IN0 -> ISP_OUT0 -> VPU_OUT0 -> NPU_WORK0`.
10. For direct ELF boot, link firmware at `0x8000_0000`.
11. For bootrom/QSPI boot, link bootrom at `0x0000_0000` and application image for
   RAM execution at `0x8000_0000`.
