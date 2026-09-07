.section .text.init
    .globl _start
_start:
    la      sp, _stack_top

    /* Zero .bss: C guarantees static storage starts at 0, and nothing else
       does it on bare metal. */
    la      t0, __bss_start
    la      t1, __bss_end
1:
    bgeu    t0, t1, 2f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       1b
2:
    call    main
3:
    wfi
    j       3b
