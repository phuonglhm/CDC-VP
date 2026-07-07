/* App entry (linked at 0x04000000): the ROM code jumps here with no ABI
 * contract, so re-set the stack before calling C. */
    .section .text.init
    .globl _start
_start:
    la      sp, _stack_top
    call    app_main
hang:
    wfi
    j       hang
