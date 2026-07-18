    .section .text.init
    .globl _start
_start:
    la      sp, _stack_top
    call    main
1:
    wfi
    j       1b
