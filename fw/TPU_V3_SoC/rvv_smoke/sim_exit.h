/* SPDX-License-Identifier: Apache-2.0
 *
 * The exit protocol between the freestanding RV32GCV smoke image and its host
 * harness. Shared by the firmware and the C++ test so the two cannot drift.
 *
 * A memory-mapped block rather than a syscall: the Phase 2 harness is a probe
 * memory behind a TLM socket with no syscall handler, so `ecall` would take a
 * trap instead of exiting. Writing to these addresses also keeps the exit
 * visible on the same TLM path as every other access, which is what the gate
 * wants to observe.
 *
 * `SIM_EXIT_KIND` is written **last** and is the trigger: the harness stops the
 * simulation when it sees a write there, by which point status, cause and
 * `mepc` are already in place.
 */

#ifndef TPU_V3_RVV_SMOKE_SIM_EXIT_H
#define TPU_V3_RVV_SMOKE_SIM_EXIT_H

/* Inside the image's 1 MiB RAM, well above the code, data and stack. */
#define SIM_EXIT_ADDR 0x000F0000

#define SIM_EXIT_KIND   (SIM_EXIT_ADDR + 0)  /* 0 = normal exit, 1 = trapped */
#define SIM_EXIT_STATUS (SIM_EXIT_ADDR + 4)  /* 0 = pass, else the failing check id */
#define SIM_EXIT_MCAUSE (SIM_EXIT_ADDR + 8)
#define SIM_EXIT_MEPC   (SIM_EXIT_ADDR + 12)

/* Three `mcycle` samples, taken by the firmware itself.
 *
 * They exist for the F11 gate. `mcycle` is only produced correctly by the CSR
 * read path — the ISS recomputes it there — so sampling it from the host would
 * check a different quantity than the one that was wrong. Reading it from
 * firmware checks exactly what firmware sees.
 *
 * The gate requires the first sample to be small (a sane baseline, not a
 * multi-second one) and the three to be strictly increasing.
 */
#define SIM_EXIT_MCYCLE_START (SIM_EXIT_ADDR + 16)
#define SIM_EXIT_MCYCLE_MID   (SIM_EXIT_ADDR + 20)
#define SIM_EXIT_MCYCLE_END   (SIM_EXIT_ADDR + 24)

/* ── one-shot fault injection ────────────────────────────────────────────────
 *
 * The firmware writes an address here; the harness makes the *next* access to
 * that address return a TLM error, once, then disarms itself. VP++ turns an
 * error response into a load or store page fault (`mem.h`), which is how a trap
 * lands in the middle of a vector instruction.
 *
 * Firmware-driven rather than armed by the harness from ELF symbols: it keeps
 * the timing explicit and stops the test depending on symbol lookup.
 * Writing 0 disarms.
 */
#define SIM_FAULT_ARM (SIM_EXIT_ADDR + 32)

/* ── trap-result signature ───────────────────────────────────────────────────
 *
 * Captured by the trap handler. This block is deliberately the same shape that
 * the Phase 2 differential corpus will compare between VP++ and Spike, so the
 * trap semantics settled here define that signature rather than being
 * retrofitted to it.
 */
#define SIM_TRAP_COUNT  (SIM_EXIT_ADDR + 64)
#define SIM_TRAP_MCAUSE (SIM_EXIT_ADDR + 68)
#define SIM_TRAP_MEPC   (SIM_EXIT_ADDR + 72)
#define SIM_TRAP_MTVAL  (SIM_EXIT_ADDR + 76)
#define SIM_TRAP_VSTART (SIM_EXIT_ADDR + 80)
#define SIM_TRAP_VL     (SIM_EXIT_ADDR + 84)
#define SIM_TRAP_VTYPE  (SIM_EXIT_ADDR + 88)

/* Observed outcome of the RV32 index-EEW=64 probe, recorded rather than
 * judged. Whether `vluxei64.v` must raise an illegal instruction on RV32 is not
 * self-evident: index EEW 64 does not exceed ELEN 64 here, so the plan's
 * phrase "RV32 restriction on 64-bit vector index EEW" has to be pinned to a
 * spec clause before it can be a pass/fail criterion. The Spike differential
 * run is what settles it. */
