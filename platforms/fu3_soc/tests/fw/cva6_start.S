// CVA6 test firmware entry at boot_addr_i 0x0001_0000: stack, direct mtvec, main.
    .section .text.start, "ax"
    .globl _start
_start:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    la sp, __stack_top
    la t0, fw_trap_entry
    csrw mtvec, t0
    call main
1:  j 1b
