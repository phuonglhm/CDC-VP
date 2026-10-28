# FU3 CPU configuration contract

## Sources and scope

Baseline: FTEL_FU3_Common_Spec v2.1 (CVA6-Config, Ibex-Config, MemoryMap,
Interrupt Mapping, Booting; revision 18/10/2026) and HAS v2.1, with the
decisions Q01/Q02 of the FU3 VP plan (`T02_De_Xuat_Q01_Q08.md`). Common v2.1
supersedes HAS where they differ (CVA6 has no L1 cache; RVF = 1, RVD = 0).
CPU0 is CVA6 (application CPU, PLIC hart 0) and CPU1 is Ibex (boot and
system-control CPU, hart 1). The source documents are private.

Both CPUs are the FU3 fork of the Bremen RV32 ISS (`cpu_models/fu3_iss`) behind
`cdc::cpu::cva6_wrapper`. The VP models instruction-set behaviour, privilege,
traps, interrupts, PMP and bus traffic. It does not model pipelines, caches,
branch prediction, lockstep, ECC or the timing of either core: instruction
timing is a VP model (Q01-g), not silicon timing.

## CPU0: CVA6 (`fu3_cpu/cva6_config.h`)

| Item | Value | Source |
| --- | --- | --- |
| Reset PC | 0x0001_0000 (CVA6_ISRAM) | CVA6-Config #59 boot_addr_i |
| Reset | Held after construction; in `fu3_soc` released by CRM FCLK_SW[0] after the staging window closes | Booting, CRM V1.1, Q02-b |
| Hart ID | 0 | Q01-c, PLIC target 0 |
| ISA | RV32IMFC, M-mode only (`misa` without A, D, S, U, N) | CVA6-Config #2-#12, Q01-a |
| Bitmanip | **not provided**: RVB = 1 is required but its subset is undecided and the ISS has no Zb* | CVA6-Config #8, Q01-a (CHỜ) |
| FP | F with fcsr; mstatus.FS gates FP instructions (Off at reset) | RVF = 1 |
| PMP | 8 entries, TOR/NA4/NAPOT, reset all OFF; only locked entries restrict M-mode | #42/#43; reset value ASSUMPTION |
| mtvec | Direct or vectored (WARL), reset 0 (ISS default; RTL value not in the spec) | — |
| Misaligned load/store | Misaligned exception (cause 4/6) | ASSUMPTION: CVA6 has no HW misaligned support |
| Interrupts | cause 3 ipi_i, 7 time_irq_i, 11 irq_i[0]; irq_i[1] tied low | Interrupt Mapping |
| Debug | Debug Mode (spec 0.13.2, execution based, see below); HaltAddress 0x0004_0000, ExceptionAddress 0x0004_0008 | #44-#46, Q04-b |
| Triggers | 2, mcontrol exact execute-address match (M); **type/fields provisional** until checked against the CVA6 revision | #47, Q04-e |
| Clock | CRM FCLK: 20 MHz OSC (50 ns) after reset, 400 MHz (2.5 ns) once software selects the PLL configured for it | HAS v2.1 §2, Q01-g |

## CPU1: Ibex (`fu3_cpu/ibex_config.h`)

| Item | Value | Source |
| --- | --- | --- |
| Reset PC | 0x0000_0080 (boot_addr 0 + 0x80) | Booting #22, MemoryMap #1, Q01-b |
| mtvec | Reset 0x0000_0001; MODE fixed vectored, BASE 256-byte aligned | Q01-b |
| Hart ID | 1 | Q01-c |
| ISA | RV32IMC + Zcb, U-mode (`misa` without A, F, D, S, N) | Ibex-Config #1, #13-#15, Q01-a |
| Zcb | c.lbu, c.lhu, c.lh, c.sb, c.sh, c.zext.b, c.not, c.mul; c.sext.b/h, c.zext.h illegal (no Zbb) | Q01-e |
| PMP/ePMP | 16 regions, Smepmp; mseccfg reset RLB = 1, MMWP = 1, MML = 0 | #2-#4, #9, Q01-d |
| PMP reset entries | **VP proposal, not signed off**: ROM RX, CVA6/Ibex ISRAM staging RW, DSRAM RW, XIP RX, EDHOC/MMIO/HyperRAM RW, all locked | #7/#8, Q01-d |
| Misaligned load/store | Split into aligned words (no trap) | Ibex LSU |
| Interrupts | cause 3/7/11 and irq_fast_i[14:0] as cause 16-30; priority fast[0..14] > MEI > MSI > MTI; irq_nm_i tied low | Interrupt Mapping (Ibex, proposed routes) |
| Debug | Debug Mode (spec 0.13.2, execution based, see below); DmHaltAddr 0x0006_0000, DmExceptionAddr 0x0006_0008; Debug Mode accesses to DmBaseAddr 0x1A11_0000 + 0xFFF bypass PMP; dcsr.ebreaku | #33-#36, Q04-b |
| Triggers | 4, mcontrol exact execute-address match (M/U), load/store 0 (Ibex upstream) | Q04-e |
| Clock | Same CRM FCLK as CVA6; released by the CRM startup 1.2 us after PRSTN | Q01-g, CRM V1.1 4.1 |

