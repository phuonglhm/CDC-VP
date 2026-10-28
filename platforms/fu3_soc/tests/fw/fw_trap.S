// Trap entry and vector table shared by the T05 CPU test firmware.
//
// fw_trap_entry saves x1..x31 (x2 as the pre-trap sp), calls
// fw_trap_handler(frame) and resumes at the address it returns.
// fw_vectors is a 256-byte aligned vectored mtvec table: every entry jumps to
// fw_trap_entry; entry 11 (machine external) first writes 11 to mscratch so a
// test can tell that the vectored path was taken.

    .section .text.trap, "ax"
    .option push
    .option norvc

    .balign 256
    .globl fw_vectors
fw_vectors:
    j fw_trap_entry          // 0: exceptions
    .rept 10
    j fw_trap_entry          // 1-10
    .endr
    j fw_vector_11           // 11: machine external
    .rept 20
    j fw_trap_entry          // 12-31 (Ibex fast 16-30)
    .endr

fw_vector_11:
    csrwi mscratch, 11
    j fw_trap_entry

    .balign 4
    .globl fw_trap_entry
fw_trap_entry:
    addi sp, sp, -128
    sw x1, 4(sp)
    sw x3, 12(sp)
    sw x4, 16(sp)
    sw x5, 20(sp)
    sw x6, 24(sp)
    sw x7, 28(sp)
    sw x8, 32(sp)
    sw x9, 36(sp)
    sw x10, 40(sp)
    sw x11, 44(sp)
    sw x12, 48(sp)
    sw x13, 52(sp)
    sw x14, 56(sp)
    sw x15, 60(sp)
    sw x16, 64(sp)
    sw x17, 68(sp)
    sw x18, 72(sp)
    sw x19, 76(sp)
    sw x20, 80(sp)
    sw x21, 84(sp)
    sw x22, 88(sp)
    sw x23, 92(sp)
    sw x24, 96(sp)
    sw x25, 100(sp)
    sw x26, 104(sp)
    sw x27, 108(sp)
    sw x28, 112(sp)
    sw x29, 116(sp)
    sw x30, 120(sp)
    sw x31, 124(sp)
    addi t0, sp, 128
    sw t0, 8(sp)
    mv a0, sp
    call fw_trap_handler
    csrw mepc, a0
    lw x1, 4(sp)
    lw x3, 12(sp)
    lw x4, 16(sp)
    lw x5, 20(sp)
    lw x6, 24(sp)
    lw x7, 28(sp)
    lw x8, 32(sp)
    lw x9, 36(sp)
    lw x10, 40(sp)
    lw x11, 44(sp)
    lw x12, 48(sp)
    lw x13, 52(sp)
    lw x14, 56(sp)
    lw x15, 60(sp)
    lw x16, 64(sp)
    lw x17, 68(sp)
    lw x18, 72(sp)
    lw x19, 76(sp)
    lw x20, 80(sp)
    lw x21, 84(sp)
    lw x22, 88(sp)
    lw x23, 92(sp)
    lw x24, 96(sp)
    lw x25, 100(sp)
    lw x26, 104(sp)
    lw x27, 108(sp)
    lw x28, 112(sp)
    lw x29, 116(sp)
    lw x30, 120(sp)
    lw x31, 124(sp)
    addi sp, sp, 128
    mret

    // void fw_run_user(void (*fn)(void)): enter U-mode at fn; fn ends with
    // ecall, whose handler resumes at label 1 below in M-mode. fn's stack
    // frame is abandoned at the ecall, so sp is restored from fw_umode_sp, and
    // the trap frame holds fn's registers: ra and the callee-saved s0-s11 are
    // restored from this frame, as for any call.
    .globl fw_run_user
fw_run_user:
    addi sp, sp, -64
    sw ra, 0(sp)
    sw s0, 4(sp)
    sw s1, 8(sp)
    sw s2, 12(sp)
    sw s3, 16(sp)
    sw s4, 20(sp)
    sw s5, 24(sp)
    sw s6, 28(sp)
    sw s7, 32(sp)
    sw s8, 36(sp)
    sw s9, 40(sp)
    sw s10, 44(sp)
    sw s11, 48(sp)
    la t1, fw_umode_sp
    sw sp, 0(t1)
    la t0, 1f
    la t1, fw_umode_return
    sw t0, 0(t1)
    csrw mepc, a0
    li t0, 0x1800            // MPP = U
    csrc mstatus, t0
    mret
1:
    la t1, fw_umode_sp
    lw sp, 0(t1)
    lw ra, 0(sp)
    lw s0, 4(sp)
    lw s1, 8(sp)
    lw s2, 12(sp)
    lw s3, 16(sp)
    lw s4, 20(sp)
    lw s5, 24(sp)
    lw s6, 28(sp)
    lw s7, 32(sp)
    lw s8, 36(sp)
    lw s9, 40(sp)
    lw s10, 44(sp)
    lw s11, 48(sp)
    addi sp, sp, 64
    ret

    .section .bss.fw_umode_sp, "aw", @nobits
    .balign 4
fw_umode_sp:
    .space 4

    .option pop
