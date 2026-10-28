// T06 Debug Mode test handler (runs in Debug Mode, see fw_debug.S) and the
// helpers the test code uses to drive it. Each debug entry is recorded in
// fw_dbg; the action depends on fw_dbg.phase, set by the test before it
// causes the entry. Bit 31 of the mailbox `req` word is the debug request
// line (debug_req_i) the harness drives.
#include "fw_common.h"

struct fw_debug_log fw_dbg;
volatile uint32_t fw_trigger_hits;
extern void fw_debug_rom(void);  // fw_debug.S, at the HaltAddress

#define DCSR_EBREAKM (1u << 15)
#define DCSR_EBREAKU (1u << 12)
#define DCSR_STEP (1u << 2)
#define MCONTROL_M (1u << 6)
#define MCONTROL_U (1u << 3)
#define MCONTROL_EXECUTE (1u << 2)

void __attribute__((noinline)) fw_trigger_target(void)
{
    fw_trigger_hits = fw_trigger_hits + 1;
}

void fw_debug_request(void)
{
    const uint32_t entries = fw_dbg.entries;
    fw_box->req = fw_box->req | FW_DEBUG_REQ;
    while (fw_dbg.entries == entries)
        ;
}

static void skip_ebreak(void)
{
    // Continue after the 32-bit ebreak that entered Debug Mode.
    CSR_WRITE(dpc, CSR_READ(dpc) + 4);
}

void fw_debug_handler(void)
{
    const uint32_t dcsr = CSR_READ(dcsr);
    fw_dbg.entries = fw_dbg.entries + 1;
    fw_dbg.cause = (dcsr >> 6) & 7;
    fw_dbg.prv = dcsr & 3;
    fw_dbg.dpc = CSR_READ(dpc);
    fw_dbg.mcause = CSR_READ(mcause);

    if (fw_dbg.cause == 3) {
        // Halt request: release debug_req_i before leaving, or the hart halts again.
        fw_box->req = fw_box->req & ~FW_DEBUG_REQ;
        while (fw_box->ack & FW_DEBUG_REQ)
            ;
    }

    switch (fw_dbg.phase) {
    case FW_DBG_ARM_EBREAKM:
        CSR_SET(dcsr, DCSR_EBREAKM | (fw_dbg.arg ? DCSR_EBREAKU : 0));
        break;
    case FW_DBG_EBREAK:
        skip_ebreak();
        break;
    case FW_DBG_EBREAK_THEN_STEP:
        skip_ebreak();
        CSR_SET(dcsr, DCSR_STEP);
        fw_dbg.phase = FW_DBG_STEP;
        break;
    case FW_DBG_STEP:
        CSR_CLEAR(dcsr, DCSR_STEP);
        break;
    case FW_DBG_EBREAK_THEN_TRIGGER:
        skip_ebreak();
        CSR_WRITE(tselect, fw_dbg.arg);
        CSR_WRITE(tdata2, (uint32_t)(uintptr_t)fw_trigger_target);
        CSR_WRITE(tdata1, MCONTROL_M | MCONTROL_EXECUTE);
        fw_dbg.misc = CSR_READ(tdata1);
        fw_dbg.phase = FW_DBG_TRIGGER;
        break;
    case FW_DBG_TRIGGER:
        fw_dbg.misc = CSR_READ(tdata1);  // hit bit set by the match
        CSR_WRITE(tdata1, 0);
        break;
    case FW_DBG_EBREAK_THEN_EXCEPTION:
        skip_ebreak();
        __asm__ volatile(".word 0");  // illegal in Debug Mode: goes to the +8 entry
        fw_dbg.misc = 0xdead;           // never reached
        break;
    case FW_DBG_EBREAK_THEN_IRQ: {
        // An enabled, pending interrupt is not taken in Debug Mode.
        skip_ebreak();
        const uint32_t before = fw_trap.irq_count;
        fw_box->req = fw_box->req | (1u << 7);
        while (!(fw_box->ack & (1u << 7)))
            ;
        for (volatile unsigned i = 0; i < 200; ++i)
            ;
        fw_dbg.misc = fw_trap.irq_count - before;
        break;
    }
    case FW_DBG_EBREAK_THEN_SCRATCH:
        skip_ebreak();
        CSR_WRITE(dscratch1, 0x5a5aa5a5u);
        __asm__ volatile("wfi");  // a NOP in Debug Mode
        fw_dbg.misc = CSR_READ(dscratch1);
        break;
    default:
        break;
    }
}

void fw_debug_exception(void)
{
    fw_dbg.exceptions = fw_dbg.exceptions + 1;
    fw_dbg.exception_mcause = CSR_READ(mcause);  // not updated in Debug Mode
}