#define SIM_EEW64_TRAPPED (SIM_EXIT_ADDR + 96)
#define SIM_EEW64_MCAUSE  (SIM_EXIT_ADDR + 100)

/* Phase marker. The firmware writes the number of the phase it has just
 * finished; the host snapshots its per-address read counts when phase 1 ends.
 *
 * Needed because later phases touch the same buffer: the EEW=64 probe indexes
 * off `vsrc` with an all-zero index vector and reads element 0 sixteen more
 * times. Counting to the end of the run would attribute those to the vstart
 * resumption and fail a correct machine. */
#define SIM_PHASE_MARK (SIM_EXIT_ADDR + 104)

/* ── F5 concurrency probe results ────────────────────────────────────────────
 *
 * One block per hart, in that hart's own memory, so the two never write to the
 * same words and a leak cannot be manufactured by the test itself.
 */
#define SIM_FP_HART              (SIM_EXIT_ADDR + 128)
#define SIM_FP_FRM               (SIM_EXIT_ADDR + 132)
#define SIM_FP_EXPECTED          (SIM_EXIT_ADDR + 136)
#define SIM_FP_ITERATIONS        (SIM_EXIT_ADDR + 140)
#define SIM_FP_RESULT_MISMATCHES (SIM_EXIT_ADDR + 144)
#define SIM_FP_FLAG_MISMATCHES   (SIM_EXIT_ADDR + 148)
#define SIM_FP_FRM_MISMATCHES    (SIM_EXIT_ADDR + 152)
#define SIM_FP_FIRST_BAD_VALUE   (SIM_EXIT_ADDR + 156)
#define SIM_FP_FIRST_BAD_FLAGS   (SIM_EXIT_ADDR + 160)
#define SIM_FP_LAST_RESULT       (SIM_EXIT_ADDR + 164)

/* ── D12 / D13 conformance gate ──────────────────────────────────────────────
 *
 * Two downstream conformance patches, one image.
 *
 * D12: all 32 RV32 indexed encodings with index EEW=64 must raise an illegal
 * instruction, and must do so *before* they count a load/store, touch the bus
 * or dirty `mstatus.VS`. Those three are what distinguishes a check placed at
 * the decode site from one placed inside `vLoadStore()`.
 *
 * D13: a failed bus access must report an **access** fault whose cause follows
 * what the access was for, never a page fault. All six origins are exercised,
 * because deriving the cause from the TLM command alone gets fetch wrong and
 * AMO wrong.
 */
#define SIM_D12_TRAP_MASK   (SIM_EXIT_ADDR + 192)  /* bit per encoding: 4 unit + 28 segment */
#define SIM_D12_MCAUSE_OK   (SIM_EXIT_ADDR + 196)  /* 1 = every one of them was cause 2 */
#define SIM_D12_VSTART_KEPT (SIM_EXIT_ADDR + 200)  /* 1 = vstart survived all 32 */
#define SIM_D12_VD_KEPT     (SIM_EXIT_ADDR + 204)  /* 1 = the destination register survived */
#define SIM_D12_VS_KEPT     (SIM_EXIT_ADDR + 208)  /* 2 = all 32 stayed Clean, 3 = one dirtied VS */
#define SIM_D12_PHASE_BEGIN (SIM_EXIT_ADDR + 212)  /* host zeroes its access counter here */
#define SIM_D12_PHASE_END   (SIM_EXIT_ADDR + 216)  /* host snapshots its access counts here */

