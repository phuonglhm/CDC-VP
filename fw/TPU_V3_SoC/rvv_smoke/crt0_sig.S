/* SPDX-License-Identifier: Apache-2.0
 *
 * Startup for the Spike differential corpus image.
 *
 * `crt0.S` cannot be reused unchanged for one reason: its exit protocol is a
 * write to a memory-mapped address that only the VP++ harness understands.
 * Spike has no such device, so this image exits through HTIF instead —
 * `tohost`, which fesvr polls natively and which the harness watches by looking
 * the symbol up in the same ELF. One exit path, two simulators, nothing to keep
 * in step by hand.
 *
 * Everything else — gp/sp, .bss, mtvec, enabling FS and VS, the weak
 * `trap_handler` returning an action — is deliberately identical to `crt0.S`,
 * because the corpus is meant to test the machine, not a second startup.
 */

#include "sig_layout.h"

    .section .text.init, "ax"
    .global _start
    .type _start, @function

_start:
.option push
.option norelax
    la      gp, __global_pointer$
.option pop

    la      sp, __stack_top

    la      t0, __bss_start
    la      t1, __bss_end
1:
    bgeu    t0, t1, 2f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       1b
2:

    la      t0, trap_entry
    csrw    mtvec, t0

    /* FS=Dirty, VS=Dirty. Both reset to Off, and the corpus starts by using
     * both units. The FS=Off and FS=Clean probes set them back deliberately. */
    li      t0, (3 << 13) | (3 << 9)
    csrs    mstatus, t0

    call    main

    mv      a1, a0                       /* main's return value is the status */
    li      a0, TPU_V3_TERM_NORMAL
    j       sig_exit

    .size _start, . - _start

/* Trap entry. Same contract as `crt0.S`:
 *
 *   SIG_TRAP_ACTION_ABORT   report and stop
 *   SIG_TRAP_ACTION_RESUME  mret, mepc unchanged
 *   SIG_TRAP_ACTION_SKIP    mret, mepc + 4
 *
 * Only 32-bit instructions are provoked in this corpus, so SKIP adding 4 is
 * correct for every deliberate trap here. The `.option norvc` regions in
 * `sig_main.c` keep it that way.
 *
 * Vector state is untouched: `vstart`, `vtype` and `vl` have to survive for the
 * mid-vector fault probe to mean anything.
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
    mv      t0, a0

    li      t1, SIG_TRAP_ACTION_SKIP
    bne     t0, t1, 1f
    csrr    t2, mepc
    addi    t2, t2, 4
    csrw    mepc, t2
1:
    beqz    t0, 2f                       /* SIG_TRAP_ACTION_ABORT */

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

2:  /* Unhandled. The signature block already holds whatever the handler
     * recorded, so an abort still produces a comparable signature — a model
     * that dies early is then visible as a diff in `termination` and
     * `phase_reached` rather than as an empty file. */
    li      a0, TPU_V3_TERM_TRAP_ABORT
    li      a1, 0xff
    j       sig_exit
    .size trap_entry, . - trap_entry

    .weak trap_handler
    .type trap_handler, @function
trap_handler:
    li      a0, SIG_TRAP_ACTION_ABORT
    ret
    .size trap_handler, . - trap_handler

/* Exit through HTIF.
 *
 *   a0 = termination reason, a1 = status
 *
 * Both land in the signature block first, then `tohost` is written last as the
 * trigger. fesvr treats bit 0 as "exited" and bits 63:1 as the exit code, so
 * the value is `(status << 1) | 1`.
 *
 * The high word is written before the low word. fesvr reads `tohost` as one
 * 64-bit quantity from a host thread, and this ordering means a read that lands
 * between the two stores sees the old zero rather than a half-formed value.
 * `tohost` starts zeroed by the linker script, so storing zero to the high word
 * is a no-op in the normal case and only matters for that race.
 */
    .align 4
    .global sig_exit
    .type sig_exit, @function
sig_exit:
    la      t0, begin_signature
    sw      a0, (TPU_V3_SIG_termination * 4)(t0)
    sw      a1, (TPU_V3_SIG_status * 4)(t0)

    la      t0, tohost
    slli    t1, a1, 1
    ori     t1, t1, 1
    sw      zero, 4(t0)
    sw      t1, 0(t0)
1:
    /* Spike stops at the store above. The VP++ harness stops on the same store,
     * observed as a bus write. Spinning rather than executing something illegal
     * keeps a harness that failed to stop us diagnosable as a hang at a known
     * PC. */
    j       1b
    .size sig_exit, . - sig_exit
