/*
 * startup_riscv.S - shared reset entry for VP_FX1 bare-metal firmware.
 *
 * Sets the stack pointer, clears .bss, then calls main(). If main returns the
 * core parks in a wfi loop. Interrupt vector (mtvec) is left to firmware, which
 * installs its own trap_handler when it enables interrupts.
 *
 * Symbols _stack_top, __bss_start, __bss_end come from the linker script.
 */
    .section .text.init
    .globl _start
_start:
    la      sp, _stack_top          /* stack grows down from top of RAM   */

    /* Zero the .bss segment. */
    la      t0, __bss_start
    la      t1, __bss_end
1:
    bgeu    t0, t1, 2f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       1b
2:
    call    main

hang:
    wfi
    j       hang
