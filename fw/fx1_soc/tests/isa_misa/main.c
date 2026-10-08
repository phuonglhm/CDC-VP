/*
 * G1: the hart implements the FX1 CVA6 extension set RV32IMAFC.
 * misa must report MXL=1 with I, M, A, F, C (and U, S) set and D, V clear;
 * an RV32D or RVV instruction must raise illegal instruction (mcause 2) while
 * single-precision F executes.
 */
#include "fx1_fw.h"

#define MISA_BIT(letter) (1u << ((letter) - 'A'))

static volatile uint32_t illegal_traps;
static volatile uint32_t last_mtval;

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    if (mcause != 2u) {
        fx1_log_hex("unexpected mcause", mcause);
        fx1_fail(0x0E0);
    }
    ++illegal_traps;
    last_mtval = mtval;
    return mepc + 4; /* the probes below are 32-bit encodings */
}

int main(void)
{
    uint32_t misa;
    __asm__ volatile("csrr %0, misa" : "=r"(misa));
    fx1_log_hex("misa", misa);
    fx1_check((misa >> 30) == 1u, 0x01, "MXL is 32-bit");
    const uint32_t required = MISA_BIT('I') | MISA_BIT('M') | MISA_BIT('A') | MISA_BIT('F') |
                              MISA_BIT('C');
    fx1_check((misa & required) == required, 0x02, "I M A F C present");
    fx1_check(!(misa & MISA_BIT('D')), 0x03, "D absent");
    fx1_check(!(misa & MISA_BIT('V')), 0x04, "V absent");

    fx1_set_trap_handler(handler);

    /* fld f0, 0(sp) -- RV32D */
    __asm__ volatile(".word 0x00013007" ::: "memory");
    fx1_check(illegal_traps == 1 && last_mtval == 0x00013007u, 0x05, "fld is illegal");
    /* vsetvli zero, zero, e8, m1, ta, ma -- RVV */
    __asm__ volatile(".word 0x0c007057");
    fx1_check(illegal_traps == 2, 0x06, "vsetvli is illegal");

    /* Single precision works (FS was set to Initial by crt0). */
    volatile float a = 1.5f, b = 2.25f;
    const float sum = a + b;
    fx1_check(sum == 3.75f && illegal_traps == 2, 0x07, "fadd.s executes");
    fx1_log("RV32IMAFC: D and V trap, F executes");
    return 0;
}
