/* SPDX-License-Identifier: Apache-2.0
 *
 * Interrupt gate for `cdc::cpu::cpu_base::set_irq()` on the VP++ backend.
 *
 * `set_irq()` was implemented and never executed. Plan §11.2 asks for machine
 * software, timer and external interrupts to be delivered and cleared, so this
 * image is the target half: it enables each source in turn, spins until the
 * handler fires, and records what `mcause` said.
 *
 * Level-triggered, and that is the part worth testing. `cpu_base` presents a
 * level to the platform while upstream exposes trigger/clear entry points, so
 * the wrapper translates between them. A translation that only ever asserted
 * would look identical here until the deassert — which is why each phase ends
 * by requiring the interrupt to *stop* firing once the host lowers the line,
 * with the handler counting every entry.
 *
 * The handler masks the source in `mie` and does **not** clear the pending bit.
 * Clearing is the host's job through `set_irq(cause, false)`, so a wrapper that
 * quietly self-cleared on entry is caught rather than hidden.
 *
 * Masking is not optional here, and the reason is the whole shape of the test.
 * The line is level-triggered and only the host can lower it, but the host only
 * lowers it when this firmware asks — and a handler that returned with the
 * source still pending and `mstatus.MIE` restored would re-enter immediately,
 * forever, never reaching the instruction that does the asking. The first
 * version of this file did exactly that and spun in `trap_entry`. Masking the
 * one source lets the foreground make progress while leaving the pending bit
 * untouched for the deassert check to inspect.
 */

#include "sim_exit.h"

enum irq_check_id {
    ICHK_SOFTWARE_NOT_TAKEN = 1,
    ICHK_SOFTWARE_WRONG_CAUSE = 2,
    ICHK_TIMER_NOT_TAKEN = 3,
    ICHK_TIMER_WRONG_CAUSE = 4,
    ICHK_EXTERNAL_NOT_TAKEN = 5,
    ICHK_EXTERNAL_WRONG_CAUSE = 6,
    ICHK_NOT_CLEARED = 7,
    ICHK_DISABLED_STILL_FIRED = 8,
    ICHK_SELF_CLEARED = 9,
};

/* mie / mip bit positions. */
#define MIE_MSIE (1u << 3)
#define MIE_MTIE (1u << 7)
#define MIE_MEIE (1u << 11)
#define MSTATUS_MIE (1u << 3)

/* mcause values, with the interrupt bit set. */
#define IRQ_SOFTWARE 0x80000003u
#define IRQ_TIMER    0x80000007u
#define IRQ_EXTERNAL 0x8000000bu

/* How long to spin waiting for an interrupt that should arrive, and how long to
 * spin proving one does not. Both are instruction counts, not time: the harness
 * drives the lines from a SystemC process, and the firmware only needs to yield
 * enough quanta for that process to run. */
/* Each iteration performs four bus accesses, so it yields to the harness every
 * time round; a few hundred is already generous. The first version used
 * 200000/20000 and simply ran the 20 ms simulation window out, which presents
 * as a hang at an arbitrary PC rather than as a missed interrupt. */
#define WAIT_ITERATIONS 2000u
#define QUIET_ITERATIONS 200u

static volatile unsigned int irq_count;
static volatile unsigned int irq_mcause;
static volatile unsigned int total_irqs;

static inline void store_word(unsigned int address, unsigned int value)
{
    *(volatile unsigned int *)(unsigned long)address = value;
}

static inline unsigned int load_word(unsigned int address)
{
    return *(volatile unsigned int *)(unsigned long)address;
}

