/* =============================================================================
 * Integrated Circuits and Systems laboratory
 * Author: Hung Nguyen <ngthung@hcmut.edu.vn>
 * Module: virtual_platform.fw.eth_bringup.linker
 * Description: 256 KiB of RAM at 0x80000000 for the Ethernet bring-up firmware.
 * Changelog:
 *   2026-09-29: Initial version.
 * ============================================================================= */
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY { RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 256K }

SECTIONS
{
    .text : {
        KEEP(*(.text.init))          /* the entry point survives --gc-sections */
        *(.text .text.*)
    } > RAM
    .rodata : { *(.rodata .rodata.* .srodata .srodata.*) } > RAM
    .data : {
        *(.data .data.*)
        __global_pointer$ = . + 0x800;
        *(.sdata .sdata.*)
    } > RAM
    .bss (NOLOAD) : {
        __bss_start = .;
        *(.sbss .sbss.* .bss .bss.* COMMON)   /* .sbss and .bss, one sweep */
        . = ALIGN(4);
        __bss_end = .;
    } > RAM
    . = ALIGN(16);
    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
