/* SPDX-License-Identifier: Apache-2.0
 *
 * Reset entry for the FreeRTOS VP_FX1 firmware.
 *
 * Unlike the bare-metal examples under fw/, mtvec is pointed at the FreeRTOS
 * port's trap handler, NOT an application handler: the port owns the machine
 * trap entry (context save/restore, tick, ecall yield). Application code must
 * never write mtvec (docs/NPU_V4_INTEGRATION_HANDOFF.md section 13.4).
 */

    .section .text.init
    .globl _start
_start:
    la      sp, _stack_top

    la      t0, freertos_risc_v_trap_handler
    csrw    mtvec, t0

    /* Zero .bss (kernel state, heap, ISR stack all live there). */
    la      t0, __bss_start
    la      t1, __bss_end
    bgeu    t0, t1, 2f
1:
    sw      zero, 0(t0)
    addi    t0, t0, 4
    bltu    t0, t1, 1b
2:
    call    main

    /* main() only returns if the scheduler failed to start. */
3:
    wfi
    j       3b