// Debug Mode checks shared by both CPUs (ids 78-99). `triggers` is the number
// of trigger slots of the core.
void fw_debug_checks(uint32_t triggers)
{
    // The debug partition (HaltAddress) answers only Debug Mode accesses:
    // a fetch from M-mode is refused by the bus (instruction access fault).
    EXPECT_TRAP(78, 1, fw_debug_rom());
    fw_check(79, fw_trap.tval == (uint32_t)(uintptr_t)fw_debug_rom, fw_trap.tval);

    // Debug CSRs and DRET exist only in Debug Mode.
    EXPECT_TRAP(80, 2, (void)CSR_READ(dcsr));
    EXPECT_TRAP(81, 2, __asm__ volatile("dret"));

    // Trigger CSRs from M-mode: mcontrol only, tselect WARL, dmode = 1 keeps
    // tdata1 read-only outside Debug Mode.
    fw_check(82, CSR_READ(tinfo) == (1u << 2), CSR_READ(tinfo));
    CSR_WRITE(tselect, 0);
    CSR_WRITE(tselect, triggers);
    const uint32_t kept = CSR_READ(tselect);
    CSR_WRITE(tselect, triggers - 1);
    fw_check(83, kept == 0 && CSR_READ(tselect) == triggers - 1, kept);
    CSR_WRITE(tdata1, MCONTROL_M | MCONTROL_EXECUTE);
    fw_check(84, CSR_READ(tdata1) == 0x28001000u, CSR_READ(tdata1));

    // Halt request: Debug Mode at the next instruction, from M, mcause untouched.
    const uint32_t mcause = CSR_READ(mcause);
    uint32_t entries = fw_dbg.entries;
    fw_dbg.phase = FW_DBG_ARM_EBREAKM;
    fw_dbg.arg = 0;
    fw_debug_request();
    fw_check(85, fw_dbg.entries == entries + 1 && fw_dbg.cause == 3 && fw_dbg.prv == 3 && fw_dbg.dpc != 0,
             fw_dbg.cause);
    fw_check(86, fw_dbg.mcause == mcause && CSR_READ(mcause) == mcause, fw_dbg.mcause);

    // ebreak with dcsr.ebreakm: Debug Mode at the ebreak itself.
    uint32_t at;
    fw_dbg.phase = FW_DBG_EBREAK;
    FW_EBREAK(at);
    fw_check(87, fw_dbg.cause == 1 && fw_dbg.dpc == at, fw_dbg.dpc);
    fw_check(88, fw_dbg.prv == 3, fw_dbg.prv);

    // Single step: after the ebreak, exactly one instruction (a nop) runs.
    entries = fw_dbg.entries;
    fw_dbg.phase = FW_DBG_EBREAK_THEN_STEP;
    __asm__ volatile("la %0, 1f\n1: .word 0x00100073\n .word 0x00000013" : "=r"(at) : : "memory");
    fw_check(89, fw_dbg.cause == 4 && fw_dbg.dpc == at + 8, fw_dbg.dpc);
    fw_check(90, fw_dbg.entries == entries + 2, fw_dbg.entries - entries);

    // Execute trigger (last slot): Debug Mode before fw_trigger_target runs,
    // which then runs once after the resume.
    fw_dbg.phase = FW_DBG_EBREAK_THEN_TRIGGER;
    fw_dbg.arg = triggers - 1;
    FW_EBREAK(at);
    fw_check(91, (fw_dbg.misc & (MCONTROL_M | MCONTROL_EXECUTE)) == (MCONTROL_M | MCONTROL_EXECUTE), fw_dbg.misc);
    const uint32_t hits = fw_trigger_hits;
    fw_trigger_target();
    fw_check(92, fw_dbg.cause == 2 && fw_dbg.dpc == (uint32_t)(uintptr_t)fw_trigger_target, fw_dbg.dpc);
    fw_check(93, fw_trigger_hits == hits + 1, fw_trigger_hits - hits);
    fw_check(94, (fw_dbg.misc & (1u << 20)) != 0, fw_dbg.misc);  // mcontrol.hit

    // An exception in Debug Mode goes to the exception entry and updates no CSR.
    fw_dbg.phase = FW_DBG_EBREAK_THEN_EXCEPTION;
    FW_EBREAK(at);
    fw_check(95, fw_dbg.exceptions == 1, fw_dbg.exceptions);
    fw_check(96, fw_dbg.exception_mcause == mcause && CSR_READ(mcause) == mcause, fw_dbg.exception_mcause);

    // Interrupts are not taken in Debug Mode; the pending one is taken after.
    const uint32_t irqs = fw_trap.irq_count;
    CSR_SET(mie, 1u << 7);
    CSR_SET(mstatus, MSTATUS_MIE);
    fw_dbg.phase = FW_DBG_EBREAK_THEN_IRQ;
    FW_EBREAK(at);
    fw_check(97, fw_dbg.misc == 0, fw_dbg.misc);
    while (fw_trap.irq_count == irqs)
        ;
    fw_check(98, fw_trap.irq_order[irqs < 8 ? irqs : 7] == 7, fw_trap.irq_count);
    CSR_CLEAR(mstatus, MSTATUS_MIE);
    CSR_CLEAR(mie, 1u << 7);

    // dscratch1 and WFI (a NOP) in Debug Mode.
    fw_dbg.phase = FW_DBG_EBREAK_THEN_SCRATCH;
    FW_EBREAK(at);
    fw_check(99, fw_dbg.misc == 0x5a5aa5a5u, fw_dbg.misc);
    fw_dbg.phase = FW_DBG_HALT;
}
