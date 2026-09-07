OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 64K
}

PHDRS
{
    text PT_LOAD FLAGS(5); /* R-X */
    data PT_LOAD FLAGS(6); /* RW- */
}

SECTIONS
{
    . = ORIGIN(RAM);
    .text : {
        KEEP(*(.text.init))
        *(.text*)
    } > RAM :text
    .rodata : { *(.rodata*) } > RAM :text
    /* RISC-V puts small objects in .sdata/.sbss, not .data/.bss.  Missing
       those patterns silently leaves them out of the zeroed range.
       The explicit ":data" is what puts them in the RW segment; without it
       they inherit ":text" and land in an executable segment. */
    .data   : { *(.data*) *(.sdata*) } > RAM :data
    . = ALIGN(4);
    __bss_start = .;
    /* NOLOAD: .bss occupies memory but contributes no file bytes. */
    .bss (NOLOAD) : { *(.sbss*) *(.bss*) *(COMMON) } > RAM :data
    . = ALIGN(4);
    __bss_end = .;
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
