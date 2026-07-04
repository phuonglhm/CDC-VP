/*
 * riscv.ld - link VP_FX1 bare-metal firmware into the FW/RTOS RAM window.
 *
 * Link base 0x80000000 (CDC_FW_BASE). The FW window is 16 MiB; stack starts at
 * the top of the region and grows down. Keep firmware text/data/heap/stack
 * inside this window - the rest of RAM0 is reserved for accelerator buffers.
 */
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 16M
}

SECTIONS
{
    . = ORIGIN(RAM);

    .text : {
        *(.text.init)          /* _start must come first */
        *(.text*)
    } > RAM

    .rodata : { *(.rodata*) } > RAM
    .data   : { *(.data*) }   > RAM

    . = ALIGN(4);
    __bss_start = .;
    .bss : { *(.bss*) *(COMMON) } > RAM
    . = ALIGN(4);
    __bss_end = .;

    . = ALIGN(16);
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
