// Ibex T05 test ROM: a boot stub, not a product Boot ROM.
//
// Reset fetches boot_addr + 0x80. The stub records the reset state the test
// checks (PC, mtvec, mseccfg, pmpcfg0) in the test mailbox, uses the reset RLB
// to make the IBEX_ISRAM staging rule (entry 4) executable, and jumps to the
// test image there. Any trap while in ROM marks the test failed.

    .section .rom, "ax"
    .option push
    .option norvc

    .globl rom_vectors
rom_vectors:                    // mtvec reset value 0x1: BASE 0, vectored
    .rept 32
    j rom_trap
    .endr

    .org 0x80
    .globl rom_boot
rom_boot:
    auipc t0, 0                 // PC of the first instruction after reset
    li t2, 0x0103ff40           // FW_MAILBOX_IBEX
    sw t0, 36(t2)               // reset_info[3]
    csrr t0, mtvec
    sw t0, 24(t2)               // reset_info[0]
    csrr t0, 0x747              // mseccfg
    sw t0, 28(t2)               // reset_info[1]
    csrr t0, pmpcfg0
    sw t0, 32(t2)               // reset_info[2]
    // Entry 4 (IBEX_ISRAM TOR): L | TOR | R | X, allowed while RLB = 1.
    li t0, 0xff
    csrc pmpcfg1, t0
    li t0, 0x8d
    csrs pmpcfg1, t0
    la t0, _start
    jr t0

rom_trap:
    li t2, 0x0103ff40
    li t0, 0xbad0ffff
    sw t0, 0(t2)
1:  j 1b

    .option pop
