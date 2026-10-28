// FU3_VP_ABI_1 debug partition placeholder (VP ONLY). The Boot ROM copies a
// debug code blob to both debug partitions during staging (CVA6 HaltAddress
// 0x0004_0000, Ibex DmHaltAddr 0x0006_0000; exception entry at +8). The blob
// for silicon is owned by the DBG/DM team: on RTL the halted hart must run the
// Debug Module's own debug code (flags, abstract commands, resume). This
// placeholder only parks the hart, which is enough on the VP because its Debug
// Module serves abstract commands and resume outside the instruction stream.
// It uses no GPR and no stack.
    .section .rodata.vp_debug_blob, "a"
    .option push
    .option norvc
    .balign 4
    .globl vp_debug_blob, vp_debug_blob_end
vp_debug_blob:
    j .                 // +0: halt entry
    nop
    j .                 // +8: debug exception entry
vp_debug_blob_end:
    .option pop
