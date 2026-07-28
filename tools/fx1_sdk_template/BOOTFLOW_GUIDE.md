# VP_FX1 Firmware Handbook — SoC Quick Reference & ROM-Code Boot Contract

Audience: the firmware team developing on the VP_FX1 virtual chip — in
particular the ROM-code boot sequence
(`vp/doc/VP_FX1_SOC/ROMCode Boot Sequence.png`; the sequence is yours, the
VP hardware was changed to match it). §1 is the one-page SoC reference;
§2 onward is the **VP-specific boot contract**: where each hardware
touchpoint of the sequence lives, how it differs from real silicon, and how
to drive the virtual chip.

Hardware reference set:

- `sw/bsp/VP_FX1_SOC/include/soc/soc_memory_map.h`, `soc_irq_map.h` — the
  compile-time ABI (bases, offsets, IRQ ids). Authoritative.
- `vp/doc/VP_FX1_SOC/peripheral_memory_map.md` — full SoC map + IRQ spec.
- `sw/bsp/VP_FX1_SOC/regref/<ip>_tlm/README.md` — per-register/bit-field
  behaviour of every IP (`gpio_tlm`, `uart2_tlm`, `spi_tlm`, `flash_nor_tlm`,
  `wdt_tlm`, ...).
- §5 below — the VP CLI for every branch of the sequence.

## 1. SoC quick reference

| Property | Value |
|---|---|
| CPU | single hart RV32IMAC (Bremen riscv-vp ISS), M-mode only, no MMU/caches |
| ABI | `rv32imac / ilp32`, link base `0x80000000` (see `sw/bsp/VP_FX1_SOC/link/riscv.ld`) |
| RAM | 256 MiB @ `0x8000_0000`; firmware window = first 16 MiB |
| Timebase | CLINT `MTIME`/`MTIMECMP` are 64-bit **microsecond** counters (not cycles) |
| Timing model | loosely-timed TLM: register semantics and IRQ ordering are valid, cycle counts / WCET are not |
| Console | UART0 (PL011-style), polled HAL in `sw/bsp/VP_FX1_SOC/src/uart.c` |
| MMIO width | 32-bit LE words (`hal/mmio.h`) — **exception: SPI0/SPI1 registers are 16-bit** |

### Memory map (summary; `soc_memory_map.h` is authoritative)

