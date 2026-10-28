// Plan T09 / decision Q03-d on CVA6 with the real TIMER0 (no harness-driven
// interrupt): TIMER0 -> pending latch -> CVA6 time_irq_i (mip.MTIP, cause 7).
// TIMER0 is periodic (model contract): COUNT runs 0..CMP, the match lasts one
// tick and clears COUNT; a CMP write also clears COUNT.
//   - periodic tick: the ISR acknowledges by writing CMP (Q03-d);
//   - an ISR that comes late (interrupts masked while matches pass) still
//     finds the event: the latch holds MTIP until the CMP write;
//   - a read of CMP does not acknowledge, a write does;
//   - after the acknowledge the next match interrupts again (re-arm).
// Released by the Ibex handoff stub (PLL on).
#include "fw_common.h"

struct fw_mailbox *const fw_box = FW_MAILBOX_CVA6;

#define TIMER0 0x44060000u
#define T_COUNT (*(volatile uint32_t *)(TIMER0 + 0x0))
#define T_CTRL (*(volatile uint32_t *)(TIMER0 + 0x4))
#define T_CMP (*(volatile uint32_t *)(TIMER0 + 0x8))
#define MIP_MTIP (1u << 7)
#define MIE_MTIE (1u << 7)
#define PERIOD 4000u   // timer ticks between matches: well above the ISR length

static volatile uint32_t ticks;
static volatile uint32_t last_cause;
static volatile uint32_t mtip_after_ack;

static void timer_isr(uint32_t cause)
{
    last_cause = cause;
    ticks = ticks + 1;
    if (cause != 7)
        return;
    T_CMP = PERIOD;   // acknowledge (Q03-d); also restarts the period
    mtip_after_ack = CSR_READ(mip) & MIP_MTIP;
}

// Busy-waits until `n` matches have passed (COUNT wrapped back), bounded.
static int wait_matches(uint32_t n)
{
    uint32_t previous = T_COUNT;
    for (uint32_t i = 0; i < 100000 && n; ++i) {
        const uint32_t now = T_COUNT;
        if (now < previous)
            --n;
        previous = now;
    }
    return n == 0;
}

static int wait_ticks(uint32_t n)
{
    for (uint32_t i = 0; i < 2000000; ++i)
        if (ticks >= n)
            return 1;
    return 0;
}

int main(void)
{
    fw_irq_hook = timer_isr;
    T_CTRL = 0;
    CSR_SET(mie, MIE_MTIE);

    // 1. Periodic tick, acknowledged by the ISR.
    T_CMP = PERIOD;
    T_CTRL = 1;
    fw_check(1, T_CTRL == 1 && T_CMP == PERIOD, T_CMP);
    CSR_SET(mstatus, MSTATUS_MIE);
    fw_check(2, wait_ticks(5), ticks);
    CSR_CLEAR(mstatus, MSTATUS_MIE);
    fw_check(3, last_cause == 7, last_cause);
    fw_check(4, fw_trap.irq_count >= 5 && fw_trap.irq_order[0] == 7 && fw_trap.irq_order[4] == 7, fw_trap.irq_count);
    fw_check(5, mtip_after_ack == 0, mtip_after_ack);

    // 2. Late ISR: two matches pass while interrupts are masked.
    T_CMP = PERIOD;   // acknowledges a pending event, restarts the period
    fw_check(6, (CSR_READ(mip) & MIP_MTIP) == 0, CSR_READ(mip));
    const uint32_t before = ticks;
    fw_check(7, wait_matches(2), T_COUNT);
    fw_check(8, (CSR_READ(mip) & MIP_MTIP) != 0, CSR_READ(mip));   // latched past the one-tick match
    // 3. A read of CMP does not acknowledge.
    fw_check(9, T_CMP == PERIOD, T_CMP);
    fw_check(10, (CSR_READ(mip) & MIP_MTIP) != 0, CSR_READ(mip));
    CSR_SET(mstatus, MSTATUS_MIE);   // the late ISR runs now, exactly once
    for (uint32_t i = 0; i < 20; ++i)
        __asm__ volatile("nop");
    CSR_CLEAR(mstatus, MSTATUS_MIE);
    fw_check(11, ticks == before + 1, ticks - before);
    fw_check(12, (CSR_READ(mip) & MIP_MTIP) == 0, CSR_READ(mip));   // the ISR's CMP write acknowledged

    // 4. After the acknowledge, the next match interrupts again.
    CSR_SET(mstatus, MSTATUS_MIE);
    fw_check(13, wait_ticks(before + 2), ticks - before);
    CSR_CLEAR(mstatus, MSTATUS_MIE);

    T_CTRL = 0;
    T_CMP = 0;
    CSR_CLEAR(mie, MIE_MTIE);
    fw_irq_hook = 0;
    fw_finish();
    return 0;
}
