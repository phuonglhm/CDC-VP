/*
 * G2 / plan C4: bus errors become architectural access faults.
 *   fetch from an unmapped address  -> mcause 1, mepc = mtval = target
 *   load from an unmapped address   -> mcause 5, mepc = the load, mtval = address
 *   store to an unmapped address    -> mcause 7, mepc = the store, mtval = address
 *   store to read-only BootROM      -> mcause 7, and the ROM word is unchanged
 * The handler resumes at a recovery address recorded before each probe.
 */
#include "fx1_fw.h"

#define UNMAPPED 0x60000000u
#define ROM_WORD (FX1_BOOTROM_BASE + 0x100u)

static volatile uint32_t got_cause, got_epc, got_tval, traps;
static volatile uint32_t resume_at;

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    got_cause = mcause;
    got_epc = mepc;
    got_tval = mtval;
    ++traps;
    return resume_at;
}

static void expect_trap(uint32_t n, uint32_t cause, uint32_t epc, uint32_t tval, uint32_t code,
                        const char* what)
{
    if (traps != n || got_cause != cause || got_epc != epc || got_tval != tval) {
        fx1_log_hex("  mcause", got_cause);
        fx1_log_hex("  mepc  ", got_epc);
        fx1_log_hex("  mtval ", got_tval);
        fx1_check(0, code, what);
    }
    fx1_log(what);
}

int main(void)
{
    fx1_set_trap_handler(handler);
    uint32_t probe = 0, value = 0;

    /* fetch: jump to UNMAPPED, resume at 1: */
    __asm__ volatile("la   t0, 1f\n"
                     "sw   t0, 0(%0)\n"
                     "jalr zero, 0(%1)\n"
                     "1:\n"
                     :
                     : "r"(&resume_at), "r"(UNMAPPED)
                     : "t0", "memory");
    expect_trap(1, 1, UNMAPPED, UNMAPPED, 0x01, "fetch fault: mcause 1, mepc = mtval = target");

    /* load */
    __asm__ volatile("la   t0, 2f\n"
                     "sw   t0, 0(%2)\n"
                     "la   %0, 3f\n"
                     "3: lw %1, 0(%3)\n"
                     "2:\n"
                     : "=&r"(probe), "=&r"(value)
                     : "r"(&resume_at), "r"(UNMAPPED)
                     : "t0", "memory");
    expect_trap(2, 5, probe, UNMAPPED, 0x02, "load fault: mcause 5, mepc = the load");

    /* store, unmapped */
    __asm__ volatile("la   t0, 4f\n"
                     "sw   t0, 0(%1)\n"
                     "la   %0, 5f\n"
                     "5: sw zero, 0(%2)\n"
                     "4:\n"
                     : "=&r"(probe)
                     : "r"(&resume_at), "r"(UNMAPPED)
                     : "t0", "memory");
    expect_trap(3, 7, probe, UNMAPPED, 0x03, "store fault: mcause 7, mepc = the store");

    /* store to read-only BootROM: slave error -> store access fault, ROM unchanged */
    const uint32_t before = fx1_read32(ROM_WORD);
    __asm__ volatile("la   t0, 6f\n"
                     "sw   t0, 0(%1)\n"
                     "la   %0, 7f\n"
                     "7: sw %3, 0(%2)\n"
                     "6:\n"
                     : "=&r"(probe)
                     : "r"(&resume_at), "r"(ROM_WORD), "r"(0xDEADBEEFu)
                     : "t0", "memory");
    expect_trap(4, 7, probe, ROM_WORD, 0x04, "ROM store fault: mcause 7");
    fx1_check(fx1_read32(ROM_WORD) == before, 0x05, "failed store left the ROM unchanged");
    (void)value;
    return 0;
}
