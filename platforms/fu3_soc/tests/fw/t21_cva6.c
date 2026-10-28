// Plan T21 wake/reset test, CVA6 side (VP verification firmware), released by
// t21_ibex.c. Interrupts disabled (mstatus.MIE = 0): WFI wakes on a pending
// interrupt enabled in mie, without a trap.
//   bit 0: TIMER0 -> time_irq_i (mip.MTIP);
//   bit 1: TIMER3 -> PLIC ID 8 -> irq_i[0] (mip.MEIP), claim/complete;
//   bit 2: SWIRQ from Ibex -> ipi_i (mip.MSIP), cleared, then SWIRQ to Ibex.
// Then it keeps reading and writing memory until the watchdog reset.
#include "fw_common.h"
#include "fu3_vp_abi.h"

struct fw_mailbox *const fw_box = FW_MAILBOX_CVA6;

#define REG(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#define TIMER0 0x44060000u
#define TIMER3 0x50070000u
#define PLIC 0x40100000u
#define PLIC_ID_TIMER3 8u
#define ABI(off) REG(FU3_VP_ABI_SYSCSR_BASE + (off))

#define MIP_MSIP (1u << 3)
#define MIP_MTIP (1u << 7)
#define MIP_MEIP (1u << 11)

#define T21_CVA6_READY REG(0x0103f308u)
#define T21_CVA6_RESULT REG(0x0103f30cu)
#define T21_TRAFFIC REG(0x0103f310u)

static void timer_arm(uint32_t base, uint32_t ticks)
{
    REG(base + 8) = REG(base + 0) + ticks;
    REG(base + 4) = 1;
}

static void timer_ack(uint32_t base)
{
    REG(base + 4) = 0;
    REG(base + 8) = 0;
}

int main(void)
{
    uint32_t result = 0;
    CSR_WRITE(mstatus, 0);

    CSR_WRITE(mie, MIP_MTIP);
    timer_arm(TIMER0, 40);
    __asm__ volatile("wfi");
    if (CSR_READ(mip) & MIP_MTIP) {
        timer_ack(TIMER0);
        if (!(CSR_READ(mip) & MIP_MTIP))
            result |= 1;
    }

    REG(PLIC + 4 * PLIC_ID_TIMER3) = 1;
    REG(PLIC + 0x2000) = 1u << PLIC_ID_TIMER3;
    REG(PLIC + 0x3000) = 0;
    CSR_WRITE(mie, MIP_MEIP);
    timer_arm(TIMER3, 40);
    __asm__ volatile("wfi");
    if ((CSR_READ(mip) & MIP_MEIP) && REG(PLIC + 0x3004) == PLIC_ID_TIMER3) {
        timer_ack(TIMER3);
        REG(PLIC + 0x3004) = PLIC_ID_TIMER3;   // complete
        if (!(CSR_READ(mip) & MIP_MEIP))
            result |= 2;
    }
    REG(PLIC + 0x2000) = 0;

    CSR_WRITE(mie, MIP_MSIP);
    T21_CVA6_READY = 1;
    __asm__ volatile("wfi");
    if (CSR_READ(mip) & MIP_MSIP) {
        ABI(FU3_VP_SWIRQ_CLEAR_OFFSET) = FU3_VP_SWIRQ_CVA6;
        if (!(CSR_READ(mip) & MIP_MSIP))
            result |= 4;
    }
    ABI(FU3_VP_SWIRQ_SET_OFFSET) = FU3_VP_SWIRQ_IBEX;
    T21_CVA6_RESULT = 0x80000000u | result;

    for (;;) {
        T21_TRAFFIC = T21_TRAFFIC + 1;
        (void)REG(TIMER0 + 0);
    }
    return 0;
}
