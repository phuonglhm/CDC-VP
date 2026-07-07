# AI CONTEXT — FX1 firmware SDK (VP_FX1_SOC)

> Load this file first when assisting in this repository. It defines what the
> project is, the fixed hardware ABI, the rules that must not be broken, and
> where the authoritative references live.

## What this project is

This repo is the **firmware/driver workspace for the VP_FX1 SoC**. The "chip"
is a SystemC/TLM virtual platform shipped as a prebuilt binary
(`vp/bin/VP_FX1_SOC/vp_fx1_full_soc`). Firmware engineers write bare-metal
RISC-V drivers and a bootloader against its **fixed register/IRQ ABI**, and run
them on the VP exactly as they would on silicon.

The VP is built from a separate repository (**CDC-VP**) and delivered here by
its packaging script (`tools/pack_fx1_sdk.sh` in CDC-VP). `vp/VERSION` records
the exact CDC-VP commit this delivery was built from.

## Hard rules (never violate these)

1. **Treat `vp/` and `sw/bsp/` as read-only.** They are regenerated on every
   SDK delivery; local edits will be clobbered and create ABI drift. Suspected
   VP bugs go back to the VP team with a repro ELF, not a local patch.
2. **Never hard-code addresses or IRQ numbers.** Always use the macros from
   `soc_memory_map.h` and `soc_irq_map.h`.
3. **Never change `-march`/`-mabi`.** The ABI is pinned to `rv32imac / ilp32`
   by the CPU model; a mismatch links fine and then crashes silently on the VP.
4. **All MMIO is 32-bit little-endian word access** via `hal/mmio.h`
   (`mmio_read32` / `mmio_write32`). No byte/halfword register access.
5. New firmware goes under `sw/drivers/sources/VP_FX1_SOC/<name>/` or
   `sw/bootloader/sources/VP_FX1_SOC/`; build outputs go to the matching
   `build/VP_FX1_SOC/` directory. Follow the `uart_hello` template.

## Hardware quick facts

| Property | Value |
|---|---|
| CPU | single hart RV32IMAC (Bremen riscv-vp ISS), M-mode only, no MMU/caches |
| ABI | `rv32imac / ilp32`, link base `0x80000000` (see `sw/bsp/VP_FX1_SOC/link/riscv.ld`) |
| Entry | `_start` in `startup_riscv.S`: sets stack, clears BSS, calls `main` |
| RAM | 256 MiB @ `0x8000_0000`; firmware window = first 16 MiB |
| Timebase | CLINT `MTIME`/`MTIMECMP` are 64-bit **microsecond** counters |
| Timing model | loosely-timed TLM: register semantics and IRQ ordering are valid, cycle counts / WCET are not |
| Console | UART0 (PL011-style) via polled HAL `sw/bsp/VP_FX1_SOC/src/uart.c` |

## Memory map (from `soc_memory_map.h` — authoritative)

| Base | Block | Notes |
|---|---|---|
| `0x0000_0000` | BOOTROM (64 KiB, RO) | first-stage boot ROM; ROM-code ELF entry must be `0x0` (see "ROM-code boot flow") |
| `0x0400_0000` | IFLASH (4 MiB, RO, XIP) | internal code flash; the ROM code jumps here when the boot strap is LOW |
| `0x0200_0000` | CLINT | `MSIP 0x0`, `MTIMECMP 0x4000`, `MTIME 0xBFF8` |
| `0x0C00_0000` | PLIC | `PRIORITY(id)=4*id`, `ENABLE 0x2000`, `THRESHOLD 0x200000`, `CLAIM 0x200004` |
| `0x1000_0000` | UART0 | console |
| `0x1001_0000` | I2C0 | |
| `0x1002_0000` | SPI0 | PL022-style; vendor `SSPCSR @ +0x28` (bit0 = CS assert); dedicated 16 MiB NOR behind it for the boot-flow SPI download |
| `0x1003_0000` | TIMER0 | |
| `0x1004_0000` | WDT0 | |
| `0x1005_0000` | PWM0 | **no IRQ output** in current model |
| `0x1006_0000` | DMA0 | also bus master into RAM |
| `0x1007_0000` | TRNG0 | |
| `0x1008_0000` | CMU0 (clkmgr) | registers only — no real gating, no IRQ |
| `0x1009_0000` | PMU0 (pwrmgr) | see gotchas below |
| `0x100A_0000` | DMIC0 | |
| `0x100B_0000` | OTP0 | |
| `0x100C_0000` | QSPI0 | NOR flash behind it |
| `0x100D–100F_0000` | ISP0 / VPU0 / NPU0 | **RESERVED — no model, no IRQ; do not write drivers for these** |
| `0x1010_0000`… | UART1, I2C1, SPI1, TIMER1, RTC0, ADC0, GPIO0 | instance-1 block, `+0x1_0000` apart; GPIO0 @ `0x1016_0000` (`VALUE 0x00` RO / `OUT 0x04` / `DIR 0x08`, 32-bit only, **no IRQ**), pin 1 = boot strap |
| `0x8000_0000` | RAM0 (256 MiB) | FW 16 MiB, then RAW_IN0/ISP_OUT0/VPU_OUT0/NPU_WGT0/NPU_WORK0 buffer windows |

