/* Link the timer-IRQ demo at the riscv_cpu_eval RAM base (0x80000000). */
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 64K
}

SECTIONS
{
    . = ORIGIN(RAM);

    .text : {
        *(.text.init)        /* _start first */
        *(.text*)
    } > RAM

    .rodata : { *(.rodata*) } > RAM
    .data   : { *(.data*) }   > RAM
    .bss    : { *(.bss*) *(COMMON) } > RAM

    . = ALIGN(16);
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
