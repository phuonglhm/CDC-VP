/* CVA6 T05 test image: code in CVA6_ISRAM (image part, below the 4 KiB debug
   code at 0x0004_0000), data and stack in DSRAM below the Ibex area and the
   test mailbox. */
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    ISRAM (rx) : ORIGIN = 0x00010000, LENGTH = 192K
    DEBUG (rx) : ORIGIN = 0x00040000, LENGTH = 4K
    RAM (rw)   : ORIGIN = 0x01000000, LENGTH = 64K
}

SECTIONS
{
    .text : { *(.text.start) *(.text.trap) *(.text .text.*) *(.rodata .rodata.* .srodata .srodata.*) } > ISRAM
    .debug_code : { KEEP(*(.debug_code)) } > DEBUG
    .data : { *(.data .data.* .sdata .sdata.*) } > RAM
    .bss (NOLOAD) : { *(.bss .bss.* .sbss .sbss.* COMMON) } > RAM
    __global_pointer$ = ADDR(.data) + 0x800;
    __debug_stack_top = 0x0103f800;
    __stack_top = 0x0103e000;
}
