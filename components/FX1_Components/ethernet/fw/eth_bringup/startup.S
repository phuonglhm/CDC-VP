/* =============================================================================
 * Integrated Circuits and Systems laboratory
 * Author: Hung Nguyen <ngthung@hcmut.edu.vn>
 * Module: virtual_platform.fw.eth_bringup.startup
 * Description: Reset entry: stack, gp, clear .bss/.sbss, main, report verdict.
 * Changelog:
 *   2026-09-29: Initial version.
 * ============================================================================= */
    .section .text.init, "ax"
    .globl _start
_start:
    .option push
    .option norelax
    la      gp, __global_pointer$
    .option pop
    la      sp, __stack_top
    /* static storage starts at zero only because this loop makes it so */
    la      t0, __bss_start
    la      t1, __bss_end
1:  bgeu    t0, t1, 2f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       1b
2:  call    main
    /* main's return value is the verdict: SIM_CTRL.EXIT ends the run */
    li      t0, 0x40020000
    sw      a0, 0(t0)
3:  wfi
    j       3b
