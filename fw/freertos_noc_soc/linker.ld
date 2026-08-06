OUTPUT_ARCH(riscv)
/* SPDX-License-Identifier: Apache-2.0 */

ENTRY(_start)

/*
 * noc_soc maps 16 MiB at 0x8000_0000 but reserves its final 4 KiB page,
 * [0x80ff_f000, 0x8100_0000), for the synthetic survey contract. Firmware
 * mode still rejects an ELF PT_LOAD that claims that page, so the reservation
 * is excluded here rather than left to image-size luck.
 */
MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 0x00FFF000
}

SECTIONS
{
    . = ORIGIN(RAM);

    .text : {
        *(.text.init)
        *(.text*)
    } > RAM

    .rodata : { *(.rodata*) *(.srodata*) } > RAM
    .data   : { *(.data*) *(.sdata*) } > RAM

    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > RAM

    __firmware_ram_end = ORIGIN(RAM) + LENGTH(RAM);
    ASSERT(__bss_end <= __firmware_ram_end,
           "FreeRTOS image exceeds noc_soc firmware RAM")

    . = ALIGN(16);
    _stack_top = __firmware_ram_end;
}
