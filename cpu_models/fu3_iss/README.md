# fu3_iss - FU3 fork of the Bremen RV32 ISS

The CVA6/Ibex wrapper (`cpu_models/cva6_vp`) runs this fork. It is built from
`third_party/riscv-vp` (upstream commit `48b2f5877b2368cc466fb0da155db349e676c0b0`,
MIT, see `LICENSE.riscv-vp`), which stays untouched and gitignored.

Only the files FU3 changes are copied here:

```text
src/core/rv32/iss.h, iss.cpp, csr.h, mem_if.h, syscall_if.h
src/core/common/instr.h, instr.cpp
```

Everything else (`trap.h`, `fp.h`, `irq_if.h`, `util/`, vendored softfloat)
is still read from the upstream checkout. Every definition in the copies lives
in namespace `fu3_rv32` (the decoder and the register-name tables too), so a
binary may also link the shared upstream `riscv_vp` target: ctest
`fu3_iss_mixed_backends` links both backends normally and elaborates one core
of each. Every FU3 edit is marked `FU3 fork` in the source.

## FU3 changes

Feature changes are opt-in through `fu3_rv32::core_options` (wrapper:
`cdc::cpu::cva6_core_options`); their defaults reproduce upstream behaviour.
Rows marked "always" are conformance fixes to the privileged/F specifications
and apply to every core, including an unconfigured one (`riscv_cpu_eval`, whose
hello regression is unchanged).

| Change | Option | Used by |
| --- | --- | --- |
| misa extensions removed at configuration | `misa_disable` | CVA6 (IMFC), Ibex (IMC+U) |
| Fast interrupts `irq_fast_i[n-1:0]` as mip/mie bits 16..30, priority above MEI/MSI/MTI, lowest index first | `fast_irqs` | Ibex (15) |
| mtvec MODE fixed vectored, BASE 256-byte aligned; reset value | `mtvec_vectored_only`, `mtvec_reset` | Ibex (reset 0x1) |
| Zcb decoding: c.lbu, c.lhu, c.lh, c.sb, c.sh, c.zext.b, c.not, c.mul; c.sext.b/h, c.zext.h (need Zbb) and c.zext.w (RV64) stay illegal | `zcb` | Ibex |
| Enforced PMP: TOR/NA4/NAPOT, lowest matching entry, partial match fails, lock and TOR-top lock, R=0/W=1 WARL, unimplemented entries read 0 | `pmp_regions` | CVA6 (8), Ibex (16) |
| Smepmp 1.0: mseccfg (0x747) MML/MMWP sticky, RLB rules, MML truth table, refusal of new M-only executable rules | `smepmp`, `mseccfg_reset` | Ibex |
| PMP reset values | `pmpcfg_reset`, `pmpaddr_reset` | Ibex (VP proposal, see ibex_config.h) |
| Misaligned loads/stores passed to the memory interface (which splits them) instead of trapping | `split_misaligned` | Ibex |
| Instruction cycle from the CPU clock; memory instructions cost one cycle when the bus models latency | `cycle_time`, `bus_timed_memory` | both (2.5 ns) |
| Clock period changeable at run time (`ISS::set_cycle_time`, from the CRM); cycles are counted as integers per opcode, so mcycle stays exact across a change | always | wrapper `set_clock_period()` |
| `ISS::reset()`: architectural reset of registers, privilege, CSRs and PMP | always available | wrapper `reset_cpu()` |
| mstatus.MPP WARL: M only without U, no S | always | both |
| EBREAK raises the breakpoint exception (cause 3) instead of flagging the upstream gdb stub (with `debug`: Debug Mode per dcsr.ebreakm/ebreaku, see below) | always | both |
| CSR privilege: encoding 00 is the lowest privilege allowed, not a U-mode feature (fflags/frm/fcsr and user counters exist without U); only N-extension CSRs need N | always | CVA6 |
| FP state: FLW/FSW and FP CSR accesses illegal with FS = Off; FLW and FP CSR writes set FS = Dirty; rounding mode RMM (4) accepted, 5/6 and dynamic 5-7 illegal | always | CVA6 |
| xRET/WFI privilege: MRET illegal below M, SRET needs S and S-or-higher, URET needs N; WFI below M illegal with TW; MRET to a lower mode clears MPRV; unsupported MPP values read back as M | always | both |
| mcycle/mcycleh writable (offset onto the cycle count), cycle/cycleh read the same value, cleared by reset | always | both |
| `halt_request` level: WFI does not sleep while the owner requests a halt (reset) | always | wrapper reset |
| Debug Mode, debug spec 0.13.2 execution based: entry on `debug_request` (level, cause haltreq), EBREAK with dcsr.ebreakm/ebreaku, execute trigger, single step (dcsr.step, stepie); entry jumps to `debug_halt_address`, an exception in Debug Mode to `debug_exception_address` with no CSR update; DRET (legal only in Debug Mode) returns to dpc/dcsr.prv; no interrupt in Debug Mode; WFI a NOP in Debug Mode and while stepping; dcsr/dpc/dscratch0-1 only in Debug Mode; Debug Mode accesses to the DM aperture bypass PMP; reset leaves Debug Mode, the request survives it | `debug`, `debug_halt_address`, `debug_exception_address`, `debug_module_base/size` | both |
| Triggers: tselect (WARL), tdata1 mcontrol type 2, dmode 1, action 1, exact execute-address match on M/U, hit bit; tdata1/tdata2 writable only in Debug Mode; tinfo = mcontrol | `triggers` | CVA6 (2), Ibex (4) |

PMP is checked in the wrapper's memory adapters through `ISS::pmp_permits()`
(fetch uses the current privilege, data the MPRV-effective one); the bus
access policy is a separate, platform-level check.

## Not in the fork yet

- Bitmanip (Zba/Zbb/Zbc/Zbs) for CVA6: the subset is not decided (plan Q01-a).
- Load/store triggers, icount, tcontrol, textra/mcontext, trigger chaining and
  timing: not decided (Q04-e); CVA6 trigger type/fields are provisional.
- dcsr.stopcount/stoptime/ebreaks: hardwired 0 (no S-mode, counters and
  timers keep running in Debug Mode).
- NMI: `irq_nm_i` is tied low on FU3 (no source), so it is not modelled.
- Ibex-specific CSRs (cpuctrlsts, secureseed) and SecureIbex/lockstep/ECC
  effects: plan Q01-f, listed in the T05 report.