## Interrupts (from `soc_irq_map.h` — authoritative)

- Local causes: `MSIP=3`, `MTIP=7`, `MEIP=11`. Enable via `mstatus.MIE` +
  `mie` bits, install `mtvec`, then for external IRQs use the PLIC
  claim/complete flow (read `CLAIM` → handle → write ID back to `CLAIM`).
- PLIC sources are 1-based (`CDC_PLIC_NUM_SOURCES = 31`):
  UART0=1, I2C0=2, SPI0=3, TIMER0=4, WDT0=5, PWM0=6(reserved), DMA0=7,
  DMA0_ABORT=8, TRNG0=9, CMU0=10(reserved), PMU0=11, DMIC0=12, OTP0=13,
  QSPI0=14, ISP0/VPU0/NPU0=15–17(reserved), UART1=18, I2C1=19, SPI1=20,
  TIMER1=21, RTC0=22(alarm), ADC0=23; 24–31 reserved.
- All modeled IRQ lines are **level-sensitive** into the PLIC.

## ROM-code boot flow (bootloader work targets this)

The VP implements the agreed ROM-code boot sequence
(`vp/doc/VP_FX1_SOC/ROMCode Boot Sequence.png`):

1. Reset → CPU starts in BOOTROM @ `0x0`. Build the ROM code with **entry
   `0x0`** (custom linker script: text in ROM, stack in RAM) and pass it with
   `--fw bootrom.elf`.
2. ROM code reads the boot strap **GPIO0 pin 1** (`CDC_GPIO_BOOT_PIN`):
   - **LOW** (default / `--boot-pin low`) → jump to the application in IFLASH
     @ `0x0400_0000` (raw image preloaded with `--int-flash app.bin`).
   - **HIGH** (`--boot-pin high`) → download probe loop:
     a. request over **UART0**; the "PC host tool" is a TCP client on the VP's
        bridge (`--uart0-socket <port>`, add `--uart0-wait` to block the sim
        until the tool connects) or a canned byte file (`--uart0-rx-file`).
     b. else NOR READ (`0x03`) over **SPI0**: assert CS via `SSPCSR` bit0,
        shift `0x03` + 3 address bytes, then dummy frames clock data out;
        deassert CS to end the command. Flash image: `--spi-flash <bin>`
        (independent from the QSPI0 flash).
3. Firmware-side duties the VP does NOT solve: bound the probe loop (WDT!),
   validate images (magic/size/entry/checksum header) before jumping, and
   frame/verify the UART protocol (no line-rate modeling — 19200 8N1 is
   cosmetic; the UART bridge also forwards ROM log prints to the host tool,
   so the protocol must tolerate interleaved text).

## Build & run

