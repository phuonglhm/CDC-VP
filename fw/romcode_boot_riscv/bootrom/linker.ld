/* ROM-code image: text/rodata in BOOTROM @0x0, stack carved out of SoC RAM. */
OUTPUT_ARCH(riscv)
ENTRY(_start)

MEMORY
{
    ROM (rx)  : ORIGIN = 0x00000000, LENGTH = 64K
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 16K
}

SECTIONS
{
    . = ORIGIN(ROM);

    .text : {
        *(.text.init)        /* _start first, at exactly 0x0 */
        *(.text*)
    } > ROM

    .rodata : { *(.rodata*) } > ROM

    /* ROM code keeps no writable data; stack only. */
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
