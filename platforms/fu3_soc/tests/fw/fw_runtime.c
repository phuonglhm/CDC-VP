// Trap handling, check bookkeeping and the interrupt request protocol shared
// by the CVA6 and Ibex test firmware (see fw_common.h).
#include "fw_common.h"

struct fw_trap_log fw_trap = {.expect = FW_NO_TRAP};
volatile uint32_t fw_umode_return;
void (*volatile fw_irq_hook)(uint32_t cause);

void fw_check(uint32_t id, int ok, uint32_t detail)
{
    fw_box->checks = fw_box->checks + 1;
    if (!ok && !fw_box->failed) {
        fw_box->failed = id;
        fw_box->detail = detail;
    }
}

void fw_finish(void)
{
    fw_box->status = fw_box->failed ? (FW_FAIL | fw_box->failed) : FW_PASS;
    for (;;)
        __asm__ volatile("wfi");
}

void fw_raise(uint32_t cause)
{
    // With the interrupt enabled, the handler may take it, drop the request
    // and see ack fall before this loop reads ack high: a serviced interrupt
    // ends the wait as well.
    const uint32_t serviced = fw_trap.irq_count;
    fw_box->req = fw_box->req | (1u << cause);
    while (!(fw_box->ack & (1u << cause)) && fw_trap.irq_count == serviced)
        ;
}

// Called from fw_trap_entry with the saved register frame (x[i] at frame[i]).
// Returns the address to resume at.
uint32_t fw_trap_handler(uint32_t *frame)
{
    const uint32_t mcause = CSR_READ(mcause);
    uint32_t epc = CSR_READ(mepc);
    fw_trap.vector = CSR_READ(mscratch);
    CSR_WRITE(mscratch, 0);

    if (mcause & 0x80000000u) {
        const uint32_t cause = mcause & 0x7fffffffu;
        if (fw_trap.irq_count < 8)
            fw_trap.irq_order[fw_trap.irq_count] = cause;
        fw_trap.irq_count = fw_trap.irq_count + 1;
        if (fw_irq_hook) {
            fw_irq_hook(cause);
            return epc;
        }
        // Service: drop the request and wait until the line is low, so the
        // same interrupt is not taken again on return.
        fw_box->req = fw_box->req & ~(1u << cause);
        while (fw_box->ack & (1u << cause))
            ;
        return epc;
    }

    fw_trap.count = fw_trap.count + 1;
    fw_trap.cause = mcause;
    fw_trap.epc = epc;
    fw_trap.tval = CSR_READ(mtval);

    if (mcause != fw_trap.expect) {
        // Nobody declared this exception: a legal instruction trapped, or an
        // illegal one did something else. Stop here with the evidence.
        fw_box->unexpected_cause = mcause;
        fw_box->unexpected_epc = epc;
        fw_box->unexpected_tval = fw_trap.tval;
        if (!fw_box->failed) {
            fw_box->failed = FW_UNEXPECTED;
            fw_box->detail = mcause;
        }
        fw_box->status = FW_FAIL | FW_UNEXPECTED;
        for (;;)
            ;
    }

    const int from_user = (CSR_READ(mstatus) & MSTATUS_MPP) == 0;
    if (mcause == 8 || (mcause == 1 && from_user)) {
        // U-mode ecall, or U-mode code that cannot even be fetched: back to
        // M-mode at the continuation fw_run_user left.
        CSR_SET(mstatus, MSTATUS_MPP);
        return fw_umode_return;
    }
    if (mcause == 1) {
        // Instruction fetch fault: epc is not readable code; return to the caller.
        return frame[1];
    }
    // Skip the trapping instruction (16 or 32 bit).
    const uint16_t low = *(volatile uint16_t *)(uintptr_t)epc;
    return epc + ((low & 3) == 3 ? 4 : 2);
}
