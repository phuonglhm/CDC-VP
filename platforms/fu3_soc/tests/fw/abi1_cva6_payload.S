// FU3_VP_ABI_1 smoke: CVA6 program staged by the Ibex boot flow into
// CVA6_ISRAM (0x0001_0000, CVA6 reset PC). Counts its start in DSRAM, then
// sends a software interrupt to Ibex (SYSCSR SWIRQ_SET bit 1) and sleeps.
// Position independent except for the absolute addresses.

    .section .rodata.abi1_cva6_payload, "a"
    .option push
    .option norvc
    .balign 4
    .globl abi1_cva6_payload, abi1_cva6_payload_end
abi1_cva6_payload:
    li t0, 0x0103fe20           // ABI1_CVA6_STARTS
    lw t1, 0(t0)
    addi t1, t1, 1
    sw t1, 0(t0)
    li t0, 0x4401011c           // SYSCSR + FU3_VP_SWIRQ_SET_OFFSET
    li t1, 2                    // FU3_VP_SWIRQ_IBEX
    sw t1, 0(t0)
1:  wfi
    j 1b
abi1_cva6_payload_end:
    .option pop
