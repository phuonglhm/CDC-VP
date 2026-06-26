# Bremen SoC Peripheral Memory Map and IRQ Spec

Target integration spec for a complete Bremen `riscv-vp` RV32 platform built
from these TLM IPs:

`UART, I2C, SPI, Timer, Watchdog, PWM, DMA, TRNG, CMU, PMU, DMIC, OTP, QSPI,
flash_nor`.

This document is the SoC-level assignment. Some existing single-IP test
platforms reuse temporary addresses such as `0x1006_0000`; those are test-local
maps and must not be copied into the final integrated SoC.

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
| Optional boot ROM flow | If a bootrom ELF is added, set its entry to `0x0000_0000`; bootrom may copy from QSPI flash to RAM and jump to `0x8000_0000` |

## Address Map

All MMIO peripherals use a 4 KiB active register window. Peripherals are placed
on 64 KiB boundaries to keep room for future register growth, debug windows, or
second instances without changing the first-instance base addresses.

| Region | Base | Size | End | CPU access | Notes |
|---|---:|---:|---:|---|---|
| BOOTROM0 | `0x0000_0000` | `0x0001_0000` | `0x0000_FFFF` | optional ROM | Optional first-stage boot image. Not required when loading firmware ELF directly to RAM. |
| CLINT0 | `0x0200_0000` | `0x0001_0000` | `0x0200_FFFF` | MMIO | RISC-V local MSIP/MTIP source. |
| PLIC0 | `0x0C00_0000` | `0x0040_0000` | `0x0C3F_FFFF` | MMIO | External interrupt controller, hart0 M-mode context. |
| UART0 | `0x1000_0000` | `0x0000_1000` | `0x1000_0FFF` | MMIO | Console UART. Current `uart_tlm` is TX-only; `uart2_tlm` has PL011-style registers. |
| I2C0 | `0x1001_0000` | `0x0000_1000` | `0x1001_0FFF` | MMIO | I2C controller. |
| SPI0 | `0x1002_0000` | `0x0000_1000` | `0x1002_0FFF` | MMIO | SPI controller. |
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
| RAM0 | `0x8000_0000` | `0x0100_0000` | `0x80FF_FFFF` | RAM | Firmware text/data/heap/stack. Minimum supported test size is 1 MiB; final SoC target is 16 MiB. |

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
| 1 | `uart0.irq` | reserved | Reserve for a future UART RX/TX interrupt. Current `uart_tlm` console model has no IRQ output. |
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
| 15-31 | reserved | reserved | Keep free for GPIO, ADC, AES, second instances, or future platform IP. |

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
| UART0 | `uart_tlm::socket` or `UartTLM::bus` | Choose one UART model for the final SoC. Use `uart_tlm` for simple TX console; use `uart2_tlm` if firmware needs PL011-style registers. |
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

#define CDC_RAM0_BASE     0x80000000u
#define CDC_RAM0_SIZE     0x01000000u
```

```c
#define CDC_IRQ_UART0       1u  /* reserved until UART IRQ is modeled */
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
```

## Platform Build Checklist

1. Instantiate CPU, RAM0, CLINT0, PLIC0, bus router, and all listed IPs.
2. Configure `bus_router` with enough initiator ports for CPU instruction/data
   plus DMA master if DMA uses a separate initiator path.
3. Add all address windows from the Address Map table.
4. Bind PLIC sources using the PLIC IRQ Map table.
5. Tie reserved IRQ sources low.
6. Bind all active-low resets (`reset_n`) to a common SoC reset signal unless an
   IP-specific reset is required.
7. Bind QSPI0 to FLASH0 through the QSPI/flash socket pair.
8. For direct ELF boot, link firmware at `0x8000_0000`.
9. For bootrom/QSPI boot, link bootrom at `0x0000_0000` and application image for
   RAM execution at `0x8000_0000`.
