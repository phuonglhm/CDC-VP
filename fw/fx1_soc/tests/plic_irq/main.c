/*
 * G2: PLIC with two M-mode contexts (hart 0 / hart 1).
 *
 * Sources: 1 = UART (real device), 4..7 = VP-only test lines (sim-control).
 *   A  interrupt delivery on hart 0: one claim per request, line lowered in the
 *      handler before complete, no re-entry
 *   B  priority order, threshold (gates MEIP only; claim ignores it), enable
 *      mask, level re-pend after complete (polled claims with MIE off;
 *      mip.MEIP checked directly)
 *   C  routing to hart 1 while it sleeps in WFI; a source enabled on both
 *      contexts is claimed exactly once
 *   D  UART TX interrupt through the PLIC, cleared at the device
 */
#include "fx1_fw.h"

#define UART_IMSC (FX1_UART_BASE + 0x038u)
#define UART_TXIM 0x20u
#define MIP_MEIP  (1u << FX1_IRQ_M_EXT)
#define SRC_TEST0 FX1_SIM_CTRL_IRQ_FIRST_SOURCE /* source 4 = test line bit 0 */

static volatile uint32_t claimed[FX1_PLIC_NUM_SOURCES]; /* successful claims per source */
static volatile uint32_t hart_claims[FX1_NUM_HARTS];
static volatile uint32_t spurious[FX1_NUM_HARTS];
static volatile uint32_t hart1_ready;
static fx1_lock_t line_lock; /* sim-control line mask is read-modify-write */

static void line(uint32_t source, int level)
{
    /* The handler lowers lines too: mask interrupts while holding the lock. */
    const uint32_t saved = fx1_irq_save();
    fx1_lock(&line_lock);
    uint32_t mask = fx1_sim_irq_get();
    const uint32_t bit = 1u << (source - SRC_TEST0);
    fx1_sim_irq_set(level ? (mask | bit) : (mask & ~bit));
    fx1_unlock(&line_lock);
    fx1_irq_restore(saved);
}

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    (void)mtval;
    if (mcause != (FX1_MCAUSE_INTERRUPT | FX1_IRQ_M_EXT)) fx1_fail(0x0E0);
    const uint32_t hart = fx1_hartid();
    const uint32_t context = FX1_PLIC_CONTEXT_HART_M(hart);
    const uint32_t id = fx1_plic_claim(context);
    if (id == 0) { /* another context took it first */
        ++spurious[hart];
        return mepc;
    }
    if (id == FX1_IRQ_UART) fx1_write32(UART_IMSC, 0); /* clear at the device */
    else if (id >= SRC_TEST0) line(id, 0);
    __atomic_fetch_add(&claimed[id], 1, __ATOMIC_RELAXED);
    ++hart_claims[hart];
    fx1_plic_complete(context, id);
    return mepc;
}

static int wait_for(volatile uint32_t* value, uint32_t expected)
{
    const uint64_t start = fx1_mtime();
    while (*value != expected)
        if (fx1_mtime() - start > FX1_MTIME_HZ / 1000) return 0; /* 1 ms */
    return 1;
}

static void settle(void)
{
    const uint64_t start = fx1_mtime();
    while (fx1_mtime() - start < 200) { /* 10 us: catch any late re-entry */
    }
}

void secondary_main(uint32_t hart)
{
    (void)hart;
    fx1_set_trap_handler(handler);
    fx1_irq_enable(FX1_MIE_MEIE);
    fx1_global_irq_enable();
    fx1_release();
    hart1_ready = 1;
    for (;;) fx1_wfi(); /* every delivery below wakes this WFI */
}

