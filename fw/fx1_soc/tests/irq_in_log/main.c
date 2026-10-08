/*
 * G1-R1: an interrupt that becomes pending while this hart prints must be
 * taken after the console critical section, and its handler may log.
 * The timer is armed to expire in the middle of a long fx1_log(); the handler
 * logs too. The test checks the deadline fell inside the print window and the
 * handler ran only when the line was complete (deferred, not nested).
 */
#include "fx1_fw.h"

static volatile uint32_t handled;
static volatile uint64_t handled_at;

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    (void)mtval;
    if (mcause != (FX1_MCAUSE_INTERRUPT | FX1_IRQ_M_TIMER)) fx1_fail(0x0E0);
    fx1_set_mtimecmp(0, ~0ull);
    handled_at = fx1_mtime();
    fx1_log("timer handler logged");
    handled = 1;
    return mepc;
}

int main(void)
{
    fx1_set_trap_handler(handler);
    fx1_irq_enable(FX1_MIE_MTIE);
    fx1_global_irq_enable();

    const uint64_t start = fx1_mtime();
    /* 10 us ahead; the line below takes tens of microseconds to print. */
    const uint64_t deadline = start + 200;
    fx1_set_mtimecmp(0, deadline);
    const uint64_t before_log = fx1_mtime();
    fx1_log("a long line printed while the timer expires: "
            "0123456789012345678901234567890123456789012345678901234567890123456789"
            "0123456789012345678901234567890123456789012345678901234567890123456789");
    const uint64_t printed = fx1_mtime();

    const uint64_t wait_start = fx1_mtime();
    while (!handled) {
        if (fx1_mtime() - wait_start > FX1_MTIME_HZ / 1000) return 0x01; /* 1 ms */
    }
    fx1_check(before_log < deadline && deadline < printed, 0x02,
              "deadline fell inside the print window");
    /* Deferred, not nested: the IRQ is taken when log_line() restores MIE at
     * the end of the line, so most of the remaining print time lies between
     * the deadline and the handler. A nested IRQ would run at the deadline. */
    fx1_check(2 * (handled_at - deadline) >= printed - deadline, 0x03,
              "interrupt deferred until the line was complete");
    fx1_log("interrupt during log: deferred and handled");
    return 0;
}
