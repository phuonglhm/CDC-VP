// T07 boot test: CVA6 program that the Ibex boot test copies into CVA6_ISRAM
// (0x0001_0000, CVA6 reset PC) during staging. Each start of CVA6 increments
// a counter in DSRAM, then CVA6 sleeps. Position independent except for the
// counter address.

    .section .rodata.cva6_payload, "a"
    .option push
    .option norvc
    .balign 4
    .globl t07_cva6_payload, t07_cva6_payload_end
t07_cva6_payload:
    li t0, 0x0103fe00           // T07_CVA6_STARTS
    lw t1, 0(t0)
    addi t1, t1, 1
    sw t1, 0(t0)
1:  wfi
    j 1b
t07_cva6_payload_end:
    .option pop
