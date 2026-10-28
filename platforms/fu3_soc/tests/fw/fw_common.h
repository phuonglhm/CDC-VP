// Shared runtime of the T05 CPU test firmware (CVA6 and Ibex).
//
// The test mailbox in the top of DSRAM is a convention between this firmware
// and tests/test_fu3_soc_cpu.cpp only; it is not a product ABI. The harness
// stands in for interrupt sources that are not integrated yet (T08/T09): the
// firmware sets a bit in `req` to ask for interrupt cause n to be asserted and
// clears it to deassert; `ack` mirrors the lines the harness currently drives.
#pragma once

#include <stdint.h>

#define FW_PASS 0x600dc0deu
#define FW_FAIL 0xbad00000u

struct fw_mailbox {
    volatile uint32_t status;     // 0 running, FW_PASS, or FW_FAIL | first failed check
    volatile uint32_t checks;     // checks executed
    volatile uint32_t failed;     // first failed check id (0: none)
    volatile uint32_t detail;     // value observed by the first failed check
    volatile uint32_t req;        // interrupt causes the firmware asks to be asserted
    volatile uint32_t ack;        // interrupt causes the harness asserts now
    volatile uint32_t reset_info[4]; // reset-state captures (Ibex ROM stub)
    // First exception nobody declared: the run stops there (status FW_FAIL | FW_UNEXPECTED).
    volatile uint32_t unexpected_cause;
    volatile uint32_t unexpected_epc;
    volatile uint32_t unexpected_tval;
};

#define FW_UNEXPECTED 0xeee
#define FW_NO_TRAP 0xffffffffu

#define FW_MAILBOX_CVA6 ((struct fw_mailbox *)0x0103ff00u)
#define FW_MAILBOX_IBEX ((struct fw_mailbox *)0x0103ff40u)

extern struct fw_mailbox *const fw_box;

// What the trap handler saw last; tests compare against it.
struct fw_trap_log {
    volatile uint32_t count;   // exceptions taken
    volatile uint32_t cause;
    volatile uint32_t epc;
    volatile uint32_t tval;
    volatile uint32_t irq_count;
    volatile uint32_t irq_order[8];
    volatile uint32_t vector;  // mscratch value at handler entry (set by distinct vector entries)
    volatile uint32_t expect;  // cause the code under test declared (FW_NO_TRAP: none)
};
extern struct fw_trap_log fw_trap;

// Interrupt service of a real platform source (T09): when set, the trap
// handler calls it for every interrupt instead of the mailbox request protocol;
// it must remove the cause (acknowledge the device) before returning.
extern void (*volatile fw_irq_hook)(uint32_t cause);

// Where an exception from U-mode `ecall` returns to (M-mode).
extern volatile uint32_t fw_umode_return;

void fw_check(uint32_t id, int ok, uint32_t detail);
void fw_finish(void);
// Ask for interrupt cause `cause` and wait until the harness drives it (or,
// when enabled, until an interrupt has been serviced).
void fw_raise(uint32_t cause);
// Run `fn` in U-mode; it must end with `ecall`. Returns after that ecall.
// The caller declares how U-mode is left (cause 8, or 1 if fn cannot be fetched).
void fw_run_user(void (*fn)(void));
#define RUN_USER(exit_cause, fn)                         \
    do {                                                 \
        const uint32_t outer_ = fw_trap.expect;          \
        fw_trap.expect = (exit_cause);                   \
        fw_run_user(fn);                                 \
        fw_trap.expect = outer_;                         \
    } while (0)

#define CSR_READ(csr)                                                \
    ({                                                               \
        uint32_t v_;                                                 \
        __asm__ volatile("csrr %0, " #csr : "=r"(v_) : : "memory"); \
        v_;                                                          \
    })
#define CSR_WRITE(csr, v) __asm__ volatile("csrw " #csr ", %0" : : "r"((uint32_t)(v)) : "memory")
#define CSR_SET(csr, v) __asm__ volatile("csrs " #csr ", %0" : : "r"((uint32_t)(v)) : "memory")
#define CSR_CLEAR(csr, v) __asm__ volatile("csrc " #csr ", %0" : : "r"((uint32_t)(v)) : "memory")

// Execute the statement and check that it trapped exactly once with `cause`.
// Only a declared cause is accepted by the trap handler; any other exception
// ends the run as FW_UNEXPECTED. The previous expectation is restored, so
// declarations nest (U-mode code run under fw_run_user).
#define EXPECT_TRAP(id, cause_, ...)                                                   \
    do {                                                                               \
        const uint32_t outer_ = fw_trap.expect;                                        \
        const uint32_t before_ = fw_trap.count;                                        \
        fw_trap.expect = (cause_);                                                     \
        __VA_ARGS__;                                                                   \
        fw_trap.expect = outer_;                                                       \
        fw_check((id), fw_trap.count == before_ + 1 && fw_trap.cause == (cause_),      \
                 fw_trap.count == before_ ? FW_NO_TRAP : fw_trap.cause);               \
    } while (0)

// T06 Debug Mode tests (fw_debug.S/.c). Bit 31 of `req` drives debug_req_i.
#define FW_DEBUG_REQ (1u << 31)

enum fw_debug_phase {
    FW_DBG_HALT = 1,             // record the entry only
    FW_DBG_ARM_EBREAKM,          // set dcsr.ebreakm (and ebreaku if arg)
    FW_DBG_EBREAK,               // ebreak entered Debug Mode: continue after it
    FW_DBG_EBREAK_THEN_STEP,     // continue after the ebreak, single-step one instruction
    FW_DBG_STEP,                 // step entry: clear dcsr.step
    FW_DBG_EBREAK_THEN_TRIGGER,  // program trigger `arg` on fw_trigger_target
    FW_DBG_TRIGGER,              // trigger entry: disable it
    FW_DBG_EBREAK_THEN_EXCEPTION,// take an exception inside Debug Mode
    FW_DBG_EBREAK_THEN_IRQ,      // assert an enabled interrupt while in Debug Mode
    FW_DBG_EBREAK_THEN_SCRATCH,  // dscratch1 round trip and WFI in Debug Mode
};

struct fw_debug_log {
    volatile uint32_t phase, arg;
    volatile uint32_t entries, cause, prv, dpc, mcause;
    volatile uint32_t exceptions, exception_mcause, misc;
};
extern struct fw_debug_log fw_dbg;
extern volatile uint32_t fw_trigger_hits;
void fw_trigger_target(void);
// Raise debug_req_i and wait until the hart has been through Debug Mode.
void fw_debug_request(void);
// Debug Mode checks shared by both CPUs (ids 80-99).
void fw_debug_checks(uint32_t triggers);
// Execute a 32-bit ebreak at a known address (returned) with fw_dbg.phase set.
#define FW_EBREAK(addr_out)                                                         \
    __asm__ volatile("la %0, 1f\n1: .word 0x00100073" : "=r"(addr_out) : : "memory")

#define MSTATUS_MIE (1u << 3)
#define MSTATUS_MPP (3u << 11)
#define MSTATUS_FS (3u << 13)
#define MSTATUS_MPRV (1u << 17)
