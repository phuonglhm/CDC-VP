/* Ibex T05 test image: boot stub in ROM, test code in IBEX_ISRAM (image part,
   below the 4 KiB debug code at 0x0006_0000), data and stack in DSRAM above
   the CVA6 area and below the test mailbox. */
OUTPUT_ARCH(riscv)
ENTRY(rom_boot)

MEMORY
{
    ROM (rx)   : ORIGIN = 0x00000000, LENGTH = 4K
    ISRAM (rx) : ORIGIN = 0x00050000, LENGTH = 64K
    DEBUG (rx) : ORIGIN = 0x00060000, LENGTH = 4K
    RAM (rw)   : ORIGIN = 0x01020000, LENGTH = 64K
}

SECTIONS
{
    .rom : { KEEP(*(.rom)) } > ROM
    .text : { *(.text.start) *(.text.trap) *(.text .text.*) *(.rodata .rodata.* .srodata .srodata.*) } > ISRAM
    .debug_code : { KEEP(*(.debug_code)) } > DEBUG
    .data : { *(.data .data.* .sdata .sdata.*) } > RAM
    .bss (NOLOAD) : { *(.bss .bss.* .sbss .sbss.* COMMON) } > RAM
    __global_pointer$ = ADDR(.data) + 0x800;
    __debug_stack_top = 0x0103f400;
    __stack_top = 0x0103c000;
}
