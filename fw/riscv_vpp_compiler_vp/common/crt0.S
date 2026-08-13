/* SPDX-License-Identifier: Apache-2.0
 *
 * Freestanding RV32GCV startup for `riscv_vpp_compiler_vp` images.
 *
 * No libc, no newlib crt0, no libgloss. The xPack toolchain ships no vector
 * multilib, so linking any of them against an RVV build would silently pull in
 * scalar objects compiled for a different `-march`. `-nostdlib -nostartfiles`
 * removes the question rather than answering it badly, and Phase 4.5 makes that
 * the documented profile for the first handoff: hosted `printf` and syscalls
 * are a later, separately gated feature.
 */

#include "compiler_vp/host_io_map.h"

    .section .text.init, "ax"
    .global _start
    .type _start, @function

_start:
    /* No relaxation against gp: nothing has set it yet, so letting the linker
     * relax the accesses below into gp-relative form would address through a
     * register that is still zero. */
.option push
.option norelax
    la      gp, __global_pointer$
.option pop

    la      sp, __stack_top

    /* Zero .bss and .vdata. Neither is carried in the image, so a program
     * reading an uninitialised result buffer would otherwise read whatever the
     * memory model happened to hold — which, for a sparse or freshly
     * constructed model, is zero, and the defect only appears on the machine
     * where it is not. */
    la      t0, __bss_start
    la      t1, __bss_end
1:  bgeu    t0, t1, 2f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       1b
2:
    la      t0, __vdata_start
    la      t1, __vdata_end
1:  bgeu    t0, t1, 2f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       1b
2:

    /* Trap vector, direct mode. Every unexpected trap lands in `trap_entry`,
     * which reports and stops: a program that silently looped inside a trap
     * handler would present as a hang, and a hang is the one failure mode the
     * watchdogs have to distinguish from a real one. */
    la      t0, trap_entry
    csrw    mtvec, t0

    /* Enable the FPU and the vector unit before touching either. `mstatus.FS`
     * and `mstatus.VS` reset to Off, and an RVV instruction executed with
     * VS=Off raises an illegal-instruction trap — a correct trap, and exactly
     * the kind of correct trap that reads as a broken vector unit. */
    li      t0, (3 << 13) | (3 << 9)     /* FS=Dirty, VS=Dirty */
    csrs    mstatus, t0

    /* argc/argv are not a thing here; main takes none. */
    call    main

    /* main's return value is the exit status. */
    mv      a1, a0
    li      a0, COMPILER_VP_EXIT_KIND_NORMAL
    li      a2, 0
    li      a3, 0
    j       hio_exit_raw

    .size _start, . - _start

/* Trap entry.
 *
 * Deliberately simpler than the Phase 2 handler, which supports resume, skip
 * and resume-at because its images provoke faults on purpose. Nothing in a
 * compiler handoff example should trap at all, so a trap here is a defect and
 * the only useful thing to do with it is report exactly where it happened and
 * stop. Adding recovery actions nobody uses would mean carrying untested paths
 * in the one piece of code every shipped image links.
 */
    .align 4
    .global trap_entry
    .type trap_entry, @function
trap_entry:
    csrr    a2, mcause
    csrr    a3, mepc
    li      a0, COMPILER_VP_EXIT_KIND_TRAPPED
    li      a1, 0xff
    j       hio_exit_raw
    .size trap_entry, . - trap_entry

/* Report the result to the platform and stop.
 *
 *   a0 = kind   (0 normal, 1 trapped)
 *   a1 = status (0 pass)
 *   a2 = mcause
 *   a3 = mepc
 *
 * `EXIT_KIND` is written last because it is the trigger; the other three words
 * are already in place when the platform sees it.
 */
    .align 4
    .global hio_exit_raw
    .type hio_exit_raw, @function
hio_exit_raw:
    /* Flush first: the console is line-buffered on the host side, and an image
     * that exits with a partial line would lose the very output the run is
     * being judged on. */
    li      t0, COMPILER_VP_HOSTIO_BASE
    li      t1, 1
    sw      t1, (COMPILER_VP_CONSOLE_FLUSH - COMPILER_VP_HOSTIO_BASE)(t0)

    sw      a2, (COMPILER_VP_EXIT_MCAUSE - COMPILER_VP_HOSTIO_BASE)(t0)
    sw      a3, (COMPILER_VP_EXIT_MEPC - COMPILER_VP_HOSTIO_BASE)(t0)
    sw      a1, (COMPILER_VP_EXIT_STATUS - COMPILER_VP_HOSTIO_BASE)(t0)
    sw      a0, (COMPILER_VP_EXIT_KIND - COMPILER_VP_HOSTIO_BASE)(t0)
1:
    /* The platform stops the simulation when the trigger is written. Spinning
     * rather than executing an illegal instruction keeps the failure mode
     * "the platform did not stop us", which is diagnosable; an illegal
     * instruction here would take a trap and re-enter this code. */
    j       1b
    .size hio_exit_raw, . - hio_exit_raw