#define CSR_READ(name)                                                        \
    ({                                                                        \
        unsigned int v_;                                                      \
        __asm__ volatile("csrr %0, " #name : "=r"(v_));                       \
        v_;                                                                   \
    })

#define CSR_SET(name, value)                                                  \
    do {                                                                      \
        __asm__ volatile("csrs " #name ", %0" : : "r"(value));                \
    } while (0)

#define CSR_CLEAR(name, value)                                                \
    do {                                                                      \
        __asm__ volatile("csrc " #name ", %0" : : "r"(value));                \
    } while (0)

/* Overrides the weak default in crt0.S. An interrupt's `mepc` is the
 * instruction that would have run next, so resuming without touching it is
 * correct — skipping 4 would step over an instruction that never executed. */
unsigned int trap_handler(unsigned int mcause, unsigned int mepc,
                          unsigned int mtval);

unsigned int trap_handler(unsigned int mcause, unsigned int mepc,
                          unsigned int mtval)
{
    (void)mepc;
    (void)mtval;
    ++irq_count;
    ++total_irqs;
    irq_mcause = mcause;

    /* Mask this source only. `mip` is deliberately left alone: whether the
     * pending bit goes away is what `set_irq(cause, false)` is being tested
     * for. */
    if ((mcause & 0x80000000u) != 0u) {
        __asm__ volatile("csrc mie, %0" : : "r"(1u << (mcause & 0x1fu)));
    }
    return TRAP_ACTION_RESUME;
}

/* Spins until the handler has fired, or gives up. Returns the number of
 * interrupts seen. The loop body writes a harness-visible word so that each
 * iteration produces a bus access: the host drives the interrupt lines from a
 * SystemC process, which only runs when this one yields, and a pure register
 * spin under temporal decoupling would never let it. */
static unsigned int wait_for_irq(unsigned int line, unsigned int iterations)
{
    for (unsigned int i = 0; i < iterations; ++i) {
        store_word(SIM_IRQ_HEARTBEAT, line);
        store_word(SIM_IRQ_MSTATUS, CSR_READ(mstatus));
        store_word(SIM_IRQ_MIE, CSR_READ(mie));
        store_word(SIM_IRQ_MIP, CSR_READ(mip));
        if (irq_count != 0u) {
            break;
        }
    }
    return irq_count;
}

static int run_line(unsigned int line, unsigned int enable_bit,
                    unsigned int expected_cause, unsigned int mcause_address,
                    unsigned int count_address, int not_taken, int wrong_cause)
{
    irq_count = 0;
    irq_mcause = 0;

    CSR_SET(mie, enable_bit);
    /* Ask the host to raise this line, then spin until it arrives. */
    store_word(SIM_IRQ_REQUEST, line);
    const unsigned int seen = wait_for_irq(line, WAIT_ITERATIONS);
    store_word(count_address, seen);
    store_word(mcause_address, irq_mcause);

    if (seen == 0u) {
        return not_taken;
    }
    if (irq_mcause != expected_cause) {
        return wrong_cause;
    }

    /* The handler masked this source. Before lowering the line, confirm the
     * pending bit is still set: that is what distinguishes "the host has not
     * lowered it yet" from "the wrapper cleared it behind our back". */
    store_word(SIM_IRQ_MIP, CSR_READ(mip));
    if ((CSR_READ(mip) & enable_bit) == 0u) {
        return ICHK_SELF_CLEARED;
    }

    /* Ask the host to lower it and wait for the acknowledgement, then unmask
     * and prove it stays quiet. A line the wrapper failed to deassert would
     * fire again the moment the source is unmasked. */
    store_word(SIM_IRQ_REQUEST, SIM_IRQ_NONE);
    while (load_word(SIM_IRQ_ACK) != SIM_IRQ_NONE) {
        store_word(SIM_IRQ_HEARTBEAT, line);
    }
    if ((CSR_READ(mip) & enable_bit) != 0u) {
        return ICHK_NOT_CLEARED;
    }

    irq_count = 0;
    CSR_SET(mie, enable_bit);
    (void)wait_for_irq(line, QUIET_ITERATIONS);
    if (irq_count != 0u) {
        return ICHK_NOT_CLEARED;
    }

    CSR_CLEAR(mie, enable_bit);
    return 0;
}

int main(void)
{
    store_word(SIM_IRQ_REQUEST, SIM_IRQ_NONE);

    /* Interrupts are only taken with mstatus.MIE set; everything below depends
     * on that, so a failure here would otherwise look like a dead line. */
    CSR_SET(mstatus, MSTATUS_MIE);

    int status = run_line(SIM_IRQ_LINE_SOFTWARE, MIE_MSIE, IRQ_SOFTWARE,
                          SIM_IRQ_SOFTWARE_MCAUSE, SIM_IRQ_SOFTWARE_COUNT,
                          ICHK_SOFTWARE_NOT_TAKEN, ICHK_SOFTWARE_WRONG_CAUSE);
    if (status != 0) {
        return status;
    }

    status = run_line(SIM_IRQ_LINE_TIMER, MIE_MTIE, IRQ_TIMER,
                      SIM_IRQ_TIMER_MCAUSE, SIM_IRQ_TIMER_COUNT,
                      ICHK_TIMER_NOT_TAKEN, ICHK_TIMER_WRONG_CAUSE);
    if (status != 0) {
        return status;
    }

    status = run_line(SIM_IRQ_LINE_EXTERNAL, MIE_MEIE, IRQ_EXTERNAL,
                      SIM_IRQ_EXTERNAL_MCAUSE, SIM_IRQ_EXTERNAL_COUNT,
                      ICHK_EXTERNAL_NOT_TAKEN, ICHK_EXTERNAL_WRONG_CAUSE);
    if (status != 0) {
        return status;
    }

    /* With every enable bit cleared, a raised line must not be taken. Without
     * this the three checks above would also pass on a model that ignored `mie`
     * entirely and delivered whatever the host asserted. */
    CSR_CLEAR(mie, MIE_MSIE | MIE_MTIE | MIE_MEIE);
    irq_count = 0;
    store_word(SIM_IRQ_REQUEST, SIM_IRQ_LINE_SOFTWARE);
    (void)wait_for_irq(SIM_IRQ_LINE_SOFTWARE, QUIET_ITERATIONS);
    store_word(SIM_IRQ_MASKED_COUNT, irq_count);
    store_word(SIM_IRQ_REQUEST, SIM_IRQ_NONE);
    if (irq_count != 0u) {
        return ICHK_DISABLED_STILL_FIRED;
    }

    store_word(SIM_IRQ_TOTAL, total_irqs);
    return SIM_EXIT_PASS;
}
