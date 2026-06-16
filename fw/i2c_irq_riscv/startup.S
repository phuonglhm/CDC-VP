    .section .text.init
    .globl _start
_start:
    la      sp, _stack_top
    call    main
hang:
    wfi
    j       hang
