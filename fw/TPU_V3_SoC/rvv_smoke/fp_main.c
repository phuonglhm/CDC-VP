/* SPDX-License-Identifier: Apache-2.0
 *
 * F5 concurrency probe — two harts doing vector FP with *different* rounding
 * modes, interleaved.
 *
 * Berkeley SoftFloat keeps its rounding mode, tininess rule and exception flags
 * in three process-global variables. They are declared `THREAD_LOCAL`, but the
 * macro expands to nothing (nothing defines it), and SystemC processes are
 * coroutines on one OS thread anyway — so every ISS instance in the process
 * shares them. `v.h::set_fp_rm()` writes the rounding mode from `fcsr.frm`, and
 * it is called from only two places, so any FP path that does *not* call it
 * inherits whatever the last instruction on any hart left behind.
 *
 * This image is the evidence for whether that actually leaks. Both harts run
 * the same loop with rounding modes that produce *different* answers for the
 * same operands, so a leak shows up as one hart computing the other's result.
 *
 * The operands are chosen so the two modes cannot agree:
 *
 *     1.0f + 1.5 * 2^-24
 *
 * `ulp(1.0f)` is 2^-23, so the exact sum sits 0.75 ulp above 1.0:
 *
 *     round-to-nearest-even  -> 1.0 + 1 ulp = 0x3f800001
 *     round-toward-zero      -> 1.0         = 0x3f800000
 *
 * Both set the inexact flag, so `fflags` is checked too: a hart seeing extra
 * bits has been given another hart's accumulated flags.
 */

#include "sim_exit.h"

enum fp_check_id {
    FCHK_WRONG_RESULT = 1,  /* this hart computed the other mode's answer */
    FCHK_WRONG_FLAGS = 2,   /* fflags carried bits this hart did not raise */
    FCHK_FRM_READBACK = 3,  /* frm did not stay as this hart set it */
};

#define ITERATIONS 200u

#define FRM_RNE 0u
#define FRM_RTZ 1u

#define F32_ONE 0x3f800000u
#define F32_TINY 0x33c00000u        /* 1.5 * 2^-24 */
#define F32_SUM_RNE 0x3f800001u
#define F32_SUM_RTZ 0x3f800000u

#define FFLAGS_NX 0x01u             /* inexact */

static unsigned int opa[4] __attribute__((section(".vdata"), aligned(64)));
static unsigned int opb[4] __attribute__((section(".vdata"), aligned(64)));
static unsigned int res[4] __attribute__((section(".vdata"), aligned(64)));

static inline unsigned int read_mhartid(void)
{
    unsigned int v;
    __asm__ volatile("csrr %0, mhartid" : "=r"(v));
    return v;
}

static inline void write_frm(unsigned int frm)
{
    __asm__ volatile("csrw frm, %0" : : "r"(frm));
}

static inline unsigned int read_frm(void)
{
    unsigned int v;
    __asm__ volatile("csrr %0, frm" : "=r"(v));
    return v;
}

static inline void clear_fflags(void)
{
    __asm__ volatile("csrw fflags, zero");
}

static inline unsigned int read_fflags(void)
{
    unsigned int v;
    __asm__ volatile("csrr %0, fflags" : "=r"(v));
    return v;
}

static inline void store_word(unsigned int address, unsigned int value)
{
    *(volatile unsigned int *)(unsigned long)address = value;
}

int main(void)
{
    const unsigned int hart = read_mhartid();

    /* Hart 0 truncates, hart 1 rounds to nearest. The harness swaps which core
     * is created — and therefore scheduled — first, so both orders are covered
     * without the image knowing. */
    const unsigned int frm = (hart == 0u) ? FRM_RTZ : FRM_RNE;
    const unsigned int expected =
        (frm == FRM_RTZ) ? F32_SUM_RTZ : F32_SUM_RNE;

    for (unsigned int i = 0; i < 4u; ++i) {
        opa[i] = F32_ONE;
        opb[i] = F32_TINY;
        res[i] = 0u;
    }

    unsigned int vl = 0;
    __asm__ volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(4u));

    unsigned int result_mismatches = 0;
    unsigned int flag_mismatches = 0;
    unsigned int frm_mismatches = 0;
    unsigned int first_bad_value = 0;
    unsigned int first_bad_flags = 0;

    for (unsigned int iteration = 0; iteration < ITERATIONS; ++iteration) {
        /* Set the mode every iteration. A hart that only set it once would be
         * testing its own discipline rather than the ISS's isolation, and the
         * question here is whether the *other* hart can overwrite it between
         * this hart's instructions. */
        write_frm(frm);
        clear_fflags();

        __asm__ volatile(
            "vle32.v v1, (%0)\n\t"
            "vle32.v v2, (%1)\n\t"
            "vfadd.vv v3, v1, v2\n\t"
            "vse32.v v3, (%2)\n\t"
            :
            : "r"(opa), "r"(opb), "r"(res)
            : "memory");

        const unsigned int flags = read_fflags();

        if (read_frm() != frm) {
            if (frm_mismatches == 0u) {
                first_bad_value = read_frm();
            }
            ++frm_mismatches;
        }

        for (unsigned int i = 0; i < 4u; ++i) {
            if (res[i] != expected) {
                if (result_mismatches == 0u) {
                    first_bad_value = res[i];
                }
                ++result_mismatches;
            }
        }

        if (flags != FFLAGS_NX) {
            if (flag_mismatches == 0u) {
                first_bad_flags = flags;
            }
            ++flag_mismatches;
        }
    }

    store_word(SIM_FP_HART, hart);
    store_word(SIM_FP_FRM, frm);
    store_word(SIM_FP_EXPECTED, expected);
    store_word(SIM_FP_ITERATIONS, ITERATIONS);
    store_word(SIM_FP_RESULT_MISMATCHES, result_mismatches);
    store_word(SIM_FP_FLAG_MISMATCHES, flag_mismatches);
    store_word(SIM_FP_FRM_MISMATCHES, frm_mismatches);
    store_word(SIM_FP_FIRST_BAD_VALUE, first_bad_value);
    store_word(SIM_FP_FIRST_BAD_FLAGS, first_bad_flags);
    store_word(SIM_FP_LAST_RESULT, res[0]);

    if (result_mismatches != 0u) {
        return FCHK_WRONG_RESULT;
    }
    if (flag_mismatches != 0u) {
        return FCHK_WRONG_FLAGS;
    }
    if (frm_mismatches != 0u) {
        return FCHK_FRM_READBACK;
    }
    return SIM_EXIT_PASS;
}
