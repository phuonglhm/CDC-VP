/*
 * FX1 SoC firmware linker script. Preprocessed with -DFX1_LINKER_SCRIPT so the
 * DDR layout comes from fx1/fx1_memory_map.h.
 */
#include "fx1/fx1_memory_map.h"

OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    IMAGE (rwx) : ORIGIN = FX1_FW_IMAGE_BASE, LENGTH = FX1_FW_IMAGE_SIZE
    STACK (rw)  : ORIGIN = FX1_FW_STACK_BASE, LENGTH = FX1_FW_STACK_SIZE * FX1_NUM_HARTS
}

SECTIONS
{
    .text : {
        KEEP(*(.text.init))
        *(.text .text.*)
    } > IMAGE

    .rodata : ALIGN(8) {
        *(.rodata .rodata.* .srodata .srodata.*)
    } > IMAGE

    .data : ALIGN(8) {
        *(.data .data.*)
        __global_pointer$ = . + 0x800;
        *(.sdata .sdata.*)
    } > IMAGE

    .bss (NOLOAD) : ALIGN(8) {
        __bss_start = .;
        *(.sbss .sbss.* .bss .bss.* COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > IMAGE

    .stack (NOLOAD) : {
        __stack_base = .;
        . += FX1_FW_STACK_SIZE * FX1_NUM_HARTS;
        __stack_end = .;
    } > STACK

    /DISCARD/ : { *(.note* .comment .riscv.attributes) }
}

/* Plan C12: the firmware regions of fx1_memory_map.h are disjoint and in
 * order, so no DMA buffer can overlap the image, the stacks or another buffer
 * region. (The image and the stacks are also bounded by MEMORY above.) */
ASSERT(FX1_FW_IMAGE_BASE + FX1_FW_IMAGE_SIZE <= FX1_FW_STACK_BASE, "fx1: image region overlaps the stacks")
ASSERT(FX1_FW_STACK_BASE + FX1_FW_STACK_SIZE * FX1_NUM_HARTS <= FX1_FW_SHARED_BASE,
       "fx1: stacks overlap the shared region")
ASSERT(FX1_FW_SHARED_BASE + FX1_FW_SHARED_SIZE <= FX1_FW_RAW_BUF_BASE, "fx1: shared region overlaps RAW buffers")
ASSERT(FX1_FW_RAW_BUF_BASE < FX1_FW_YUV_BUF_BASE, "fx1: RAW and YUV buffer regions out of order")
ASSERT(FX1_FW_YUV_BUF_BASE < FX1_FW_TEST_AREA_BASE, "fx1: YUV buffers and the test area out of order")
ASSERT(FX1_FW_TEST_AREA_BASE < FX1_DDR_BASE + FX1_DDR_SIZE, "fx1: test area outside DDR")
