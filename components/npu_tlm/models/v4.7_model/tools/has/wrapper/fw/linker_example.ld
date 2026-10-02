/*
 * linker_example.ld -- memory layout of the firmware example: code and data in the first megabyte of RAM, the program
 * stream after the code, and the program's DRAM image at a fixed, page-aligned address above them. The NPU reads and
 * writes the image through the wrapper's DRAM window, so it must lie in system RAM and must not overlap the firmware.
 * Adapt ORIGIN and the image address to the platform.
 */
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    RAM   (rwx) : ORIGIN = 0x80000000, LENGTH = 1M
    IMAGE (rw)  : ORIGIN = 0x81000000, LENGTH = 240M
}

SECTIONS
{
    .text : {
        *(.text.init)
        *(.text*)
    } > RAM

    .rodata : { *(.rodata*) *(.npu_has_stream) } > RAM
    .data   : { *(.data*) } > RAM
    .bss    : { *(.bss*) *(COMMON) } > RAM

    . = ALIGN(16);
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);

    .npu_has_image : { KEEP(*(.npu_has_image)) } > IMAGE
}
