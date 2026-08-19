/* SPDX-License-Identifier: Apache-2.0
 *
 * The Phase 7 NEO-CORE pipeline image, linked against the real TPU_V3 map.
 *
 * Unlike the Phase 2 smoke image, which ran on one flat probe memory, this one
 * is laid out where the architecture actually puts things:
 *
 *   .text     GLOBAL_BOOT_ROM   0x0000_0000, because that is the reset vector
 *   .data     GLOBAL_RAM        0x8000_0000, because the boot ROM refuses
 *                               ordinary writes and .data is written
 *   buffers   CORE_SRAM         0xC000_0000, named explicitly in the firmware
 *
 * Splitting text and data across two regions is the point rather than an
 * inconvenience: it makes the image exercise the hart's external path for
 * fetch and its local path for data, which is exactly the decode
 * `neo_hart_port` exists to perform.
 */

OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    ROM (rxa)  : ORIGIN = 0x00000000, LENGTH = 64K
    RAM (rwxa) : ORIGIN = 0x80000000, LENGTH = 64K
}

SECTIONS
{
    .text : ALIGN(4)
    {
        KEEP(*(.text.init))     /* _start first: the reset PC is 0 */
        *(.text .text.*)
        *(.rodata .rodata.*)
    } > ROM

    .data : ALIGN(8)
    {
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.data .data.*)
        *(.sdata .sdata.*)
    } > RAM

    .bss (NOLOAD) : ALIGN(8)
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    .vdata (NOLOAD) : ALIGN(64)
    {
        __vdata_start = .;
        *(.vdata .vdata.*)
        __vdata_end = .;
    } > RAM

    . = ALIGN(16);
    __stack_bottom = .;
    . = . + 0x2000;                 /* 8 KiB */
    __stack_top = .;

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
