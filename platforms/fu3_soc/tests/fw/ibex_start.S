// Ibex test image entry in IBEX_ISRAM (0x0005_0000): stack, vectored mtvec, main.
    .section .text.start, "ax"
    .globl _start
_start:
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    la sp, __stack_top
    la t0, fw_vectors
    ori t0, t0, 1
    csrw mtvec, t0
    call main
1:  j 1b
