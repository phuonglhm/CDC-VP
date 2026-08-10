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

/* What the trap handler tells `crt0` to do next. */
#define TRAP_ACTION_ABORT  0u  /* unhandled: report and stop */
#define TRAP_ACTION_RESUME 1u  /* mret with mepc unchanged: re-run the instruction */
#define TRAP_ACTION_SKIP   2u  /* mret with mepc + 4: step over a 32-bit instruction */

#define SIM_EXIT_KIND_NORMAL  0u
#define SIM_EXIT_KIND_TRAPPED 1u

#define SIM_EXIT_PASS 0u

#endif /* TPU_V3_RVV_SMOKE_SIM_EXIT_H */
