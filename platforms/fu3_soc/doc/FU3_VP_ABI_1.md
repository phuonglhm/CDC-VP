# FU3_VP_ABI_1 — firmware baseline of the FU3 virtual platform

Status: **approved 2026-10-26** for Boot ROM / driver development on the VP
(proposal `plan/De_Xuat_Chot_Spec_Ban_Giao_FW_SW.md` with five review
amendments). It is a VP baseline, not a statement about RTL: every point
where RTL differs or is not confirmed is listed in §9 with its owner. It is
not the R1 release: the open requirements of §9/§10 remain open.

Machine-readable sources: `include/fu3_vp_abi.h` (C, shared by the model and
firmware), the IP register headers (`fu3_plic/registers.h`,
`fu3_crm/registers.h`, `fu3_syscsr/registers.h`, …), `src/fu3_iopad_map.h`
(generated from Common v2.1 IOPAD MAP).

## 1. Address map

Common Spec v2.1 MemoryMap (`fu3_bus::BusConfig::fu3()`, revision string in
`--version`). EDHOC_OSCORE, PWM0/1 and CAN are disabled (decode error) until R2.

## 2. Register interfaces kept as approved

| Block | Interface | Decision |
| --- | --- | --- |
| AP_PLIC | PRIO(id) = 4 × id, PENDING 0x1000, ENABLE 0x2000, THRESHOLD 0x3000, CLAIM/COMPLETE 0x3004; 31 sources, priority 5 bit; no MSIP | Q03-a (not RTL-confirmed) |
| AP_HBCTRL | VP ABI v1 "HRVP" (model header `fu3_hyperram/registers.h`) | Q07-a |
| CRM | CSR_CRM_fu3_v2 + **bits 25–29 of CLK_GATE_1/SW_RST_N_1 = UART3–7** (reset 1, `FU3_CRM_PERI_UART3..7`) | ABI_1 (#13) |
| CRM bit 13 | gate/reset of UART2 at 0x4409_0000 (by name) | Q02-c (#14) |
| SYSCSR | FU3_SYSCSR_v1; `syscsr_status` (0x70) reads the **live** pins | SYSCSR v1 (#19) |
| DMA0 | peripheral ID 1 = SPI0, 2 = UART1, 3 = UART2, 4 = SPI1; channel allocation of Common v2.1 | ABI_1 (#30) |

## 3. SYSCSR extension (0x4401_0100–0x4401_0123)

| Offset | Name | Access | Reset | Meaning |
| --- | --- | --- | --- | --- |
| 0x100 | VP_ABI_VERSION | RO | 0x0001_0000 | ABI 1.0 |
| 0x104 | BOOT_STATUS | RO | 0 while SYSCSR in reset | [0] BOOT, [1] MODE_0, [2] MODE_1, [3] TEST_MODE, [4] TEST_MODE_SEL captured at the release of the SYSCSR reset; [8] DEBUG_EN live (pad die 6); [31] STRAPS_VALID |
| 0x108 | RESET_CAUSE | R/W1C | POR: 0x1 | [0] POR, [1] global, [2] software, [3] watchdog, [4] debug (ndmreset). Cumulative until cleared; POR-only state (system, global, watchdog, debug and SYSCSR resets keep it); one event per reset |
| 0x10C | — | RAZ/WI | 0 | reserved |
| 0x110 | INTMAP_EN0 | RW | 0 | bit s enables INTMAP source s (0–31) |
| 0x114 | INTMAP_EN1 | RW | 0 | bits 0–1: sources 32–33; bits 2–31 reserved |
| 0x118 | SWIRQ_PENDING | RO | 0 | [0] CVA6 `ipi_i`, [1] Ibex `irq_software_i` (through INTMAP source 32) |
| 0x11C | SWIRQ_SET | W1S | reads 0 | sets pending bits |
| 0x120 | SWIRQ_CLEAR | W1C | reads 0 | clears pending bits |

Rules: byte enables select lanes, a zero-strobe write has no effect, reserved
bits read 0 and ignore writes. Debug (backdoor) reads have no side effect;
debug writes are not performed. The SYSCSR reset clears the INTMAP enables,
SWIRQ and the captured straps. Both CPUs may access these registers (the
MemoryMap gives SYSCSR to both; no firewall).

INTMAP source indices (`FU3_VP_INTMAP_SRC_*`): 0 WDT0, 1 CRM, 2 GPIO0_WAKE,
3 EDHOC, 4 AES, 5 RNG, 6 DMA0, 7 QSPI, 8–15 UART0–7, 16–19 SPI0–3, 20 CAN,
21–23 GPIOA/B/C, 24 I2C, 25 ADC, 26–27 PWM0/1, 28–29 TIMER2/3, 30 WDT1,
31 MAILBOX_IBEX, 32 Ibex software, 33 TIMER1. Groups and causes: Common v2.1
Interrupt Mapping (fast[0..14], external, software, timer).

## 4. Clock and reset profile

- Reset domains: POR (PRSTN, filter 100 ns, readiness 255 OSC cycles); system
  resets (software, global, watchdog WDT0/1, debug = DM ndmreset) start a new
  boot epoch (Ibex reboots from ROM, CVA6 held, staging reopened). Software,
  watchdog, global and POR resets include the DBG domain (DM, JTAG2AXI
  registers); ndmreset keeps it (Q05-e). Holding CVA6 in reset does not reset DBG.
  Watchdog requests reset the system through the CRM (HAS 6.5, §9 #16).
- Clock gates stop the IP itself (state kept). Reset assertion is immediate,
  release is synchronized to the destination clock: a release while gated
  waits for the clock plus two bus clock periods (CRM Spec V1.1 3.1.2).
- Clock: OSC 20 MHz at reset (CPU and bus). VP PLL profile: write PLL
  configuration, power up, wait lock (counter 404 OSC cycles), then select
  with the CRM 3.7.4 sequence (divider changed only with both selectors on
  the OSC); CPU 400 MHz / bus 200 MHz is the tested target. **RTL: the
  integrated CRM wrapper has no analog PLL** (§9 #15).

## 5. Boot contract

Ibex resets at 0x0000_0080 (ROM, `mtvec` 0x1 vectored), CVA6 at 0x0001_0000
(released by CRM FCLK_SW[0], which closes the boot staging window). Reference
sequence:

1. read VP_ABI_VERSION, BOOT_STATUS (use the captured straps), RESET_CAUSE (then W1C);
2. clock / pad / source initialisation;
3. stage both images and the debug blob (§6) while the staging rules allow it;
4. apply the final PMP (below), then clear `mseccfg.RLB`;
5. wait for DEBUG_EN = 0 (BOOT_STATUS[8]);
6. enable the INTMAP routes it needs, release CVA6, branch to 0x0005_0000.

Ibex PMP at reset (16 entries, `mseccfg` = RLB | MMWP; VP profile following
Ibex-Config #7–#9, encodings not RTL-confirmed):

| Entry | Region | Reset cfg | Final (ABI_1 reference) |
| --- | --- | --- | --- |
| 0 | ROM 0x0000_0000, 4 KiB NAPOT | L R X | unchanged |
| 1 | TOR base 0x0001_0000 | OFF | unchanged |
| 2 | CVA6_ISRAM to 0x0004_1000 (TOR) | L R W | **L, no access** |
| 3 | TOR base 0x0005_0000 | OFF | unchanged |
| 4 | IBEX_ISRAM to 0x0006_1000 (TOR) | L R W | **L R X, ending at 0x0006_0000** (ROM stub sets RX before entering the image) |
| 5 | DSRAM 256 KiB NAPOT | L R W | unchanged |
| 6 | QSPI XIP 16 MiB NAPOT | L R X | unchanged |
| 7 | EDHOC_OSCORE 64 KiB NAPOT | L R W | unchanged |
| 8 | TOR base 0x4000_0000 | OFF | unchanged |
| 9 | MMIO to 0x5011_0000 (TOR) | L R W | unchanged |
| 10 | HyperRAM 32 MiB NAPOT | L R W | unchanged |
| 11 | — | OFF | **Ibex debug partition 0x0006_0000, 4 KiB NAPOT: L R W during staging, then L R X** |
| 12–15 | — | OFF | OFF |

No region is ever RWX. Clearing RLB makes the locked entries final. Debug
code fetch is allowed by the bus only in Debug Mode. Verified by
`tests/fw/abi1_boot_tests.c` (`fu3_soc_abi1_boot`).

## 6. Debug

One Debug Module at 0x1A11_0000 (hartsel 0 = CVA6, 1 = Ibex), abstract
register access, no progbuf/SBA; JTAG2AXI at IR/DR level (no OpenOCD). The VP
also has a GDB server per CPU (`--gdb-cva6/--gdb-ibex`, README "GDB"): a VP
tool, not an RTL debug path; it shares the hart control with the DM, one
owner at a time. A hart held in reset is reported unavailable; a halt request
set meanwhile halts it on its first instruction.

Debug partitions (CVA6 0x0004_0000, Ibex 0x0006_0000; exception entry +8)
are written by the Boot ROM during staging from a **blob that is a build
input owned by the DBG/DM team**. The VP ships a placeholder
(`tests/fw/vp_debug_blob.S`, built as `vp_debug_blob.bin`: `j .` at +0 and
+8) that is **VP-only**: it works because the VP Debug Module serves abstract
commands outside the instruction stream; on RTL the hart must run the Debug
Module's own debug code.

## 7. Access profile

| Region | CVA6 / Ibex | DMA0 | DBG |
| --- | --- | --- | --- |
| ROM | Ibex RX only | denied | read; write denied |
| CVA6_ISRAM / IBEX_ISRAM | MemoryMap staging/handoff rules | denied | read/write (patching only when the hart is halted/reset: **not enforced**) |
| Debug partitions | fetch in Debug Mode only; written during staging | denied | read/write |
| DSRAM, HyperRAM | R/W, no execute | R/W | R/W |
| QSPI XIP | read; **Ibex may fetch, CVA6 may not** (CVA6-Config #54) | read | read |
| UART1/UART2 data register (offset 0) | as their CSR | R/W | as CSR |
| SPI0/SPI1 TXDR (0x0C) / RXDR (0x10) | as their CSR | TXDR write, RXDR read | as CSR |
| Other control CSRs (CRM, SYSCSR, PLIC, DM, …) | as their CSR | denied | as CSR |

Implemented as bus access grants (`fu3_bus::InitiatorConfig::grants_only`,
`no_execute`) in `platform_bus_config()`. A denied access answers
`TLM_GENERIC_ERROR_RESPONSE` (DMA: slave error).

## 8. Pads

Common v2.1 IOPAD MAP die pads 1–108 (`src/fu3_pinmux.*`). Each pad carries
the function its 2-bit select chooses (b00 at reset). Until the die pad ↔
SYSCSR `{left,bottom,right,top}_io_cfg` side/index mapping is delivered
(§9 #10), the selects are set by the host (`set_pad_select`), not by firmware
writes to `*_io_cfg`; pad electrical controls (IE/PU/PD/OD…) are registers
only. UART0 (console) is on dies 48/49 at b00.

## 9. VP / RTL differences and owners

| # | Item | VP baseline | RTL / spec status | Owner | Driver impact |
| --- | --- | --- | --- | --- | --- |
| 1 | INTMAP enable | SYSCSR 0x110/0x114 | Not in SYSCSR v1 | SYSCSR RTL | Without it Ibex gets no peripheral IRQ on RTL |
| 3 | CVA6 XIP execute | Denied (copy XIP → ISRAM) | Common v2.1 Booting says both CPUs use XIP; CVA6-Config #54 excludes it | CPU | XIP boot mode copies to ISRAM |
| 4 | CVA6 RVB subset | Not supported in the ISS; test flags `rv32imfc` | RVB = 1, subset not listed | CPU | Do not use Zb* |
| 6 | Ibex PMP encodings | §5 table | "verify in RTL" | CPU | ROM PMP code may need new values |
| 8 | RESET_CAUSE | SYSCSR 0x108, POR-only state | Not in RTL; CRM 0x58 is live request status only | SYSCSR/CRM RTL | Boot reason unavailable on RTL |
| 9 | DEBUG_EN read | BOOT_STATUS[8] | Not in SYSCSR v1 ("separate external input") | SYSCSR RTL | ROM wait loop |
| 10 | Pad select | Host `set_pad_select` | Die ↔ (side, index) mapping missing | Pad / SYSCSR | Pinmux driver incomplete |
| 12 | SWIRQ | SYSCSR 0x118–0x120 | Not in SYSCSR v1 | SYSCSR RTL | No IPI on RTL |
| 13 | UART3–7 gates | CRM bits 25–29 | Not in CSR v2 | CRM RTL | Gate/reset of UART3–7 |
| 15 | PLL | VP has a PLL, 400/200 MHz profile | CRM wrapper has no analog PLL | CRM | Clock init must have an OSC fallback |
| 16 | Reset requests | WDT/SW/debug requests reset through the CRM | CRM ROM has no handler (status + MRV IRQ only) | CRM | WDT does not reset on RTL today |
| 17 | DBG reset | DBG kept on ndmreset, independent of CVA6 hold | CRM 3.1.2: debug reset follows the FCLK path | CRM | Debugger access before handoff |
| 19 | Strap capture | BOOT_STATUS captured at SYSCSR reset release | SYSCSR v1 STATUS live; capture TBD | SYSCSR RTL | Read BOOT_STATUS, not STATUS |
| 21 | Debug blob | VP placeholder | Must be the DM's debug code | DBG | ROM embeds the DBG blob |
| — | DMA/DBG access profile | §7 enforced on the bus | No firewall defined | SoC | — |
| — | Halt-before-patch for DBG | not enforced | — | SoC | Debugger discipline |

## 10. Agreements still needed from FW/SW

- UART boot frame "8P1" does not name the parity: proposal 19200 **8E1**;
  baud divisor and tolerance at the OSC and after the clock switch.
- Image format and UART download protocol (header/version, endianness,
  lengths, load/entry, QSPI offset, checksum, ACK/retry/timeout); a checksum
  is not secure-boot authentication.
- Boot failure behaviour (bad/oversize image, timeout, PLL not locking,
  MODE = 00/01 unsupported: recovery wait with an error code, CVA6 not released).
- ROM layout (4 KiB, vectors at its start, stack/scratch, linker layout,
  compiler ABI per CPU).

## 11. Loading images in the VP (`vp_fu3_soc`)

The VP command line preloads images before power-on reset; this stands for
the mask ROM content, the board flash and, for tests, what a Boot ROM would
have staged. It is not part of the boot contract (§5) and does not replace a
Boot ROM. Rules (`src/image_loader.h`, README "Run"):

- `--rom FILE` is required (an ELF with a ROM segment, or a raw image at
  0x0); without ROM content the run exits 3, since Ibex resets at 0x0000_0080.
- Allowed: ROM, CVA6_ISRAM 0x0001_0000-0x0003_FFFF, IBEX_ISRAM
  0x0005_0000-0x0005_FFFF, DSRAM, QSPI NOR (`--flash FILE[@OFF]` or the XIP
  window). Refused: debug partitions (§6), HyperRAM, MMIO, unmapped, any range
  crossing a region boundary; a refused ELF writes nothing.
- Code placed in a CPU's region must match that CPU (ELF float ABI, RVE flag,
  `.riscv.attributes` arch string): Ibex RV32IMC + Zicsr/Zifencei/Zca/Zcb, soft
  float; CVA6 RV32IMFC + Zicsr/Zifencei/Zca/Zcf, soft or single float.
- Straps, DEBUG_EN (and its release time), pad levels, console input, ADC
  values and the RNG seed are set from the command line; the console verdict
  (`--expect-uart`/`--fail-uart`) gives exit 0/1, 2 on timeout.
