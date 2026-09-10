/* SPDX-License-Identifier: Apache-2.0
 *
 * The standalone NEO-CORE microbenchmark images, linked against the real
 * TPU_V3 map.
 *
 *   .text      GLOBAL_BOOT_ROM  0x0000_0000, the reset vector
 *   .data/.bss GLOBAL_RAM       0x8000_0000, first 32 KiB only
 *   buffers    CORE_SRAM        named explicitly in the firmware
 *
 * Text and data live in different regions on purpose: it makes every image
 * exercise the hart's external path for fetch and its local path for buffer
 * data, which is exactly the decode `neo_hart_port` performs.
 *
 * RAM is capped at 32 KiB rather than given the whole window. Above it sits the
 * region the host stages parameters and end-to-end buffers into, and the ASSERT
 * at the bottom is what keeps the two apart: an image whose data grew into the
 * buffers fails to link instead of overwriting the inputs at run time and
 * producing a benchmark result for data nobody supplied.
 */

OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    ROM (rxa)  : ORIGIN = 0x00000000, LENGTH = 64K
    RAM (rwxa) : ORIGIN = 0x80000000, LENGTH = 32K
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

    . = ALIGN(16);
    __stack_bottom = .;
    . = . + 0x2000;                 /* 8 KiB */
    __stack_top = .;

    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}

/* Must equal `BENCH_HOST_REGION_BASE` in `bench_map.h`. The runner asserts the
 * two agree; this assertion is what makes the linker enforce it. */
ASSERT(__stack_top <= 0x80008000,
       "firmware RAM usage has grown into the host-staged buffer region; \
raise BENCH_FIRMWARE_RAM_BYTES in bench_map.h and this script together")
