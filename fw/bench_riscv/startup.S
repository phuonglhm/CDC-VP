/* Minimal RISC-V reset entry: set the stack pointer, call main, then halt. */
    .section .text.init
    .globl _start
_start:
    la      sp, _stack_top      /* stack grows down from top of RAM */
    call    main
hang:
    wfi
    j       hang
