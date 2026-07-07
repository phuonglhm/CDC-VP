/* BOOTROM reset entry (linked at 0x0): set stack in RAM, run the ROM code. */
    .section .text.init
    .globl _start
_start:
    la      sp, _stack_top
    call    rom_main
hang:
    wfi
    j       hang
