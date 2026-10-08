/*
 * FX1 SoC start-up for every hart (one ELF, D5 boot flow).
 *
 * All harts reset at _start. Hart 0 zeroes .bss and runs main(); its return
 * value goes to fx1_exit(). Every other hart parks in WFI with only MSIE
 * enabled until hart 0 sets fx1_secondary_release (forced into .data, so it is
 * 0 in the loaded image and is never touched by the .bss clear) and raises its
 * MSIP (fx1_release_secondaries). The hart then fences (acquire), clears its
 * own MSIP and calls secondary_main(hartid). Each hart gets its own 64 KiB
 * stack.
 */
#include "fx1/fx1_memory_map.h"

#define MIE_MSIE   (1 << 3)
#define MSTATUS_FS_INITIAL (1 << 13)

    .section .text.init, "ax"
    .globl  _start
_start:
    .option push
    .option norelax
    la      gp, __global_pointer$
    .option pop
    csrw    mie, zero
    csrr    t0, mhartid
    li      t1, FX1_NUM_HARTS
    bgeu    t0, t1, .Lpark

    /* sp = __stack_base + (hartid + 1) * FX1_FW_STACK_SIZE */
    la      sp, __stack_base
    addi    t2, t0, 1
    li      t1, FX1_FW_STACK_SIZE
    mul     t2, t2, t1
    add     sp, sp, t2

    la      t1, fx1_trap_entry
    csrw    mtvec, t1
    li      t1, MSTATUS_FS_INITIAL
    csrs    mstatus, t1
    csrw    fcsr, zero
    bnez    t0, .Lsecondary

    la      a0, __bss_start
    la      a1, __bss_end
1:  bgeu    a0, a1, 2f
    sw      zero, 0(a0)
    addi    a0, a0, 4
    j       1b
2:  call    main
    call    fx1_exit

.Lsecondary:
    li      t1, MIE_MSIE
    csrs    mie, t1
3:  wfi
    la      t2, fx1_secondary_release
    lw      t1, 0(t2)
    beqz    t1, 3b
    /* acquire: nothing hart 0 published, and no device access, moves above
     * the flag read (WFI is a wait, not a barrier) */
    fence   iorw, iorw
    /* acknowledge the release IPI: msip[hartid] = 0 */
    li      t1, FX1_CLINT_BASE + FX1_CLINT_MSIP_OFF
    slli    t2, t0, 2
    add     t1, t1, t2
    sw      zero, 0(t1)
    li      t1, MIE_MSIE
    csrc    mie, t1
    mv      a0, t0
    call    secondary_main
.Lpark:
    wfi
    j       .Lpark

/*
 * Machine trap entry. Saves the caller-saved integer registers, calls
 * fx1_trap_dispatch(mcause, mepc, mtval) and resumes at the mepc it returns.
 * Floating-point registers are not saved: handlers must not use FP.
 */
    .text
    .align  2
    .globl  fx1_trap_entry
fx1_trap_entry:
    addi    sp, sp, -64
    sw      ra,  0(sp)
    sw      t0,  4(sp)
    sw      t1,  8(sp)
    sw      t2, 12(sp)
    sw      a0, 16(sp)
    sw      a1, 20(sp)
    sw      a2, 24(sp)
    sw      a3, 28(sp)
    sw      a4, 32(sp)
    sw      a5, 36(sp)
    sw      a6, 40(sp)
    sw      a7, 44(sp)
    sw      t3, 48(sp)
    sw      t4, 52(sp)
    sw      t5, 56(sp)
    sw      t6, 60(sp)
    csrr    a0, mcause
    csrr    a1, mepc
    csrr    a2, mtval
    call    fx1_trap_dispatch
    csrw    mepc, a0
    lw      ra,  0(sp)
    lw      t0,  4(sp)
    lw      t1,  8(sp)
    lw      t2, 12(sp)
    lw      a0, 16(sp)
    lw      a1, 20(sp)
    lw      a2, 24(sp)
    lw      a3, 28(sp)
    lw      a4, 32(sp)
    lw      a5, 36(sp)
    lw      a6, 40(sp)
    lw      a7, 44(sp)
    lw      t3, 48(sp)
    lw      t4, 52(sp)
    lw      t5, 56(sp)
    lw      t6, 60(sp)
    addi    sp, sp, 64
    mret