int main(void)
{
    const uint32_t c0 = FX1_PLIC_CONTEXT_HART_M(0), c1 = FX1_PLIC_CONTEXT_HART_M(1);
    fx1_release_secondaries();
    fx1_check(wait_for(&hart1_ready, 1), 0x01, "hart 1 ready");
    fx1_plic_set_priority(FX1_IRQ_UART, 4);
    fx1_plic_set_priority(4, 1);
    fx1_plic_set_priority(5, 2);
    fx1_plic_set_priority(6, 5);
    fx1_plic_set_priority(7, 3);

    /* A: delivery on hart 0 */
    fx1_set_trap_handler(handler);
    fx1_irq_enable(FX1_MIE_MEIE);
    fx1_global_irq_enable();
    fx1_plic_enable(c0, 4, 1);
    line(4, 1);
    fx1_check(wait_for(&claimed[4], 1), 0x10, "A: source 4 delivered to hart 0");
    settle();
    fx1_check(claimed[4] == 1 && hart_claims[0] == 1 && hart_claims[1] == 0, 0x11,
              "A: exactly one claim, on hart 0");
    fx1_check(fx1_plic_pending() == 0 && !(fx1_read_mip() & MIP_MEIP), 0x12,
              "A: nothing pending, MEIP low after complete");
    fx1_log("A: delivery, one claim, no re-entry");

    /* B: priority/threshold/mask/re-pend, polled */
    fx1_global_irq_disable();
    fx1_plic_enable(c0, 5, 1);
    fx1_plic_enable(c0, 6, 1);
    fx1_plic_enable(c0, 7, 1);
    line(5, 1);
    line(6, 1);
    line(7, 1);
    fx1_check(fx1_read_mip() & MIP_MEIP, 0x20, "B: MEIP pending with MIE off");
    const uint32_t first = fx1_plic_claim(c0), second = fx1_plic_claim(c0),
                   third = fx1_plic_claim(c0), none = fx1_plic_claim(c0);
    fx1_check(first == 6 && second == 7 && third == 5 && none == 0, 0x21,
              "B: claims in priority order 6 (5), 7 (3), 5 (2)");
    line(5, 0);
    line(6, 0);
    line(7, 0);
    fx1_plic_complete(c0, 6);
    fx1_plic_complete(c0, 7);
    fx1_plic_complete(c0, 5);
    fx1_check(fx1_plic_pending() == 0 && !(fx1_read_mip() & MIP_MEIP), 0x22, "B: drained");

    /* The threshold gates notification (MEIP) only; claim ignores it. */
    line(6, 1);
    fx1_plic_set_threshold(c0, 5);
    fx1_check(!(fx1_read_mip() & MIP_MEIP), 0x23, "B: threshold 5 masks notification of priority 5");
    fx1_plic_set_threshold(c0, 4);
    fx1_check(fx1_read_mip() & MIP_MEIP, 0x24, "B: threshold 4 notifies priority 5");
    fx1_plic_set_threshold(c0, 7);
    fx1_check(!(fx1_read_mip() & MIP_MEIP) && fx1_plic_claim(c0) == 6, 0x29,
              "B: polled claim at threshold 7 still returns the request");
    line(6, 0);
    fx1_plic_complete(c0, 6);
    fx1_plic_set_threshold(c0, 0);

    fx1_plic_enable(c0, 7, 0);
    line(7, 1);
    fx1_check(!(fx1_read_mip() & MIP_MEIP) && (fx1_plic_pending() & (1u << 7)), 0x25,
              "B: disabled source pends without MEIP");
    fx1_plic_enable(c0, 7, 1);
    fx1_check((fx1_read_mip() & MIP_MEIP) && fx1_plic_claim(c0) == 7, 0x26,
              "B: enabling it raises MEIP");
    fx1_plic_complete(c0, 7); /* line still high */
    fx1_check((fx1_plic_pending() & (1u << 7)) && fx1_plic_claim(c0) == 7, 0x27,
              "B: level source still high pends again after complete");
    line(7, 0);
    fx1_plic_complete(c0, 7);
    fx1_check(fx1_plic_pending() == 0 && !(fx1_read_mip() & MIP_MEIP), 0x28,
              "B: lowered source does not pend again");
    fx1_plic_enable(c0, 5, 0);
    fx1_plic_enable(c0, 6, 0);
    fx1_plic_enable(c0, 7, 0);
    fx1_log("B: priority, threshold, mask and level re-pend");

    /* C: hart 1 routing from WFI, and a shared source claimed once */
    fx1_global_irq_enable();
    fx1_plic_enable(c1, 7, 1);
    line(7, 1);
    fx1_check(wait_for(&claimed[7], 1), 0x30, "C: source 7 woke hart 1");
    fx1_check(hart_claims[1] == 1 && hart_claims[0] == 1, 0x31, "C: taken by hart 1 only");
    fx1_plic_enable(c0, 5, 1);
    fx1_plic_enable(c1, 5, 1);
    line(5, 1);
    fx1_check(wait_for(&claimed[5], 1), 0x32, "C: shared source 5 claimed");
    settle();
    fx1_check(claimed[5] == 1, 0x33, "C: shared source claimed exactly once");
    fx1_log_hex("C: spurious claims (hart0 << 16 | hart1)", (spurious[0] << 16) | spurious[1]);
    fx1_plic_enable(c0, 5, 0);
    fx1_plic_enable(c1, 5, 0);
    fx1_plic_enable(c1, 7, 0);
    fx1_log("C: hart 1 woken from WFI; shared source claimed once");

    /* D: UART TX interrupt (TX FIFO empty) through the PLIC */
    fx1_plic_enable(c0, FX1_IRQ_UART, 1);
    fx1_write32(UART_IMSC, UART_TXIM);
    fx1_check(wait_for(&claimed[FX1_IRQ_UART], 1), 0x40, "D: UART interrupt delivered");
    settle();
    fx1_check(claimed[FX1_IRQ_UART] == 1 && fx1_plic_pending() == 0, 0x41,
              "D: cleared at the device, not pending again");
    fx1_plic_enable(c0, FX1_IRQ_UART, 0);
    fx1_log("D: UART interrupt claimed and cleared at the device");
    return 0;
}
