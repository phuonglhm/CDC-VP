/* IFLASH application: XIP from the 4 MiB read-only window @0x0400_0000.
 * No writable .data/.bss (flash is read-only); stack lives in SoC RAM. */
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    IFLASH (rx)  : ORIGIN = 0x04000000, LENGTH = 4M
    RAM    (rwx) : ORIGIN = 0x80004000, LENGTH = 16K
}

SECTIONS
{
    . = ORIGIN(IFLASH);

    .text : {
        *(.text.init)        /* _start first, at exactly 0x04000000 */
        *(.text*)
    } > IFLASH

    .rodata : { *(.rodata*) } > IFLASH

    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
