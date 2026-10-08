/*
 * G1: CLINT timer and software interrupts on both harts.
 *
 * Per hart, three timer rounds: arm mtimecmp a fixed number of ticks ahead,
 * WFI, take exactly one MTIP interrupt (the handler disarms mtimecmp, so the
 * level drops before mret), and check the interrupt did not fire early.
 * Hart 0 then sends three IPIs to hart 1; hart 1 must take exactly one
 * software interrupt per IPI. rdtime must track MMIO mtime.
 */
#include "fx1_fw.h"

#define ROUNDS      3u
#define AHEAD_TICKS 200u /* 10 us at 20 MHz */

static volatile uint32_t timer_hits[FX1_NUM_HARTS];
static volatile uint64_t timer_seen[FX1_NUM_HARTS];
static volatile uint32_t soft_hits;
static volatile uint32_t hart1_done;
static volatile uint32_t hart1_error;

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    (void)mtval;
    const uint32_t hart = fx1_hartid();
    if (mcause == (FX1_MCAUSE_INTERRUPT | FX1_IRQ_M_TIMER)) {
        timer_seen[hart] = fx1_mtime();
        fx1_set_mtimecmp(hart, ~0ull); /* disarm: MTIP must drop */
        ++timer_hits[hart];
    } else if (mcause == (FX1_MCAUSE_INTERRUPT | FX1_IRQ_M_SOFT)) {
        fx1_set_msip(hart, 0);
        ++soft_hits;
    } else {
        fx1_log_hex("unexpected mcause", mcause);
        fx1_fail(0x0E0);
    }
    return mepc;
}

static uint32_t run_timer_rounds(void)
{
    const uint32_t hart = fx1_hartid();
    fx1_set_trap_handler(handler);
    fx1_irq_enable(FX1_MIE_MTIE);
    fx1_global_irq_enable();
    for (uint32_t round = 0; round < ROUNDS; ++round) {
        const uint32_t before = timer_hits[hart];
        const uint64_t armed = fx1_mtime();
        const uint64_t deadline = armed + AHEAD_TICKS;
        fx1_set_mtimecmp(hart, deadline);
        while (timer_hits[hart] == before) fx1_wfi();
        if (timer_seen[hart] < deadline) return 0x10 + round;  /* fired early */
        /* No second interrupt for the same event (the level was dropped). */
        const uint64_t settle = fx1_mtime();
        while (fx1_mtime() - settle < AHEAD_TICKS) {
        }
        if (timer_hits[hart] != before + 1) return 0x20 + round;
    }
    const uint64_t a = fx1_mtime(), t = fx1_rdtime(), b = fx1_mtime();
    if (t < a || t > b) return 0x30; /* rdtime and mtime share one source */
    return 0;
}

void secondary_main(uint32_t hart)
{
    (void)hart;
    hart1_error = run_timer_rounds();
    fx1_irq_enable(FX1_MIE_MSIE);
    hart1_done = 1;
    for (;;) fx1_wfi(); /* software interrupts from hart 0 land here */
}

int main(void)
{
    fx1_release_secondaries();
    const uint32_t error = run_timer_rounds();
    fx1_check(error == 0, 0x100 | error, "hart 0 timer rounds");
    fx1_log("hart 0 timer: 3 interrupts, one per deadline");

    while (!hart1_done) {
        /* poll: hart 0's timer is disarmed, so a WFI here would never wake */
    }
    fx1_check(hart1_error == 0, 0x200 | hart1_error, "hart 1 timer rounds");
    fx1_log("hart 1 timer: 3 interrupts, one per deadline");

    for (uint32_t ipi = 1; ipi <= ROUNDS; ++ipi) {
        fx1_set_msip(1, 1);
        const uint64_t start = fx1_mtime();
        while (soft_hits != ipi) {
            if (fx1_mtime() - start > FX1_MTIME_HZ / 1000) return 0x300 | ipi; /* 1 ms */
        }
    }
    const uint64_t settle = fx1_mtime();
    while (fx1_mtime() - settle < AHEAD_TICKS) {
    }
    fx1_check(soft_hits == ROUNDS, 0x310, "exactly one software interrupt per IPI");
    fx1_log("hart 1 software interrupts: 3 of 3");
    return 0;
}
