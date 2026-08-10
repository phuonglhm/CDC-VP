/* SPDX-License-Identifier: Apache-2.0
 *
 * Freestanding RV32GCV smoke image (decision record D4).
 *
 * Deliberately tiny and self-contained: no libc, no newlib startup, no heap.
 * The layout matches the TPU_V3 address map only loosely on purpose — Phase 2
 * runs this image against a flat probe memory, not against the SoC. Phase 10
 * firmware gets a linker script generated from `tpu_v3/address_map.h`.
 */

OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    /* One flat region. The Phase 2 harness maps a single probe memory at 0, so
     * splitting text and data across regions would only add a way to be wrong
     * about where things live. */
    RAM (rwxa) : ORIGIN = 0x00000000, LENGTH = 1M
}

SECTIONS
{
    . = ORIGIN(RAM);

    .text : ALIGN(4)
    {
        KEEP(*(.text.init))     /* _start must be first: the reset PC is 0 */
        *(.text .text.*)
        *(.rodata .rodata.*)
    } > RAM

    .data : ALIGN(8)
    {
        /* Normally supplied by the toolchain's default linker script, which
         * `-T` replaces. gp addresses ±2 KiB around itself, so it is placed
         * 0x800 into the small-data area to cover as much of it as possible.
         * Without this, crt0's `la gp, __global_pointer$` fails to link. */
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.data .data.*)
        *(.sdata .sdata.*)
    } > RAM

    /* Zeroed by crt0, so the image on disk stays small. */
    .bss (NOLOAD) : ALIGN(8)
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    /* Vector test buffers need 64-byte alignment: one RVV register at
     * VLEN=512 is 64 bytes, and a misaligned buffer would turn an alignment
     * bug into a data bug that looks like an arithmetic error. */
    .vdata (NOLOAD) : ALIGN(64)
    {
        __vdata_start = .;
        *(.vdata .vdata.*)
        __vdata_end = .;
    } > RAM

    . = ALIGN(16);
    __stack_bottom = .;
    . = . + 0x4000;                 /* 16 KiB */
    __stack_top = .;

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