## Debug Mode (both CPUs)

- Entry: debug request (level, dcsr.cause 3, before the next instruction),
  EBREAK with dcsr.ebreakm/ebreaku (cause 1, dpc = the ebreak), execute
  trigger (cause 2, before the instruction), single step (cause 4, dcsr.step;
  interrupts during a step only with dcsr.stepie). The hart runs in M-mode
  from the HaltAddress; dcsr.prv keeps the interrupted privilege.
- In Debug Mode: an exception jumps to the ExceptionAddress and updates no
  CSR; interrupts are not taken (they stay pending); WFI is a NOP; EBREAK
  jumps to the HaltAddress. DRET returns to dpc in dcsr.prv.
- dcsr/dpc/dscratch0-1 and DRET exist only in Debug Mode (illegal
  instruction otherwise). tdata1/tdata2 have dmode = 1: writable only in
  Debug Mode. dcsr.stopcount/stoptime are 0: counters and timers run on.
- Fetches and data accesses in Debug Mode carry `fu3_bus::Access::debug`:
  the bus serves the ISRAM debug partition only to them (an M-mode fetch of
  the HaltAddress outside Debug Mode is an instruction access fault).
- Reset leaves Debug Mode and clears dcsr/dpc/triggers; the request is a
  level and survives reset: held across reset, the hart halts before its
  first instruction with dpc = reset PC.
- The VP provides no debug code: the debug partition holds whatever the image
  loader or the Debug Module puts there (T18/T20).

## Not modelled / open

- CVA6 bitmanip (Q01-a), NMI (no source on FU3).
- Load/store triggers, icount, tcontrol, trigger chaining (Q04-e).
- Ibex PMP reset table (VP proposal) gives the debug partition R | W, locked,
  under MMWP: a halt before the ROM opens it cannot fetch the debug code and
  the hart loops between the debug entries (`test_fu3_cpu_debug ibex`). To
  be decided with the PMP reset table (Q01-d, T06 report).
- Abstract register access writes the architectural registers directly while
  the debug code runs: debug code that saves and restores GPRs overwrites
  such a write at DRET. The contents of the debug partition with a Debug
  Module attached are decided in T18.
- Ibex cpuctrlsts/secureseed and SecureIbex effects (dummy instructions, data
  independent timing, lockstep, ECC): Q01-f requires a CSR/feature table with
  explicit gaps (T05 report); none is implemented yet.
- CVA6 XIP execution and entry selection: Q02-e (CHỜ).

## Integration contract

- One AXI master per CPU: bind `data_bus()` to the FU3 bus initiator `CVA6` or
  `IBEX` (`has_unified_bus()`); every transaction carries `fu3_bus::Access`.
- Fetches are aligned 4-byte words, split when the PC is 2-byte aligned (RVC);
  PMP checks the instruction bytes themselves.
- Bus latency is taken into the ISS quantum keeper once; there is no extra
  wrapper delay.
- `set_irq(cause, level)` accepts only the causes listed above and throws on
  any other, so a mis-wired interrupt fails at integration.
- `set_reset(true)` stops the core after the current instruction (waking it
  from WFI, even a WFI fetched while the request arrives). The architectural
  reset runs in the CPU thread before the next instruction after release: an
  instruction that was waiting on the bus when reset came does not commit into
  the state after reset; execution restarts at the reset PC.
- Interrupt input levels are kept apart from `mip` and re-applied after every
  reset, so a line held high across reset is pending after release.
- `set_clock_period(period)` sets the core clock (the platform follows CRM
  `cpu_clock_hz()`); it takes effect before the next instruction, cycle counts
  (mcycle) are unaffected, and SC_ZERO_TIME stops the core until a non-zero
  period is set. An instruction takes its cycle cost times the period plus the
  bus latency of its accesses, counted once.
- The platform sets the TLM global quantum before creating the CPUs
  (`fu3_soc_options::quantum`, default 1 us): a CPU sees an input change at
  most one quantum late; WFI and reset wait on events.
- Debug control (Debug Module `fu3_dbg::DebugHart`, GDB stub):
  `set_debug_request(level)` drives debug_req_i; `debug_halted()` is true in
  Debug Mode until `debug_resume()`, which the CPU thread applies as a DRET
  before the next instruction (one instruction with dcsr.step);
  `debug_access_register(regno, write, value)` while halted: 0x0000-0x0FFF
  CSR, 0x1000-0x101F GPR (x0 stays 0), 0x1020-0x103F FPR (CVA6 only); false
  when not halted or the register does not exist. The platform adapter is
  `platforms/fu3_soc/src/cpu_debug_hart.h` (`fu3_soc_top::debug_harts()`).