```bash
export PATH=/opt/toolchains/xpack-riscv-none-elf-gcc-15.2.0-1/bin:$PATH  # riscv-none-elf-

make -C sw/drivers/sources/VP_FX1_SOC/uart_hello        # build a driver
sw/bootloader/test/VP_FX1_SOC/run_vp.sh                  # run default uart_hello.elf
sw/bootloader/test/VP_FX1_SOC/run_vp.sh path/to/app.elf  # run a specific ELF
sw/bootloader/test/VP_FX1_SOC/run_vp.sh app.elf --sim-ms 20
sw/bootloader/test/VP_FX1_SOC/run_vp.sh --no-fw          # SoC banner only

# ROM-code boot flow (extra args pass through to the VP):
sw/bootloader/test/VP_FX1_SOC/run_vp.sh bootrom.elf --int-flash app.bin --boot-pin low
sw/bootloader/test/VP_FX1_SOC/run_vp.sh bootrom.elf --boot-pin high \
    --uart0-socket 5577 --uart0-wait --sim-ms 60000    # then connect the host tool
sw/bootloader/test/VP_FX1_SOC/run_vp.sh bootrom.elf --boot-pin high \
    --spi-flash image.bin --sim-ms 300
```

Expected smoke-test output: `VP_FX1 SDK: UART console up` + `driver template OK`.
No SystemC install is needed (bundled `libsystemc.so*`, rpath `$ORIGIN`); the
host only needs the xPack `riscv-none-elf-` bare-metal toolchain.

A driver Makefile sets `BSP := ../../../../bsp/VP_FX1_SOC` and includes
`bsp.mk`, which provides `CC/CFLAGS/LDFLAGS` and `BSP_SRCS`
(`startup_riscv.S` + `uart.c`). Copy `uart_hello/` as the starting point.

## Where to find register details

1. **Compile against** `sw/bsp/VP_FX1_SOC/include/soc/regs/soc_regs_<ip>.h` —
   clean macros (`CDC_<IP>_<REG>`), machine-verified against the VP models at
   pack time. Shipped so far: `timer`, `i2c`, `dma` (UART is covered by the HAL).
2. **Read** `sw/bsp/VP_FX1_SOC/regref/<ip>_tlm/README.md` — per-IP register /
   bit-field / behaviour documentation for every modeled IP. Documentation
   only; never `#include` from regref.
3. SoC-level docs in `vp/doc/VP_FX1_SOC/`: `peripheral_memory_map.md`
   (full map + IRQ table) and `interrupt_modeling_policy.md`.
4. For IPs without a clean header yet (SPI, QSPI, WDT, TRNG, RTC, ADC, PWM,
   DMIC, OTP, PMU, CMU): read the regref doc, then define offsets locally in
   the driver or request a `soc_regs_<ip>.h` from the VP team.

## Known gotchas

- **PMU (0x1009_0000):** the SoC top drives its power-up handshake at boot, so
  firmware sees it already ACTIVE. Its 6 wakeup and 2 reset-request inputs are
  tied to 0 in this SoC — a committed low-power entry has no wired wakeup
  source. Exercise fall-through / abort / `INTR_TEST` flows instead of a full
  sleep-wake round trip. A plain wakeup does **not** set `INTR_STATE`.
- **PWM0, CMU0 and GPIO0 have no IRQ** despite having PLIC slots reserved
  (GPIO0's is source 24). Poll `GPIO VALUE` for pin changes.
- **BOOTROM/IFLASH are read-only**: functional stores fail with a bus error;
  only the VP's image loaders (debug/backdoor writes) can fill them.
- **UART HAL is polled**, not interrupt-driven; UART0 RX/TX interrupts exist in
  the model if a driver wants them (see `regref/uart2_tlm`).
- `MTIME` counts microseconds, not cycles — timer math must not assume a
  CPU-frequency timebase.
- The VP process ends when simulation time runs out; use `--sim-ms N` to
  extend runs.

## Repo etiquette for AI assistants

- Deliverables live under `sw/drivers/` and `sw/bootloader/` only.
- Keep the `sources/ → build/` split; never commit build outputs from other
  trees into `sources/`.
- If `soc_memory_map.h` and observed VP behaviour disagree, the VP binary wins;
  report the mismatch (with the failing access + `vp/VERSION`) instead of
  editing headers.
- This file is generated from the CDC-VP SDK template; propose changes there,
  not here.