| Base | Block | Notes |
|---|---|---|
| `0x0000_0000` | BOOTROM (64 KiB, RO) | ROM-code ELF entry must be `0x0` |
| `0x0400_0000` | IFLASH (4 MiB, RO, XIP) | ROM code jumps here when the strap is LOW |
| `0x0200_0000` | CLINT | `MSIP 0x0`, `MTIMECMP 0x4000`, `MTIME 0xBFF8` |
| `0x0C00_0000` | PLIC | `PRIORITY(id)=4*id`, `ENABLE 0x2000`, `THRESHOLD 0x200000`, `CLAIM 0x200004` |
| `0x1000_0000` | UART0 | console + boot-flow protocol channel |
| `0x1001_0000` | I2C0 | |
| `0x1002_0000` | SPI0 | PL022-style, 16-bit regs; vendor `SSPCSR @ +0x28`; 16 MiB NOR behind it (boot-flow) |
| `0x1003_0000` | TIMER0 | |
| `0x1004_0000` | WDT0 | |
| `0x1005_0000` | PWM0 | **no IRQ output** in current model |
| `0x1006_0000` | DMA0 | also bus master into RAM |
| `0x1007_0000` | TRNG0 | |
| `0x1008_0000` | CMU0 (clkmgr) | registers only — no real gating, no IRQ |
| `0x1009_0000` | PMU0 (pwrmgr) | see gotchas below |
| `0x100A_0000` | DMIC0 | |
| `0x100B_0000` | OTP0 | |
| `0x100C_0000` | QSPI0 | its own NOR flash behind it (separate from SPI0's) |
| `0x100D–100E_0000` | ISP0 / VPU0 | **RESERVED — no model, no IRQ** |
| `0x1020_0000` | NPU0 | SAURIA v4 wrapper: 64 KiB MMIO + RAM master, INT8 GEMM |
| `0x1010_0000`… | UART1, I2C1, SPI1, TIMER1, RTC0, ADC0, GPIO0 | instance-1 block, `+0x1_0000` apart; GPIO0 @ `0x1016_0000` (`VALUE 0x00` RO / `OUT 0x04` / `DIR 0x08`, 32-bit only, **no IRQ**), pin 1 = boot strap |
| `0x8000_0000` | RAM0 (256 MiB) | FW 16 MiB, then RAW_IN0/ISP_OUT0/VPU_OUT0/NPU_WGT0/NPU_WORK0 buffer windows |

### Interrupts (summary; `soc_irq_map.h` is authoritative)

- Local causes: `MSIP=3`, `MTIP=7`, `MEIP=11`. Enable via `mstatus.MIE` +
  `mie` bits, install `mtvec`, then for external IRQs use the PLIC
  claim/complete flow (read `CLAIM` → handle → write ID back to `CLAIM`).
- PLIC sources are 1-based (`CDC_PLIC_NUM_SOURCES = 31`):
  UART0=1, I2C0=2, SPI0=3, TIMER0=4, WDT0=5, PWM0=6(reserved), DMA0=7,
  DMA0_ABORT=8, TRNG0=9, CMU0=10(reserved), PMU0=11, DMIC0=12, OTP0=13,
  QSPI0=14, ISP0/VPU0=15–16(reserved), NPU0=17, UART1=18, I2C1=19, SPI1=20,
  TIMER1=21, RTC0=22(alarm), ADC0=23; 24–31 reserved (24 earmarked GPIO0).
- All modeled IRQ lines are **level-sensitive** into the PLIC.

### SoC-wide gotchas

- **PMU (0x1009_0000):** the SoC top drives its power-up handshake at boot,
  so firmware sees it already ACTIVE. Its 6 wakeup and 2 reset-request inputs
  are tied to 0 — a committed low-power entry has no wired wakeup source.
  Exercise fall-through / abort / `INTR_TEST` flows instead of a full
  sleep-wake round trip. A plain wakeup does **not** set `INTR_STATE`.
- **PWM0, CMU0 and GPIO0 have no IRQ** despite reserved PLIC slots. Poll.
- **NPU0 IRQ17 is level-sensitive.** Clear `NPU_IRQ_STATUS` first, then
  complete the PLIC claim. See `regs/soc_regs_npu_v4.h` and the NPU regref.
- **BOOTROM/IFLASH are read-only:** functional stores fail with a bus error;
  only the VP's image loaders (debug/backdoor writes) can fill them.
- **UART HAL is polled**; UART0 RX/TX interrupts exist in the model if a
  driver wants them (see `regref/uart2_tlm`).
- The VP process ends when simulation time runs out (`--sim-ms N`).

## 2. Boot flow: hardware summary (what the diagram maps to)

| Diagram element | VP hardware | ABI symbols |
|---|---|---|
| ROM code at reset | BOOTROM 64 KiB @ `0x0`, read-only; CPU enters at ELF entry, so **link the ROM code with entry `0x0`** | `CDC_BOOTROM_BASE/_SIZE` |
| "Port A pin 1" boot strap | GPIO0 pin 1 (input at reset). Strap driven by the VP CLI: `--boot-pin low|high` (default low) | `CDC_GPIO0_BASE`, `CDC_GPIO_VALUE/OUT/DIR`, `CDC_GPIO_BOOT_PIN` |
| Internal flash + "Jump to App" | IFLASH 4 MiB @ `0x0400_0000`, read-only, executable (XIP). Preloaded from a raw binary: `--int-flash app.bin` | `CDC_IFLASH_BASE/_SIZE` |
| USART0 ↔ PC host tool | UART0 (PL011-style). The "PC host tool" connects over TCP to the VP's UART bridge | `CDC_UART0_BASE` |
| SPI0 master ↔ external NOR | SPI0 (PL022-style) + dedicated 16 MiB NOR flash behind it (separate device from the QSPI0 flash). Image: `--spi-flash nor.bin` | `CDC_SPI0_BASE`, `CDC_SPI_CSR` |
| WDT / "SysTick" | `wdt_tlm` @ `CDC_WDT0_BASE`; system tick = CLINT `MTIME` (**microseconds**, not cycles) | `CDC_WDT0_BASE`, `CDC_CLINT_*` |

## 3. VP-specific access notes per step

(Not a tutorial — only the points where the VP's behaviour is a contract
you must know: access widths, vendor registers, what is and isn't modeled.)

### 3.1 Read the boot strap (GPIO0)

`DIR` resets to all-inputs, so no configuration is strictly needed; the
diagram's "configure pin 1 as input" step is a `DIR` bit clear at most.
32-bit accesses only.

```c
unsigned strap = (mmio_read32(CDC_GPIO0_BASE + CDC_GPIO_VALUE)
                  >> CDC_GPIO_BOOT_PIN) & 1u;
if (strap == 0u) {            /* LOW -> internal flash app */
    ((void (*)(void))CDC_IFLASH_BASE)();
}
```

There is no GPIO interrupt in this revision — poll `VALUE`.

### 3.2 Jump to App

The app is a raw binary executing in place at `CDC_IFLASH_BASE`; its first
bytes must be code (`_start`). IFLASH is read-only: the app can keep `.text`
and `.rodata` there but must place stack/data in RAM (a store into IFLASH
fails with a bus error — that is intentional).

### 3.3 USART0 probe (19200 8N1)

Baud/format writes (`UARTIBRD/UARTFBRD/UARTLCR_H`) are accepted but **not
modeled** — there is no line rate on the VP. Functional TX/RX:

```c
/* TX one byte */
while (mmio_read32(CDC_UART0_BASE + 0x18) & (1u << 5)) {}  /* FR.TXFF */
mmio_write32(CDC_UART0_BASE + 0x00, byte);                  /* DR      */

/* RX poll with your own timeout */
if (!(mmio_read32(CDC_UART0_BASE + 0x18) & (1u << 4)))      /* FR.RXFE */
    byte = mmio_read32(CDC_UART0_BASE + 0x00) & 0xFF;
```

RX FIFO is 16 deep; interrupts (RXRIS/RTRIS) exist if you prefer an
IRQ-driven loop — see `regref/uart2_tlm`.

### 3.4 SPI0 NOR read (CMD 0x03)

SPI0 registers are 16-bit — use 16-bit loads/stores (`MMIO16`). The
chip-select is software-controlled through the vendor register `SSPCSR`
(`CDC_SPI_CSR = +0x28`, bit0: 1 = assert). A NOR command spans many frames
and **terminates on CS deassert**; the flash decodes nothing while deselected.

```c
MMIO16(CDC_SPI0_BASE + 0x00) = 0x0007;   /* CR0: 8-bit frames        */
MMIO16(CDC_SPI0_BASE + 0x10) = 2;        /* CPSR                     */
MMIO16(CDC_SPI0_BASE + 0x04) = 1u << 1;  /* CR1: SSE enable          */

MMIO16(CDC_SPI0_BASE + CDC_SPI_CSR) = 1; /* CS assert                */
spi_xfer(0x03);                          /* NOR READ                 */
spi_xfer(a23_16); spi_xfer(a15_8); spi_xfer(a7_0);
for (i = 0; i < n; ++i) buf[i] = spi_xfer(0xFF);  /* clock data out  */
MMIO16(CDC_SPI0_BASE + CDC_SPI_CSR) = 0; /* CS deassert = command end */
```

where `spi_xfer` writes `DR` (wait `SR.TNF`, bit1), then reads `DR` back
(wait `SR.RNE`, bit2) — every TX frame produces exactly one RX frame
(full duplex). Only CMD `0x03` is decoded on this face; anything else
returns `0xFF` until CS toggles.

## 4. Building the ROM code and the app

Toolchain: xPack `riscv-none-elf-` (`rv32imac_zicsr / ilp32`), see repo README.

- **ROM code**: custom linker script — `.text`/`.rodata` in ROM at `0x0`
  (entry `0x0`, `_start` first), stack in RAM (`0x8000_xxxx`). No writable
  `.data`/`.bss` in ROM; if you need them, copy to RAM in `_start`.
- **App**: linked at `0x0400_0000`, converted with `objcopy -O binary`;
  first instruction at the image start. Same read-only rules.

Working linker scripts, startup, Makefile: `romcode_ref/`.

## 5. Running every branch of the diagram

```bash
RUN=sw/bootloader/test/VP_FX1_SOC/run_vp.sh

# strap LOW -> jump to app in IFLASH
$RUN bootrom.elf --int-flash app.bin --boot-pin low

# strap HIGH + live PC host tool over TCP (sim blocks until the tool connects)
$RUN bootrom.elf --boot-pin high --uart0-socket 5577 --uart0-wait --sim-ms 60000
#   host tool = any TCP client on 127.0.0.1:5577; it receives everything the
#   firmware transmits and its bytes land in UART0 RX.

# strap HIGH + canned UART response (deterministic, for CI)
$RUN bootrom.elf --boot-pin high --uart0-rx-file resp.bin --sim-ms 200

# strap HIGH + NOR image behind SPI0
$RUN bootrom.elf --boot-pin high --spi-flash nor_image.bin --sim-ms 300

# strap HIGH, nobody answers (probe loop must terminate on its own!)
$RUN bootrom.elf --boot-pin high --sim-ms 400
```

Simulation time races wall clock: interactive host-tool sessions need
`--uart0-wait` and/or a generous `--sim-ms`.

## 6. What the VP does NOT solve for you (design these in firmware)

1. **Probe-loop exit.** The diagram loops UART→SPI→repeat forever. If System
   Init arms the WDT, an unanswered probe loop trips it. Bound the loop
   (retry limit) or service the WDT inside it.
2. **"Received response" must be a protocol decision, not a wire decision.**
   An absent/erased NOR still shifts out `0xFF`; an idle UART line has no
   modeled electrical state. Define a valid image header (magic + size +
   entry + checksum) and validate it before jumping — for the IFLASH app too.
3. **UART0 is console AND protocol channel.** The TCP bridge forwards every
   TX byte to the host tool, including any log prints. Either keep UART0
   silent during protocol phases (log on UART1) or make the host protocol
   framing tolerate interleaved text.
4. **"Disable unused IPs" is cosmetic.** The clock manager has no real
   gating; write the code for silicon, but do not expect the VP to verify it.
5. **No timing.** 19200 8N1 is accepted but not enforced; never derive
   protocol correctness from timing. `MTIME` counts microseconds.

## 7. Known-good sample (platform sanity check)

`sw/bootloader/sources/VP_FX1_SOC/romcode_ref/` is the VP team's E2E test
firmware: it exercises every branch of the sequence (strap read, IFLASH
jump, UART probe, SPI0 CMD-0x03) and is known to pass on the delivered
binary. Use it to (a) verify your environment in five minutes, (b) see the
exact register sequences and linker setup the VP was validated against, or
(c) ignore it entirely. Delivered once; the directory is firmware-owned and
SDK re-packs never touch it.

## 8. Ground rules (never violate these)

1. **Treat `vp/` and `sw/bsp/` as read-only.** They are regenerated on every
   SDK delivery; local edits get clobbered and create ABI drift. Suspected VP
   bugs go back to the VP team with a repro ELF (and `vp/VERSION`), not a
   local patch. If headers/docs and observed VP behaviour disagree, **the VP
   binary wins**.
2. **Never hard-code addresses or IRQ numbers** — use `soc_memory_map.h` /
   `soc_irq_map.h` macros.
3. **Never change `-march`/`-mabi`.** Pinned to `rv32imac / ilp32` by the CPU
   model; a mismatch links fine and then crashes silently on the VP.
4. **MMIO is 32-bit little-endian word access** (`hal/mmio.h`) — except
   SPI0/SPI1, whose registers are 16-bit (use 16-bit accesses there).
5. Deliverables live under `sw/bootloader/sources/VP_FX1_SOC/` and
   `sw/drivers/sources/VP_FX1_SOC/<name>/`; build outputs go to the matching
   `build/VP_FX1_SOC/` — keep the `sources/ → build/` split, never commit
   build outputs into `sources/`.
6. This file is generated from the CDC-VP SDK template; propose changes
   there, not here.
