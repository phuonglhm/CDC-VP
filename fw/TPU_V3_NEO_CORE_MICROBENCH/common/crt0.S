/* SPDX-License-Identifier: Apache-2.0
 *
 * Freestanding RV32GCV startup for the standalone NEO-CORE microbenchmark
 * images (G2).
 *
 * Structurally the same as the Phase 7 pipeline image's startup, and
 * deliberately a separate file rather than a shared one: the two images include
 * different map headers, and the pipeline image is behind a green gate that a
 * refactor of its startup would put at risk for no functional gain. Plan §8
 * puts the convergence point at the Phase 10 generated header; until then two
 * short startups are cheaper than one shared one with two configurations.
 *
 * No libc, no newlib crt0, no libgloss (D4): the toolchain has no vector
 * multilib, so linking any of them against an RVV build would pull scalar
 * objects built for a different `-march`.
 */

/* `.S` is preprocessed, so the exit protocol comes from the same header the C
 * code and the host runner use. */
#include "bench_map.h"

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

    /* Zero .bss. The image does not carry it, so a benchmark reading an
     * uninitialised counter would otherwise read whatever the memory model
     * happened to hold — and a zero that came from luck is indistinguishable
     * from a zero that was measured. */
    la      t0, __bss_start
    la      t1, __bss_end
1:
    bgeu    t0, t1, 2f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       1b
2:

    /* Trap vector, direct mode. Every unexpected trap lands in `trap_entry`,
     * which records the cause and halts: a benchmark that silently looped in a
     * trap handler would look like a hang, not a failure — and its measured
     * interval would never close. */
    la      t0, trap_entry
    csrw    mtvec, t0

    /* Enable the FPU and the vector unit before touching either. mstatus.FS
     * and mstatus.VS reset to Off, and an RVV instruction executed with VS=Off
     * raises an illegal-instruction trap — a correct trap that looks exactly
     * like a broken vector unit. */
    li      t0, (3 << 13) | (3 << 9)     /* FS=Dirty, VS=Dirty */
    csrs    mstatus, t0

    call    main

    /* main's return value is the exit status. */
    mv      a1, a0
    li      a0, 0                        /* 0 = normal exit */
    j       sim_exit

    .size _start, . - _start

/* Trap entry. Calls `trap_handler(mcause, mepc, mtval)`, which decides what
 * happens next; the weak default treats every trap as a defect and stops. */
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

    li      t1, TRAP_ACTION_SKIP
    bne     t0, t1, 1f
    csrr    t2, mepc
    addi    t2, t2, 4
    csrw    mepc, t2
1:
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

/* Recovery point for TRAP_ACTION_RESUME_AT. Weak so every image links. */
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

/* Report the result to the runner and stop.
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
    /* The runner stops the simulation when the trigger is written. Spinning
     * rather than executing an illegal instruction keeps the failure mode
     * "runner did not stop us", which is diagnosable. */
    j       1b
    .size sim_exit, . - sim_exit
