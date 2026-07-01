# CDC-VP — Virtual SoC Platform

> SystemC / TLM-2.0 / C++17 virtual SoC for early firmware, driver, and
> architecture development before silicon.
>
> **Status legend used throughout:**
> `✅ Implemented & verified` · `◐ Modeled (partial / stub)` · `⛯ Planned / Research`
>
> Doc revision: 2026-06-29. Verified against the repository at this date.

---

## Table of contents
1. [Overview & goals](#1-overview--goals)
2. [Quick start (build & run)](#2-quick-start-build--run)
3. [Repository & platform layout](#3-repository--platform-layout)
4. [Architecture: bus, clock, reset](#4-architecture-bus-clock-reset)
5. [Memory map](#5-memory-map)
6. [Interrupt architecture & programming](#6-interrupt-architecture--programming)
7. [CPU subsystem](#7-cpu-subsystem)
8. [Peripheral programming reference](#8-peripheral-programming-reference)
9. [Boot & firmware contract](#9-boot--firmware-contract)
10. [Verification & test flow](#10-verification--test-flow)
11. [Status / maturity matrix](#11-status--maturity-matrix)
12. [Scope & limitations / future work](#12-scope--limitations--future-work)
13. [Extending the platform (add an IP)](#13-extending-the-platform-add-an-ip)
14. [Roadmap: security/boot & software ecosystem](#14-roadmap-securityboot--software-ecosystem)
15. [Provenance, licenses, versions](#15-provenance-licenses-versions)
16. [References](#16-references)

---

## 1. Overview & goals

CDC-VP is a SystemC/TLM virtual SoC used to bring up software and validate system
architecture early. It is **firmware-driven**: software configures IPs through MMIO
registers, programs RAM buffer addresses, starts execution via control registers,
and handles done/error events through the PLIC. The memory map and IRQ map are
fixed at SoC-spec level so firmware, drivers, and TLM IP models develop independently.

The primary integrated RISC-V CPU backend:

- **`riscv_vp`** — Bremen *RISC-V VP* rv32 ISS (application-class, boots an RTOS, ≈ ARM Cortex-A).

The `riscv_tlm` backend from mariusmm RISC-V-TLM is kept as an alternate/evaluation
backend. The SoC target described in this document is the Bremen `riscv_vp`
configuration unless stated otherwise.

Goals: boot an RTOS (FreeRTOS first), verify each TLM IP through MMIO + PLIC,
validate drivers, and grow toward a media/AI accelerator pipeline
`RAW → ISP → VPU → NPU`.

### Current capability snapshot

| Area | Current status | Platform meaning |
|---|---|---|
| CPU execution | ✅ Bremen RV32 ISS, direct ELF load | Bare-metal firmware can execute real RISC-V code from RAM. |
| Interrupts | ✅ CLINT + PLIC | Firmware can validate timer tick, software IRQ, and external IRQ paths. |
| Peripheral MMIO | ✅ Most listed IPs have TLM models and CTests | Driver-visible register models exist for per-IP verification. |
| DMA / masters | ✅ DMA has target + master socket | Non-CPU RAM traffic can be validated through the same address map. |
| QSPI / NOR | ✅ Register-driven QSPI to serial NOR | Flash transactions work through QSPI; CPU XIP is not implemented. |
| RTOS | ⛯ FreeRTOS target, port not yet created | The hardware hooks exist; BSP/porting work remains. |
| Full SoC top | ⛯ Target spec exists, final integrated top not assembled | Current platforms are per-IP and smaller integration platforms. |
| Media/AI | ⛯ ISP/VPU/NPU planned | Memory map and programming contract are reserved; algorithms not implemented. |

### Platform contract

CDC-VP should be treated as a **software-visible SoC contract**, not only a
collection of component models:

- The address map in [`peripheral_memory_map.md`](peripheral_memory_map.md) is the
  authoritative SoC-level contract. Single-IP test-local addresses must not be copied
  into the final integrated SoC.
- The PLIC source map is fixed for firmware and driver development. Source ID 0 is
  reserved by the RISC-V PLIC architecture and must never be assigned.
- All normal MMIO registers are accessed as 32-bit little-endian words. Component
  models should reject unsupported width, command, or unaligned accesses with a TLM
  error response.
- Bus target addresses received by an IP are region-local; the interconnect subtracts
  the region base before forwarding the transaction.
- CPU, DMA, and future ISP/VPU/NPU master sockets must share the same physical RAM map.
  A buffer address programmed by firmware must mean the same thing to all bus masters.
- Interrupts into the PLIC are modeled as level-sensitive device signals. Firmware
  clears the device interrupt cause first, then completes the PLIC claim.
- Reset is active-low (`reset_n`) where modeled. IPs without reset inputs are treated
  as always initialized at elaboration time.
- Timing is loosely timed and functional. This platform validates register semantics,
  driver flows, IRQ ordering, and memory movement; it is not a timing-closure model.

---

## 2. Quick start (build & run)

**Host / toolchain** (reference): AlmaLinux 9, GCC, CMake, SystemC 2.3.4 at
`/opt/systemc-2.3.4`, Boost (for the Bremen core), and the RISC-V bare-metal
toolchain `riscv-none-elf` (xpack GCC).

```bash
# 0. Deterministic host compiler selection
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++
export PATH=/usr/bin:/bin:$PATH
$CC -dumpfullversion
$CXX --version | head -n 1

# 1. Environment (sets RISC-V toolchain on PATH)
source tools/third_party/setup_env.sh

# 2. Fetch the external CPU core into third_party/ (Bremen riscv-vp, MIT)
tools/third_party/setup_third_party.sh

# 3. Configure + build (from the repo root)
cmake -S . -B build \
  -DCDC_CPU_BACKEND=riscv_vp \
  -DCDC_BUILD_CUSTOM_SOC=ON -DCDC_BUILD_TESTS=ON \
  -DCDC_BUILD_MINI_TLM=OFF -DCDC_BUILD_CPU_EVAL=OFF
cmake --build build -j"$(nproc)"

# 4. Run the component test suite
ctest --test-dir build --output-on-failure

# 5. No-firmware smoke run of a per-IP platform (elaboration check)
build/platforms/tests/uart_platform/uart_platform -c \
  platforms/tests/uart_platform/configs/default.yaml --sim-ms 0

# 6. Build and run firmware
make -C fw/hello_baremetal_riscv          # -> hello.elf (toolchain from step 1)
build/platforms/tests/uart_platform/uart_platform \
  -c platforms/tests/uart_platform/configs/default.yaml --fw fw/hello_baremetal_riscv/hello.elf
```

CMake presets `debug` / `release` are available (`CMakePresets.json`). Top-level
options: `CDC_BUILD_MINI_TLM`, `CDC_BUILD_CPU_EVAL`, `CDC_BUILD_CUSTOM_SOC`,
`CDC_BUILD_TESTS`, `CDC_CPU_BACKEND`.

---

## 3. Repository & platform layout

```
CDC-VP/
  components/     Reusable TLM IP libraries  (cdc::components::<ip>)
  cpu_models/     cpu_base interface + riscv_vp wrapper
  platforms/      SoC tops and per-IP test platforms
  fw/             Bare-metal firmware + fw/common HAL (mmio/uart/linker/startup)
  docs/           Spec, diagrams, this document
  third_party/    External cores (riscv-vp, RISC-V-TLM)  [gitignored]
  tools/          Environment + third-party setup scripts
  tests/support/  Shared test helpers (tlm_probe.h)
```

**Platforms**

| Platform | Purpose | Status |
|---|---|---|
| `platforms/tests/<ip>_platform` | One-IP harness (CPU+bus+RAM+UART+IP) for smoke/driver tests | ✅ |
| `platforms/tests/uart_platform` | UART console reference platform (uart2_tlm) | ✅ |
| `platforms/riscv_cpu_eval` | CPU/quantum benchmark harness | ✅ |
| `platforms/riscv_custom_soc` | Smaller integration SoC | ✅ builds |
| `platforms/mini_soc` | Minimal multi-IP integration | ✅ builds |
| `platforms/mini_tlm` | Legacy mini platform | ◐ does not build (legacy timer API) |
| Full integrated SoC (per memory map) | Final target top | ⛯ not yet assembled |

**Expected deliverables from the repository**

| Deliverable | Location | Purpose |
|---|---|---|
| SoC-level memory/IRQ spec | `docs/peripheral_memory_map.md` | Firmware and platform integration source of truth. |
| Main platform spec | `docs/CDC-VP_VIRTUAL_SoC_PLATFORM.md` | Human-readable development-platform contract. |
| Block/data-flow diagrams | `docs/*diagram.svg`, `docs/*diagram.png` | Architecture review and spec inclusion. |
| Per-IP component libraries | `components/<ip>` | Reusable TLM IP models with focused tests. |
| Per-IP CPU platforms | `platforms/tests/<ip>_platform` | Firmware-driven IP smoke and driver validation. |
| Firmware examples | `fw/*_riscv` | Bare-metal proof that real RISC-V code can drive the models. |

---

## 4. Architecture: bus, clock, reset

Block diagram: [`docs/virtual_soc_block_diagram.svg`](virtual_soc_block_diagram.svg)
(PNG: `virtual_soc_block_diagram.png`).

### Bus protocol — TLM-2.0 loosely-timed
- Sockets: `simple_initiator_socket` / `simple_target_socket`. Transport: blocking
  `b_transport` (timed via `delay`) + `transport_dbg` (untimed backdoor).
- **No DMI** (`get_direct_mem_ptr` not forwarded by `bus_router`) → every fetch/load
  is a `b_transport`. This is the main simulation-speed limiter.
- Registers are **32-bit**; many IPs require **4-byte aligned 32-bit** access, else
  `TLM_ADDRESS_ERROR_RESPONSE`; bad command → `TLM_COMMAND_ERROR_RESPONSE`.
- `bus_router`: address-decode interconnect, region-local translation (`addr-base`),
  shared map for all initiators (CPU instr/data, DMA master, accelerator masters).

### Clock — loosely-timed, no real clock tree
- Time is modeled per-IP via `sc_time`: `access_latency` (default 10 ns) on each
  access; `tick_period` for counters (Timer 20 ns, RTC 1 s, …); **CLINT
  `mtime`/`mtimecmp` in µs** (RTOS tick source).
- **CMU0** (`clkmgr_tlm`) models the *software* clock-management interface only; it
  **does not gate real clocks** and has no IRQ. ◐

### Reset — active-low `reset_n`
- Present on adc, dma, dmic, qspi, rtc, spi, timer, trng, wdt.
- PMU0 is the reset/power origin (FSM: `por_rst_n`, `sw_rst_req`, `ndmreset_req` →
  `rst_lc_n`, `sys_rst_n`). WDT adds a `reset_o` (watchdog reset request).
- No global reset tree is modeled; platform tops tie `reset_n` to a common signal.

### Integration rules for platform tops

| Rule | Required behavior |
|---|---|
| Address decode | Every target is registered once with base/size; overlapping regions are invalid. |
| Local address | The bus forwards `addr - base` to the target IP. IP code should not compare against SoC physical bases. |
| Multiple initiators | CPU, DMA, and future accelerators use the same router/map to reach RAM and MMIO. |
| Unified vs split CPU bus | Bremen `riscv_vp` exposes one upstream bus; mariusmm `riscv_tlm` exposes instruction and data buses. Platform tops must handle both if backend-selectable. |
| Reserved regions | Reserved XIP and planned accelerator windows may be documented before implementation, but must not be bound to fake behavior that firmware could depend on. |
| Error policy | Decode miss or illegal access should return a TLM error, not silently succeed. |

---

## 5. Memory map

| Region | Base | End | Size | Access | Notes |
|---|---|---|---|---|---|
| BOOTROM0 | 0x0000_0000 | 0x0000_FFFF | 64 KiB | ROM (optional) | First-stage boot ⛯ |
| CLINT0 | 0x0200_0000 | 0x0200_FFFF | 64 KiB | MMIO | MSIP/MTIP |
| PLIC0 | 0x0C00_0000 | 0x0C3F_FFFF | 4 MiB | MMIO | External IRQ, hart0 M-mode |
| UART0 | 0x1000_0000 | 0x1000_0FFF | 4 KiB | MMIO | uart2_tlm (PL011) |
| I2C0 | 0x1001_0000 | 0x1001_0FFF | 4 KiB | MMIO | |
| SPI0 | 0x1002_0000 | 0x1002_0FFF | 4 KiB | MMIO | |
| TIMER0 | 0x1003_0000 | 0x1003_0FFF | 4 KiB | MMIO | |
| WDT0 | 0x1004_0000 | 0x1004_0FFF | 4 KiB | MMIO | |
| PWM0 | 0x1005_0000 | 0x1005_0FFF | 4 KiB | MMIO | |
| DMA0 | 0x1006_0000 | 0x1006_0FFF | 4 KiB | MMIO + master | |
| TRNG0 | 0x1007_0000 | 0x1007_0FFF | 4 KiB | MMIO | |
| CMU0 | 0x1008_0000 | 0x1008_0FFF | 4 KiB | MMIO | clkmgr_tlm |
| PMU0 | 0x1009_0000 | 0x1009_0FFF | 4 KiB | MMIO | pmu_tlm |
| DMIC0 | 0x100A_0000 | 0x100A_0FFF | 4 KiB | MMIO | PDM |
| OTP0 | 0x100B_0000 | 0x100B_0FFF | 4 KiB | MMIO | |
| QSPI0 | 0x100C_0000 | 0x100C_0FFF | 4 KiB | MMIO | NOR flash behind it |
| ISP0 | 0x100D_0000 | 0x100D_FFFF | 64 KiB | MMIO + master | ⛯ planned |
| VPU0 | 0x100E_0000 | 0x100E_FFFF | 64 KiB | MMIO + master | ⛯ planned |
| NPU0 | 0x100F_0000 | 0x100F_FFFF | 64 KiB | MMIO + master | ⛯ planned |
| UART1 | 0x1010_0000 | 0x1010_0FFF | 4 KiB | MMIO | uart2_tlm (PL011) |
| I2C1 | 0x1011_0000 | 0x1011_0FFF | 4 KiB | MMIO | |
| SPI1 | 0x1012_0000 | 0x1012_0FFF | 4 KiB | MMIO | |
| TIMER1 | 0x1013_0000 | 0x1013_0FFF | 4 KiB | MMIO | |
| RTC0 | 0x1014_0000 | 0x1014_0FFF | 4 KiB | MMIO | rtc_tlm (PL031) |
| RAM0 | 0x8000_0000 | 0x8FFF_FFFF | 256 MiB | RAM/DDR | see layout below |
| (XIP aperture) | 0x2000_0000 | — | 16 MiB | reserved | ⛯ QSPI XIP read mode |

**RAM0 internal layout**

| Window | Range | Producer → Consumer |
|---|---|---|
| FW / RTOS | 0x8000_0000 – 0x80FF_FFFF | loader → CPU |
| RAW_IN0 | 0x8100_0000 – 0x81FF_FFFF | sensor/TB → ISP0 |
| ISP_OUT0 | 0x8200_0000 – 0x83FF_FFFF | ISP0 → VPU0 |
| VPU_OUT0 | 0x8400_0000 – 0x85FF_FFFF | VPU0 → NPU0 |
| NPU_WEIGHTS0 | 0x8600_0000 – 0x87FF_FFFF | loader → NPU0 |
| NPU_WORK0 | 0x8800_0000 – 0x8BFF_FFFF | NPU0/CPU |
| PIPELINE_RSVD | 0x8C00_0000 – 0x8FFF_FFFF | reserved |

Authoritative source: [`docs/peripheral_memory_map.md`](peripheral_memory_map.md).
**ADC0 base/IRQ are not yet assigned (TBD).**

---

## 6. Interrupt architecture & programming

- **CLINT** (local): `msip @ +0x0000`, `mtimecmp @ +0x4000`, `mtime @ +0xBFF8`
  (µs units). Drives **MSIP (cause 3)** and **MTIP (cause 7)**. RTOS tick =
  program `mtimecmp = mtime + interval`.
- **PLIC** (external): up to 31 sources, programmable priority/threshold,
  pending/enable, level-sensitive gateway, claim/complete. Drives **MEIP (cause 11)**.
  Source id = array index + 1.
- CPU injection is level-triggered via `cpu.set_irq(cause, level)`.

**Typical external-IRQ handler flow:** firmware enables `mstatus.MIE` + `mie.MEIE`,
sets PLIC priority/threshold and source enable → on trap, **claim** (read claim reg)
→ service the device (clear its W1C status) → **complete** (write claim reg back).

**PLIC source map**

| ID | Signal | Status |
|---:|---|---|
| 0 | — | reserved (RISC-V) |
| 1 | uart0.irq (UARTINTR) | ✅ |
| 2 | i2c0.irq | ✅ |
| 3 | spi0.irq | ✅ |
| 4 | timer0.irq_out | ✅ |
| 5 | wdt0.irq | ✅ |
| 6 | pwm0.irq | ◐ reserved (no IRQ output yet) |
| 7 | dma0.irq_nonzero | ✅ |
| 8 | dma0.irq_abort | ✅ |
| 9 | trng0.irq_out | ✅ |
| 10 | cmu0.irq | ◐ reserved (no IRQ output) |
| 11 | pmu0.wakeup_irq | ✅ |
| 12 | dmic0.irq_out | ✅ |
| 13 | otp0.irq_out | ✅ |
| 14 | qspi0.irq | ✅ |
| 15–17 | isp0/vpu0/npu0.irq | ⛯ reserved |
| 18 | uart1.irq | ✅ |
| 19 | i2c1.irq | ✅ |
| 20 | spi1.irq | ✅ |
| 21 | timer1.irq_out | ✅ |
| 22 | rtc0.irq_out | ✅ |
| 23–31 | — | reserved (GPIO/ADC/AES/future) |

---

## 7. CPU subsystem

Interface `cpu_base` (`cpu_models/include/cdc/cpu/cpu_base.h`) makes the platform
backend-agnostic; the concrete core is chosen by `-DCDC_CPU_BACKEND`.

| Group | API |
|---|---|
| Bus | `instr_bus()`, `data_bus()`, `has_unified_bus()` |
| Interrupt | `set_irq(cause, level)`, `raise_irq(cause)` (cause 3/7/11 = MSIP/MTIP/MEIP) |
| Boot/control | `load_elf(path)`, `reset_cpu()` |
| Introspection | `get_pc()`, `get_instret()`, `backend_name()` |

`cpu_config { xlen=32; num_irq=0; has_mmu=false; has_smp=false; }` — defaults:
RV32, no MMU, single-hart.

### The core (upstream capability vs CDC-VP-enabled)

| | **riscv_vp** (Bremen) |
|---|---|
| Upstream project | RISC-V VP (RV32GC/RV64GC, M/S/U, Sv32/39/48, GDB-RSP, Linux-capable) |
| **CDC-VP build** | RV32IMAC, M-mode IRQ path, **MMU off**, **single-hart** (`hart_id=0`), **GDB not wired** |
| Class | Application — boots RTOS (≈ Cortex-A) |
| Bus | **Unified** instr+data → 1 upstream port |
| Instruction timing | multi-cycle |
| Wrapper internals | `ISS` + `CombinedMemoryInterface` + `DirectCoreRunner` |
| `backend_name()` | `riscv_vp (Bremen rv32)` |

Block diagram: [`docs/cpu_block_diagram.svg`](cpu_block_diagram.svg).

### Benchmark — TLM quantum sweep (re-measured 2026-06-27, Debug build)

| Quantum | Retired instr | Host (s) | MIPS |
|---|--:|--:|--:|
| 10 ns (sync) | 482,765 | 0.108 | 4.48 |
| 1 ms | 482,765 | 0.060 | 7.98 |
| 10 ms | 482,765 | 0.060 | 7.99 |

Raising the TLM global quantum from sync-every-cycle to 1 ms lifts host throughput
~1.8× (diminishing past ~1 ms). Retired-instr counts are exact/reproducible; **MIPS
is relative only** (LT, Debug, host-variance ±10%; no DMI dominates). Full method:
[`docs/cpu_benchmark_results.md`](cpu_benchmark_results.md).

---

## 8. Peripheral programming reference

Each IP is a `cdc::components::<ip>` library. Register **bit-field** detail lives in
`components/<ip>/include/*.h` and the per-IP `README.md`; the cards below summarize
the model, MMIO window, key registers, interface, and IRQ.

> Convention: addresses in `b_transport` are **region-local** (bus_router subtracts
> the base). All register accesses are 32-bit.

- **UART** (`uart2_tlm`, PL011) — DR/FR/IBRD/FBRD/LCR_H/CR/**IFLS**/IMSC/RIS/MIS/ICR;
  16-deep FIFOs, programmable trigger; interrupts RX/TX/RT(timeout)/OE(overrun);
  ports `bus`,`tx`,`irq`,`rx`. ✅
- **SPI** (PL022-style) — periph/cell id, 8-deep FIFO; APB target + peripheral
  initiator socket; `irq`. ✅
- **I2C** (`<i2c.h>`, OpenTitan-style) — host + target FSM; FMT/RX/ACQ/TX FIFOs (64);
  INTR_STATE/FIFO_CTRL/STATUS; `irq`. ✅
- **DMIC** (PDM + CIC) — CTRL/STATUS/FIFO_DATA/FIFO_WM/INT_CLR; FIFO 64; flags
  empty/full/overrun/watermark; `bus_target_socket` + `pdm_target_socket`, `irq_out`. ✅
- **Timer** (PrimeCell-style) — CTRL/VALUE/RELOAD/INTSTATUS (ENABLE/EX_EN/EX_CLK/INTR_EN);
  ports `reset_n`, `extin`, `irq_out`. ✅
- **WDT** (SP805-style) — LOAD/VALUE/CONTROL/INTCLR down-counter; `reset_n`,`irq`,`reset_o`. ✅
- **PWM** (OpenTitan-style) — 6 channels PARAM/DUTY/BLINK, CFG/PWM_EN/INVERT, REGWEN;
  **no real IRQ yet**. ◐
- **ADC** — CONTROL/STATUS/DATA/INTR_ENABLE (0x00–0x0C); `reset_n`,`irq_out`. ✅ *(base/IRQ TBD in map)*
- **DMA** (PL330-style) — manager DSR/DPC + per-channel regs; MFIFO 1024 B; channel/manager
  faults; target + **master** socket; `irq_nonzero`, `irq_abort`. ✅
- **TRNG** (CryptoCell-style) — IMR/ISR/ICR/CONFIG/VALID, 6×32-bit EHR (192-bit), BIST;
  `reset_n`,`irq_out`. ✅
- **OTP** (OpenTitan OTP_CTRL-style) — partition status, DAI CMD/ADDR/WDATA/RDATA, read-lock,
  integrity check; `irq_out`. ✅
- **CMU** (OpenTitan clkmgr) — EXTCLK_CTRL/STATUS, CLK_ENABLES/HINTS/HINTS_STATUS, REGWEN;
  **no real clock gating, no IRQ**. ◐
- **PMU** (OpenTitan pwrmgr) — slow/fast FSM, low-power hint, reset reason; env inputs
  (wakeups/rstreqs), `wakeup_irq`. ◐ *(needs environment signals; not pure MMIO)*
- **QSPI** — CTRL; APB target + `to_flash_socket` initiator; `reset_n`,`irq`. ✅
- **NOR flash** (`flash_nor_tlm`) — JEDEC EF 40 18, 16 MiB; not CPU-mapped; only via QSPI. ✅
- **RTC** (`rtc_tlm`, PL031) — DR/MR/LR/CR/IMSC/RIS/MIS/ICR; free-running counter + alarm;
  `reset_n`,`irq_out`. ✅
- **CLINT / PLIC** — see §6.
- **ISP / VPU / NPU** — planned: MMIO target + RAM master + IRQ; shared register block
  (CTRL/STATUS/IRQ_*, SRC/DST/SCRATCH_ADDR, WIDTH/HEIGHT/STRIDE/FORMAT/OP_MODE, WEIGHTS_ADDR). ⛯

---

## 9. Boot & firmware contract

- **Link base `0x8000_0000`** (RAM0). Linker script and startup live in
  `fw/common/linker/riscv.ld` and `fw/common/startup/startup_riscv.S`.
- Startup sets up stack, clears BSS, installs `mtvec`, then calls `main`.
- **ELF load:** `cpu.load_elf()` writes the image via backdoor (`transport_dbg`) and
  sets reset PC = ELF entry; no-ELF fallback PC (Bremen) = `0x8000_0000`.
- **HAL (minimal, present):** `fw/common/drivers/mmio.h` (register accessors) and a
  `uart` driver (`uart.c/.h`). Firmware otherwise talks to IPs via raw MMIO at the
  documented bases. A full HAL/BSP is **early-stage**. ◐
- **Examples (`fw/`):** `hello_baremetal_riscv`, per-IP `*_irq_riscv` / `*_test_riscv`,
  `clint_timer_riscv`, `soc_irq_riscv`, `bench_riscv`. ✅
- **OS:** FreeRTOS is the first RTOS target — **port not yet created** ⛯;
  `zephyr_app` / `linux_minimal` are stubs (Bremen core is Linux-capable upstream). ⛯
- **Debug:** ISS-level (`get_pc`, `get_instret`). GDB/RSP **not wired** in CDC-VP. ◐

### FreeRTOS bring-up acceptance checklist

The first RTOS milestone is considered complete only when these are demonstrated
on the Bremen backend:

| Gate | Expected proof |
|---|---|
| Reset/vector entry | Startup reaches `main`, sets stack/BSS, installs `mtvec`. |
| UART console | RTOS banner and task logs print through UART0. |
| CLINT tick | `mtimecmp` drives periodic MTIP and the scheduler tick advances. |
| PLIC external IRQ | At least one peripheral IRQ is claimed, serviced, and completed through PLIC. |
| Task scheduling | Two or more tasks run with timer-driven preemption or periodic delay. |
| Driver smoke | UART + Timer + one serial IP + one stateful IP run from RTOS tasks. |
| Fault visibility | Illegal MMIO/decode errors are visible in simulation logs or test failure path. |

### BSP / firmware artifacts to keep stable

| Artifact | Purpose |
|---|---|
| `soc_memory_map.h` or equivalent | C constants for all SoC bases and RAM buffer windows. |
| `soc_irq_map.h` or equivalent | PLIC source IDs and local interrupt cause IDs. |
| Linker script | Places firmware at `0x8000_0000`, with stack/heap inside FW/RTOS RAM window. |
| Startup/trap code | Initializes `mtvec`, `mstatus`, `mie`, CLINT tick, and PLIC. |
| Minimal HAL | `mmio_read32/write32`, UART console, CLINT timer, PLIC claim/complete. |

---

## 10. Verification & test flow

- Each component ships a **self-checking CTest** (non-zero exit on failure; pattern
  in `tests/support/tlm_probe.h`). Run via `ctest --test-dir <build>`.
- Per-IP platforms support a **no-firmware smoke run** (`--sim-ms 0`) to validate
  elaboration/binding.
- Status: the component suite passes at the last full run; all listed IPs have a
  self-checking test (i2c and dmic include real register/behaviour assertions).
- Adding a software-visible IP must update `components/CMakeLists.txt`,
  `docs/peripheral_memory_map.md`, the IP test, and platform integration.

### Regression gates

| Gate | Scope | Pass criteria |
|---|---|---|
| Component unit | `components/<ip>/tests` | Self-checking CTest passes, no manual waveform/log inspection needed. |
| Platform elaboration | `platforms/tests/<ip>_platform --sim-ms 0` | All sockets, reset, clock, IRQ wiring elaborate without runtime errors. |
| Firmware smoke | Per-IP bare-metal firmware | Firmware reaches PASS marker or expected UART transcript. |
| Interrupt path | CLINT/PLIC + one external device | CPU trap handler observes correct cause, PLIC claim ID, device clear, complete. |
| DMA / master path | DMA and future accelerators | Master reads/writes RAM through physical addresses programmed by firmware. |
| SoC integration | Final integrated top | No address overlaps, no duplicate PLIC sources, all reserved sources tied low or safely unconnected. |
| RTOS milestone | FreeRTOS target | Scheduler tick, UART console, PLIC IRQ, and at least two driver tasks run. |

Recommended CI command set once the full SoC top is assembled:

```bash
cmake -S . -B build/bremen -G Ninja -DCDC_CPU_BACKEND=riscv_vp -DCDC_BUILD_TESTS=ON
cmake --build build/bremen -j"$(nproc)"
ctest --test-dir build/bremen --output-on-failure
```

---

## 11. Status / maturity matrix

| IP / block | Model origin | Functional | IRQ | reset_n | CTest | Notes |
|---|---|:--:|:--:|:--:|:--:|---|
| UART0/1 | PL011 | ✅ | ✅ | — | ✅ | RX/TX/RT/OE ints, UARTIFLS |
| I2C0/1 | OpenTitan | ✅ | ✅ | — | ✅ | host+target |
| SPI0/1 | PL022 | ✅ | ✅ | ✅ | ✅ | |
| TIMER0/1 | PrimeCell | ✅ | ✅ | ✅ | ✅ | |
| WDT0 | SP805 | ✅ | ✅ | ✅ | ✅ | + reset_o |
| PWM0 | OpenTitan | ✅ | ◐ | — | ✅ | no IRQ yet |
| DMA0 | PL330 | ✅ | ✅×2 | ✅ | ✅ | master socket |
| ADC0 | custom | ✅ | ✅ | ✅ | ✅ | base/IRQ TBD |
| TRNG0 | CryptoCell | ✅ | ✅ | ✅ | ✅ | |
| DMIC0 | PDM/CIC | ✅ | ✅ | ✅ | ✅ | |
| OTP0 | OpenTitan | ✅ | ✅ | — | ✅ | |
| CMU0 | OpenTitan clkmgr | ◐ | ◐ | — | ✅ | no real clock gating |
| PMU0 | OpenTitan pwrmgr | ◐ | ✅ | ✅(por) | ✅ | env signals required |
| QSPI0 | custom | ✅ | ✅ | ✅ | ✅ | |
| NOR flash | JEDEC | ✅ | — | — | ✅ | behind QSPI |
| RTC0 | PL031 | ✅ | ✅ | ✅ | ✅ | alarm |
| CLINT0 | RISC-V | ✅ | n/a | — | ✅ | MSIP/MTIP |
| PLIC0 | RISC-V | ✅ | n/a | — | ✅ | MEIP |
| bus_router | TLM | ✅ | — | — | ✅ | no DMI |
| memory | TLM | ✅ | — | — | ✅ | |
| ISP/VPU/NPU | accel | ⛯ | ⛯ | — | — | planned |

---

## 12. Scope & limitations / future work

Honest boundaries of the current model — important when judging fitness for a task.

- **Single-hart only.** `has_smp=false`; Bremen runs `hart_id=0`; CLINT/PLIC are
  modeled for one hart. **No multi-core.** SMP would require multiple ISS instances,
  per-hart CLINT contexts (msip/mtimecmp), and PLIC multi-context claim/complete.
- **No cache model → cache coherence is N/A.** Every access goes to memory via
  `b_transport`; no caches/L1/L2 exist. Coherence (RVWMO + a coherent interconnect /
  directory / CHI-ACE-like model) is **not present** and is a major future item.
- **No always-on power domain.** PMU/RTC are register models, not a real AON island.
- **Loosely-timed, not cycle-accurate.** Timing is functional (per-IP `sc_time`),
  so **hard-realtime / WCET guarantees cannot be validated here**; use for functional
  firmware/driver bring-up, not timing closure. (MIPS numbers are relative only.)
- **MMU off, M-mode focus.** Bremen supports Sv32+ upstream but `has_mmu=false` here.
- **No DMI** → simulation is slower than the cores' native speed.
- **GDB/RSP not wired**; debug is ISS-level introspection only.
- Legacy `mini_tlm` does not build (old timer API).

---

## 13. Extending the platform (add an IP)

1. Create `components/<ip>/{include,src,tests}`, a `CMakeLists.txt` with target
   `cdc::components::<ip>`, and register it in `components/CMakeLists.txt`.
2. Follow conventions: active-low `sc_in<bool> reset_n`, `sc_out<bool> irq_out` (if it
   interrupts), region-local 32-bit register access, validate address/length →
   `TLM_ADDRESS_ERROR_RESPONSE`.
3. Add a **self-checking** CTest (use `tests/support/tlm_probe.h`; exit non-zero on
   failure). Prefer a pure-C++ model class + a thin TLM wrapper (see `adc_tlm`, `rtc_tlm`).
4. Update `docs/peripheral_memory_map.md` (base + PLIC source) and integrate into the
   SoC top (bind socket + `reset_n` + IRQ to PLIC).

**Definition of done for a new software-visible IP**

| Item | Required output |
|---|---|
| Component model | Header/source with documented sockets, reset behavior, register window, and error responses. |
| Test | Self-checking component CTest that covers reset/defaults, normal R/W, error access, and IRQ if present. |
| Platform | CPU+bus+RAM+UART+IP platform or integration into an existing platform. |
| Firmware | Minimal firmware or test sequence that exercises the driver-visible behavior. |
| Documentation | README plus memory map / IRQ map update. |
| Review check | No address overlap, no duplicate PLIC source, no unbounded master access outside RAM. |

---

## 14. Roadmap: security/boot & software ecosystem

### Security & boot (RISC-V, not ARM TF-A directly) ⛯ Research
ARM Trusted Firmware (TF-A) is ARM-specific (EL3, PSCI, TrustZone). The **RISC-V
equivalents** that *do* apply:

| TF-A concept | RISC-V equivalent | Applicable to CDC-VP |
|---|---|---|
| BL31 / EL3 secure monitor | **OpenSBI** (M-mode runtime, SBI calls) | ✅ fits (M-mode today) |
| Multi-stage boot BL1/2/31 | BootROM(ZSBL) → FSBL → OpenSBI → U-Boot | ✅ matches BootROM+OTP |
| Secure / measured boot | BootROM + **OTP** signature verify | ✅ OTP+BootROM present/planned |
| TrustZone / secure world | **PMP** now; Keystone/CoVE/Smmtt later | ◐ needs core PMP enabled |
| PSCI (cpu on/off/power) | **SBI HSM** extension | ⛯ needs multi-hart first |

Near-term, applicable pieces: **OpenSBI** as the boot/runtime layer, **PMP** for
M-mode memory isolation, and **secure/measured boot** leveraging the existing OTP +
(planned) BootROM. Full TEE is out of scope for now.

### Software ecosystem prior-art ⛯ Research
- **Pulp-SDK / PMSIS** (PULP RISC-V): a useful **reference architecture** for a
  HAL/driver/runtime layer and for accelerator (cluster) offload — relevant to the
  ISP/VPU/NPU pipeline. **Not runnable on CDC-VP** (different cores/ISA-extensions/
  memory map); borrow structure, not code.
- **FreeRTOS** (first RTOS target), then evaluate **Zephyr** (Bremen upstream supports
  both) once CLINT tick + PLIC + a board port exist.

---

## 15. Provenance, licenses, versions

| Component | Source | License | Pinned |
|---|---|---|---|
| Bremen core | `agra-uni-bremen/riscv-vp` | **MIT** | commit `48b2f58…` |
| SystemC | Accellera | Apache-2.0 | 2.3.4 (`/opt/systemc-2.3.4`) |
| RISC-V toolchain | xpack `riscv-none-elf` GCC | — | host install |

CDC-VP's own source is **Apache-2.0** (`LICENSE`, `NOTICE`). The external Bremen core
(MIT) is not bundled (fetched into gitignored `third_party/`). All dependencies are
permissive — see [`THIRD_PARTY.md`](../THIRD_PARTY.md).

IP register models are **reference-design-style** (ARM PrimeCell/PL011/PL022/PL330/
SP805, CryptoCell; OpenTitan clkmgr/pwrmgr/OTP/I2C/PWM) — re-implemented as TLM
functional models, not vendor RTL. Build: C++17, SystemC TLM-2.0.

---

## 16. References

- RISC-V Privileged ISA spec (M/S/U, CLINT/PLIC, mtvec/mie/mstatus).
- OSCI/Accellera **TLM-2.0 LRM** (LT coding style, generic payload).
- Bremen **RISC-V VP**: `github.com/agra-uni-bremen/riscv-vp`.
- **OpenSBI**: `github.com/riscv-software-src/opensbi` (M-mode runtime / SBI).
- **PULP / Pulp-SDK**: `github.com/pulp-platform` (BSP/accelerator prior-art).
- Companion docs: `peripheral_memory_map.md`, `cpu_benchmark_results.md`, and the
  `*_diagram.svg/png` figures.
