// Ibex boot stub for the CVA6 test runs (T07): a test stub, not a product
// Boot ROM. The CVA6 image is preloaded by the test harness; the stub only
// does what the boot flow does in hardware before the handoff:
//   PLL 20 MHz x 20 / (1 x 1) = 400 MHz, wait for lock, bus divider 1 (/ 2,
//   set while both selectors are still on the OSC: CRM spec 3.7.4), then CPU
//   and bus clocks from the PLL (CLK_0, then CLK_1), then CRM FCLK_SW[0] = 1,
//   which releases CVA6 (and closes the staging window).
// Afterwards Ibex sleeps.

    .section .rom, "ax"
    .option push
    .option norvc

    .org 0x80
    .globl rom_boot
rom_boot:
    li t0, 0x50010000           // CRM
    li t1, 0x150                // PLL: n_div 1, m_div 20, od 0 (/1)
    sw t1, 0x10(t0)
    li t1, 0x5948               // PLL_CTRL: reset value with powerdown cleared
    sw t1, 0x18(t0)
    li t2, 0x4000               // PLL.lock
1:  lw t1, 0x10(t0)
    and t1, t1, t2
    beqz t1, 1b
    li t1, 1
    sw t1, 0x24(t0)             // CLK: bus divider 1 -> / 2 (selectors still on the OSC)
    sw t1, 0x1c(t0)             // CLK_0: branch selector to the PLL
    sw t1, 0x20(t0)             // CLK_1: final selector to the branch
    sw t1, 0x34(t0)             // FCLK_SW: release CVA6
2:  wfi
    j 2b

    .option pop