#define SIM_D13_FETCH_MCAUSE  (SIM_EXIT_ADDR + 224)
#define SIM_D13_FETCH_MTVAL   (SIM_EXIT_ADDR + 228)
#define SIM_D13_LOAD_MCAUSE   (SIM_EXIT_ADDR + 232)
#define SIM_D13_LOAD_MTVAL    (SIM_EXIT_ADDR + 236)
#define SIM_D13_STORE_MCAUSE  (SIM_EXIT_ADDR + 240)
#define SIM_D13_STORE_MTVAL   (SIM_EXIT_ADDR + 244)
#define SIM_D13_VLOAD_MCAUSE  (SIM_EXIT_ADDR + 248)
#define SIM_D13_VLOAD_MTVAL   (SIM_EXIT_ADDR + 252)
#define SIM_D13_VLOAD_VSTART  (SIM_EXIT_ADDR + 256)
#define SIM_D13_VSTORE_MCAUSE (SIM_EXIT_ADDR + 260)
#define SIM_D13_VSTORE_MTVAL  (SIM_EXIT_ADDR + 264)
#define SIM_D13_VSTORE_VSTART (SIM_EXIT_ADDR + 268)
#define SIM_D13_AMO_MCAUSE    (SIM_EXIT_ADDR + 272)
#define SIM_D13_AMO_MTVAL     (SIM_EXIT_ADDR + 276)
/* A target that refuses with TLM_GENERIC_ERROR_RESPONSE rather than
 * TLM_ADDRESS_ERROR_RESPONSE. Both mean "the target said no" and both must
 * become the same guest access fault; the protocol-error statuses must not,
 * and that half is checked by a separate run of the harness. */
#define SIM_D13_GENERIC_MCAUSE (SIM_EXIT_ADDR + 280)
#define SIM_D13_GENERIC_MTVAL  (SIM_EXIT_ADDR + 284)

/* ── interrupt gate ──────────────────────────────────────────────────────────
 *
 * A request/ack pair rather than the host driving lines on a timer. The
 * firmware says which line it wants raised; the host raises it and echoes the
 * line back in `SIM_IRQ_ACK`. That keeps the two sides in step under temporal
 * decoupling, where "wait a while and hope the other process ran" is a race
 * whose failure looks like a dead interrupt line.
 *
 * `SIM_IRQ_HEARTBEAT` exists so the spin loop produces bus traffic. A pure
 * register spin never yields, so the host's process would never get to run and
 * the interrupt would never arrive — a hang that looks exactly like a broken
 * `set_irq()`.
 */
#define SIM_IRQ_REQUEST   (SIM_EXIT_ADDR + 320)
#define SIM_IRQ_ACK       (SIM_EXIT_ADDR + 324)
#define SIM_IRQ_HEARTBEAT (SIM_EXIT_ADDR + 328)

#define SIM_IRQ_NONE           0u
#define SIM_IRQ_LINE_SOFTWARE  3u   /* MSIP */
#define SIM_IRQ_LINE_TIMER     7u   /* MTIP */
#define SIM_IRQ_LINE_EXTERNAL 11u   /* MEIP */

#define SIM_IRQ_SOFTWARE_MCAUSE (SIM_EXIT_ADDR + 332)
#define SIM_IRQ_SOFTWARE_COUNT  (SIM_EXIT_ADDR + 336)
#define SIM_IRQ_TIMER_MCAUSE    (SIM_EXIT_ADDR + 340)
#define SIM_IRQ_TIMER_COUNT     (SIM_EXIT_ADDR + 344)
#define SIM_IRQ_EXTERNAL_MCAUSE (SIM_EXIT_ADDR + 348)
#define SIM_IRQ_EXTERNAL_COUNT  (SIM_EXIT_ADDR + 352)
#define SIM_IRQ_MASKED_COUNT    (SIM_EXIT_ADDR + 356)
#define SIM_IRQ_TOTAL           (SIM_EXIT_ADDR + 360)
/* Sampled inside the wait loop. Without them a line that never fires is
 * indistinguishable from one the firmware never enabled. */
#define SIM_IRQ_MSTATUS         (SIM_EXIT_ADDR + 364)
#define SIM_IRQ_MIE             (SIM_EXIT_ADDR + 368)
#define SIM_IRQ_MIP             (SIM_EXIT_ADDR + 372)

/* What the trap handler tells `crt0` to do next. */
#define TRAP_ACTION_ABORT  0u  /* unhandled: report and stop */
#define TRAP_ACTION_RESUME 1u  /* mret with mepc unchanged: re-run the instruction */
#define TRAP_ACTION_SKIP   2u  /* mret with mepc + 4: step over a 32-bit instruction */
#define TRAP_ACTION_RESUME_AT 3u /* mret to `trap_resume_pc` */

#define SIM_EXIT_KIND_NORMAL  0u
#define SIM_EXIT_KIND_TRAPPED 1u

#define SIM_EXIT_PASS 0u

#endif /* TPU_V3_RVV_SMOKE_SIM_EXIT_H */
