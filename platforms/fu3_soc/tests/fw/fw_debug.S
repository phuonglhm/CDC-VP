// T06 Debug Mode test code, linked at the CPU's debug halt address (debug
// partition of its ISRAM). Execution-based Debug Mode: entry jumps to +0, an
// exception in Debug Mode to +8. Not a product debug ROM.
//
// The entry saves the caller-saved registers on a private debug stack (sp in
// dscratch0), runs fw_debug_handler() and returns with DRET. An exception in
// Debug Mode runs fw_debug_exception() and leaves through the same restore
// path; the saved frame is always at the top of the debug stack.

    .section .debug_code, "ax"
    .option push
    .option norvc

    .globl fw_debug_rom
fw_debug_rom:
    j fw_debug_entry            // debug_halt_address
    nop
    j fw_debug_exc              // debug_exception_address (+8)

fw_debug_entry:
    csrw dscratch0, sp
    la sp, __debug_stack_top
    addi sp, sp, -64
    sw ra, 0(sp)
    sw t0, 4(sp)
    sw t1, 8(sp)
    sw t2, 12(sp)
    sw a0, 16(sp)
    sw a1, 20(sp)
    sw a2, 24(sp)
    sw a3, 28(sp)
    sw a4, 32(sp)
    sw a5, 36(sp)
    sw a6, 40(sp)
    sw a7, 44(sp)
    sw t3, 48(sp)
    sw t4, 52(sp)
    sw t5, 56(sp)
    sw t6, 60(sp)
    call fw_debug_handler
fw_debug_restore:
    la sp, __debug_stack_top
    addi sp, sp, -64
    lw ra, 0(sp)
    lw t0, 4(sp)
    lw t1, 8(sp)
    lw t2, 12(sp)
    lw a0, 16(sp)
    lw a1, 20(sp)
    lw a2, 24(sp)
    lw a3, 28(sp)
    lw a4, 32(sp)
    lw a5, 36(sp)
    lw a6, 40(sp)
    lw a7, 44(sp)
    lw t3, 48(sp)
    lw t4, 52(sp)
    lw t5, 56(sp)
    lw t6, 60(sp)
    csrr sp, dscratch0
    dret

fw_debug_exc:
    la sp, __debug_stack_top
    addi sp, sp, -64
    call fw_debug_exception
    j fw_debug_restore

    .option pop
