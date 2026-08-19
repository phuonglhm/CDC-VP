/* SPDX-License-Identifier: Apache-2.0
 *
 * Freestanding RV32GCV startup for the Phase 7 NEO-CORE pipeline image.
 *
 * No libc, no newlib crt0, no libgloss. The toolchain has no vector multilib,
 * so linking any of them against an RVV build would silently pull scalar
 * objects built for a different `-march`. `-nostdlib -nostartfiles` avoids the
 * question entirely, which is what D4 requires for the Phase 2 smoke image.
 */

/* `.S` is preprocessed, so the exit protocol comes from the same header the C
 * code and the host harness use. Passing it as a `-D` instead would put the
 * address in three places. */
#include "neo_core_map.h"

    .section .text.init, "ax"
    .global _start
    .type _start, @function

_start:
    /* No relaxation against gp: nothing sets it before this point, so letting
     * the linker relax gp-relative accesses here would produce loads through a
     * register that is still zero. */
.option push
.option norelax
    la      gp, __global_pointer$
.option pop

    la      sp, __stack_top

    /* Zero .bss. The image does not carry it, so a test reading an
     * uninitialised counter would otherwise read whatever the memory model
     * happened to hold. */
    la      t0, __bss_start
    la      t1, __bss_end
1:
    bgeu    t0, t1, 2f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       1b
2:

    /* Trap vector, direct mode. Every unexpected trap lands in `trap_entry`,
     * which records the cause and halts: a smoke test that silently looped in a
     * trap handler would look like a hang, not a failure. */
    la      t0, trap_entry
    csrw    mtvec, t0

    /* Enable the FPU and the vector unit before touching either. mstatus.FS
     * and mstatus.VS reset to Off, and an RVV instruction executed with VS=Off
     * raises an illegal-instruction trap — which is a correct trap, and exactly
     * the kind of correct trap that looks like a broken vector unit. */
    li      t0, (3 << 13) | (3 << 9)     /* FS=Dirty, VS=Dirty */
    csrs    mstatus, t0

    call    main

    /* main's return value is the exit status. */
    mv      a1, a0
    li      a0, 0                        /* 0 = normal exit */
    j       sim_exit

    .size _start, . - _start

/* Trap entry.
 *
 * Calls `trap_handler(mcause, mepc, mtval)`, which decides what happens next:
 *
 *   TRAP_ACTION_ABORT      report and stop        (the default handler)
 *   TRAP_ACTION_RESUME     mret, mepc unchanged   re-run the faulting instruction
 *   TRAP_ACTION_SKIP       mret, mepc + 4         step over a 32-bit instruction
 *   TRAP_ACTION_RESUME_AT  mret to `trap_resume_pc`
 *
 * `trap_handler` is weak, so an image that does not define one keeps the old
 * behaviour: any trap is a defect and stops the run. Resuming by default would
 * turn a genuine fault into a silent loop.
 *
 * Every caller-saved register is preserved. The handler is ordinary C and may
 * clobber all of them, and it can be re-entered on an instruction that the
 * interrupted code will resume into. Vector state is untouched on purpose:
 * `vstart`, `vtype` and `vl` must survive so the resumed instruction restarts
 * where the RVV spec says it should.
 */
    .align 4
    .global trap_entry
    .type trap_entry, @function
trap_entry:
    addi    sp, sp, -80
    sw      ra, 0(sp)
    sw      t0, 4(sp)
    sw      t1, 8(sp)
    sw      t2, 12(sp)
    sw      t3, 16(sp)
    sw      t4, 20(sp)
    sw      t5, 24(sp)
    sw      t6, 28(sp)
    sw      a0, 32(sp)
    sw      a1, 36(sp)
    sw      a2, 40(sp)
    sw      a3, 44(sp)
    sw      a4, 48(sp)
    sw      a5, 52(sp)
    sw      a6, 56(sp)
    sw      a7, 60(sp)

    csrr    a0, mcause
    csrr    a1, mepc
    csrr    a2, mtval
    call    trap_handler
    mv      t0, a0                       /* action */

    /* TRAP_ACTION_SKIP: step over the 32-bit instruction that trapped. */
    li      t1, TRAP_ACTION_SKIP
    bne     t0, t1, 1f
    csrr    t2, mepc
    addi    t2, t2, 4
    csrw    mepc, t2
1:
    /* TRAP_ACTION_RESUME_AT: continue at `trap_resume_pc` instead.
     *
     * Needed for faults that cannot be stepped over: an instruction-fetch fault
     * leaves `mepc` pointing at unmapped memory, so `mepc + 4` is unmapped too
     * and skipping would fault forever. The handler names a recovery point
     * instead. */
    li      t1, TRAP_ACTION_RESUME_AT
    bne     t0, t1, 2f
    la      t2, trap_resume_pc
    lw      t2, 0(t2)
    csrw    mepc, t2
2:
    beqz    t0, 3f                       /* TRAP_ACTION_ABORT */

    lw      ra, 0(sp)
    lw      t0, 4(sp)
    lw      t1, 8(sp)
    lw      t2, 12(sp)
    lw      t3, 16(sp)
    lw      t4, 20(sp)
    lw      t5, 24(sp)
    lw      t6, 28(sp)
    lw      a0, 32(sp)
    lw      a1, 36(sp)
    lw      a2, 40(sp)
    lw      a3, 44(sp)
    lw      a4, 48(sp)
    lw      a5, 52(sp)
    lw      a6, 56(sp)
    lw      a7, 60(sp)
    addi    sp, sp, 80
    mret

3:  /* unhandled */
    csrr    a2, mcause
    csrr    a3, mepc
    csrr    a4, mtval
    li      a0, 1                        /* 1 = trapped */
    li      a1, 0xff
    j       sim_exit
    .size trap_entry, . - trap_entry

/* Default handler: every trap is a defect. Overridden by images that
 * deliberately provoke one. */
/* Recovery point for TRAP_ACTION_RESUME_AT. Weak so every image links; the
 * one image that uses the action defines it strongly. */
    .section .data
    .align 2
    .weak trap_resume_pc
trap_resume_pc:
    .word 0
    .section .text, "ax"

    .weak trap_handler
    .type trap_handler, @function
trap_handler:
    li      a0, 0                        /* TRAP_ACTION_ABORT */
    ret
    .size trap_handler, . - trap_handler

/* Report the result to the harness and stop.
 *
 * The contract is a memory-mapped word rather than a syscall: the Phase 2
 * harness is a probe memory and a TLM socket, with no syscall handler, and
 * `ecall` would take a trap instead. Writing to the exit register is visible on
 * the same TLM path as everything else, which is the point.
 *
 *   a0 = 0 normal exit, 1 trapped
 *   a1 = status (0 = pass)
 */
    .align 4
    .global sim_exit
    .type sim_exit, @function
sim_exit:
    li      t0, SIM_BASE
    sw      a2, 8(t0)                    /* mcause, or 0 on a normal exit */
    sw      a3, 12(t0)                   /* mepc */
    sw      a1, 4(t0)                    /* status */
    sw      a0, 0(t0)                    /* kind: written last, it is the trigger */
1:
    /* The harness stops the simulation when the trigger is written. Spinning
     * rather than executing an illegal instruction keeps the failure mode
     * "harness did not stop us", which is diagnosable. */
    j       1b
    .size sim_exit, . - sim_exit
