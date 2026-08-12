/* SPDX-License-Identifier: Apache-2.0
 *
 * RV32GCV vector-trap image — decision record D10.
 *
 * Four things, in the order the Phase 2 gate needs them:
 *
 *   1. a fault at element k of a vector load, and correct resumption from
 *      `vstart` afterwards. This is the one that matters: it is the only
 *      observable form of RVV restart state in this backend, because VP++
 *      executes a vector instruction atomically with respect to interrupts
 *      (no interrupt check anywhere in its per-element loop), so the
 *      interrupt-driven restart that plan §11.2 originally asked for cannot
 *      happen at all;
 *   2. `mstatus.VS` — Off makes a vector instruction illegal, and executing one
 *      from Initial or Clean leaves the state Dirty;
 *   3. a reserved `vtype`, which must set `vill` and zero `vl` rather than trap;
 *   4. RV32 indexed access with index EEW=64, which must raise an illegal
 *      instruction.
 *
 * The trap signature written to the exit block is deliberately the shape the
 * Spike differential corpus will compare, so settling it here means the corpus
 * is built against real semantics rather than the other way round.
 */

#include "sim_exit.h"

enum trap_check_id {
    TCHK_NO_TRAP = 1,          /* the armed fault never fired */
    TCHK_MCAUSE = 2,           /* wrong cause for a faulting vector load */
    TCHK_MTVAL = 3,            /* mtval is not the faulting element address */
    TCHK_VSTART_AT_TRAP = 4,   /* vstart did not name the faulting element */
    TCHK_MEPC = 5,             /* mepc is not the vector instruction */
    TCHK_RESUME_DATA = 6,      /* the resumed load produced wrong data */
    TCHK_VSTART_AFTER = 7,     /* vstart not cleared after completion */
    TCHK_TRAP_COUNT = 8,       /* faulted more than once: not a clean resume */
    TCHK_VS_OFF_NOT_ILLEGAL = 9,
    TCHK_VS_NOT_DIRTY = 10,
    TCHK_VILL_NOT_SET = 11,
    TCHK_VILL_VL_NOT_ZERO = 12,
    TCHK_INDEX_EEW64 = 13,
};

#define VLEN_ELEMENTS 16u
#define FAULT_ELEMENT 5u

static unsigned int vsrc[VLEN_ELEMENTS]
    __attribute__((section(".vdata"), aligned(64)));
static unsigned int vdst[VLEN_ELEMENTS]
    __attribute__((section(".vdata"), aligned(64)));

/* Written by the handler, read by main. `volatile`: the handler runs from a
 * trap, which the compiler cannot see as a call. */
static volatile unsigned int trap_count;
static volatile unsigned int trap_mcause;
static volatile unsigned int trap_mepc;
static volatile unsigned int trap_mtval;
static volatile unsigned int trap_vstart;
static volatile unsigned int trap_action;

static inline unsigned int read_csr_vstart(void)
{
    unsigned int v;
    __asm__ volatile("csrr %0, vstart" : "=r"(v));
    return v;
}

static inline unsigned int read_csr_vtype(void)
{
    unsigned int v;
    __asm__ volatile("csrr %0, vtype" : "=r"(v));
    return v;
}

static inline unsigned int read_csr_vl(void)
{
    unsigned int v;
    __asm__ volatile("csrr %0, vl" : "=r"(v));
    return v;
}

static inline unsigned int read_mstatus(void)
{
    unsigned int v;
    __asm__ volatile("csrr %0, mstatus" : "=r"(v));
    return v;
}

static inline void store_word(unsigned int address, unsigned int value)
{
    *(volatile unsigned int *)(unsigned long)address = value;
}

/* mstatus.VS is bits [10:9]. */
#define MSTATUS_VS_SHIFT 9u
#define MSTATUS_VS_MASK (3u << MSTATUS_VS_SHIFT)
#define VS_OFF 0u
#define VS_INITIAL 1u
#define VS_CLEAN 2u
#define VS_DIRTY 3u

static inline unsigned int read_vs(void)
{
    return (read_mstatus() & MSTATUS_VS_MASK) >> MSTATUS_VS_SHIFT;
}

static inline void write_vs(unsigned int state)
{
    __asm__ volatile("csrc mstatus, %0" : : "r"(MSTATUS_VS_MASK));
    __asm__ volatile("csrs mstatus, %0"
                     :
                     : "r"((state << MSTATUS_VS_SHIFT) & MSTATUS_VS_MASK));
}

/* Overrides the weak default in crt0.S. */
unsigned int trap_handler(unsigned int mcause, unsigned int mepc,
                          unsigned int mtval);

unsigned int trap_handler(unsigned int mcause, unsigned int mepc,
                          unsigned int mtval)
{
    trap_count = trap_count + 1u;
    trap_mcause = mcause;
    trap_mepc = mepc;
    trap_mtval = mtval;

    store_word(SIM_TRAP_COUNT, trap_count);
    store_word(SIM_TRAP_MCAUSE, mcause);
    store_word(SIM_TRAP_MEPC, mepc);
    store_word(SIM_TRAP_MTVAL, mtval);

    /* The vector CSRs are only readable when the vector unit is enabled.
     *
     * This is not a detail: with `mstatus.VS = Off` a `csrr vstart` is itself an
     * illegal instruction, so a handler that sampled them unconditionally would
     * trap inside the trap and spin forever — which is exactly what the first
     * version of this file did, on the VS=Off check below. Sampling them only
     * when the unit is on keeps the handler usable for *both* kinds of trap.
     */
    if (read_vs() != VS_OFF) {
        /* Read before returning: this is the value the resumed instruction
         * restarts from, and it is the whole point of the check. */
        trap_vstart = read_csr_vstart();
        store_word(SIM_TRAP_VSTART, trap_vstart);
        store_word(SIM_TRAP_VL, read_csr_vl());
        store_word(SIM_TRAP_VTYPE, read_csr_vtype());
    }

    return trap_action;
}

int main(void)
{
    for (unsigned int i = 0; i < VLEN_ELEMENTS; ++i) {
        vsrc[i] = 0x1000u + i;
        vdst[i] = 0xdeadbeefu;
    }

    /* ── 1. fault at element k, then resume ─────────────────────────────────
     *
     * The armed address is element k of `vsrc`, so the fault lands inside the
     * vector load rather than before it. On resumption the RVV spec requires
     * elements below `vstart` to be left alone; the host counts accesses per
     * address and checks exactly that.
     */
    trap_action = TRAP_ACTION_RESUME;
    trap_count = 0;

    unsigned int vl = 0;
    __asm__ volatile("vsetvli %0, %1, e32, m1, ta, ma"
                     : "=r"(vl)
                     : "r"(VLEN_ELEMENTS));

    store_word(SIM_FAULT_ARM, (unsigned int)(unsigned long)&vsrc[FAULT_ELEMENT]);

    __asm__ volatile(
        "vle32.v v1, (%0)\n\t"
        "vse32.v v1, (%1)\n\t"
        :
        : "r"(vsrc), "r"(vdst)
        : "memory");

    if (trap_count == 0u) {
        return TCHK_NO_TRAP;
    }
    if (trap_count != 1u) {
        return TCHK_TRAP_COUNT;
    }
    /* EXC_LOAD_ACCESS_FAULT. Cause 5, not 13: the harness answers with a TLM
     * error, and decision record D13 makes a failed *bus* access an access
     * fault. A page fault would require address translation to have failed,
     * and this core runs with satp.MODE = Bare. */
    if (trap_mcause != 5u) {
        return TCHK_MCAUSE;
    }
    if (trap_mtval != (unsigned int)(unsigned long)&vsrc[FAULT_ELEMENT]) {
        return TCHK_MTVAL;
    }
    if (trap_vstart != FAULT_ELEMENT) {
        return TCHK_VSTART_AT_TRAP;
    }
    /* mepc must name the faulting vector load, not the instruction after it:
     * a page fault is not a completed instruction. Confirmed by decoding the
     * major opcode at mepc, which is 0x07 for a vector load.
     *
     * Read as a **halfword**. With the C extension enabled a 32-bit vector
     * instruction can sit on a 2-byte boundary — here it lands at 0x196 — and a
     * 32-bit load from there raises a misaligned-load trap of its own. The
     * major opcode lives in the low 7 bits of the first halfword, so 16 bits is
     * both sufficient and always aligned. */
    if ((*(volatile unsigned short *)(unsigned long)trap_mepc & 0x7fu) != 0x07u) {
        return TCHK_MEPC;
    }
    for (unsigned int i = 0; i < VLEN_ELEMENTS; ++i) {
        if (vdst[i] != 0x1000u + i) {
            return TCHK_RESUME_DATA;
        }
    }
    if (read_csr_vstart() != 0u) {
        return TCHK_VSTART_AFTER;
    }

    /* Phase 1 is complete and `vsrc` has not been touched by anything else
     * yet. The host snapshots its read counts here. */
    store_word(SIM_PHASE_MARK, 1u);

    /* ── 2. mstatus.VS ──────────────────────────────────────────────────────
     * With VS=Off a vector instruction is illegal. Skip it rather than resume,
     * or the handler would loop on it forever.
     */
    trap_action = TRAP_ACTION_SKIP;
    trap_count = 0;
    write_vs(VS_OFF);
    __asm__ volatile("vsetvli zero, %0, e32, m1, ta, ma" : : "r"(4u));
    if (trap_count != 1u || trap_mcause != 2u) {  /* illegal instruction */
        write_vs(VS_DIRTY);
        return TCHK_VS_OFF_NOT_ILLEGAL;
    }

    /* From Initial, executing a vector instruction must leave VS Dirty. */
    trap_action = TRAP_ACTION_ABORT;
    write_vs(VS_INITIAL);
    __asm__ volatile("vsetvli zero, %0, e32, m1, ta, ma" : : "r"(VLEN_ELEMENTS));
    __asm__ volatile("vadd.vi v1, v1, 1");
    if (read_vs() != VS_DIRTY) {
        return TCHK_VS_NOT_DIRTY;
    }

    /* ── 3. reserved vtype sets vill, and does not trap ─────────────────────
     * vsew = 0b111 is reserved. The spec requires vill set and vl zeroed, not
     * an exception, so a trap here would itself be the defect.
     */
    trap_action = TRAP_ACTION_ABORT;
    unsigned int bad_vtype = (7u << 3);  /* vsew = 0b111, vlmul = 0 */
    unsigned int got_vl = 0;
    __asm__ volatile("vsetvl %0, %1, %2"
                     : "=r"(got_vl)
                     : "r"(VLEN_ELEMENTS), "r"(bad_vtype));
    if ((read_csr_vtype() >> 31) != 1u) {  /* vill is the top bit */
        return TCHK_VILL_NOT_SET;
    }
    if (got_vl != 0u || read_csr_vl() != 0u) {
        return TCHK_VILL_VL_NOT_ZERO;
    }

    /* Recover from vill before using the vector unit again. */
    __asm__ volatile("vsetvli zero, %0, e32, m1, ta, ma" : : "r"(VLEN_ELEMENTS));

    /* ── 4. RV32 indexed access with index EEW=64 ───────────────────────────
     *
     * Asserted now, not merely recorded. When this image was written it was
     * unclear whether the instruction had to be illegal, so the probe published
     * the outcome and left the verdict to the Spike differential run. That run
     * settled it: RVV 1.0 §18.2 states the V extension does not support EEW=64
     * for index values when XLEN=32, and §7.3 requires an illegal-instruction
     * exception for an unsupported offset EEW. Spike raises it; VP++ did not,
     * which became audit finding F12 and then the D12 conformance patch.
     *
     * The published words stay, because the differential corpus compares them
     * on both models.
     */
    trap_action = TRAP_ACTION_SKIP;
    trap_count = 0;
    __asm__ volatile("vluxei64.v v8, (%0), v16" : : "r"(vsrc) : "memory");
    store_word(SIM_EEW64_TRAPPED, trap_count != 0u ? 1u : 0u);
    store_word(SIM_EEW64_MCAUSE, trap_count != 0u ? trap_mcause : 0u);
    if (trap_count != 1u || trap_mcause != 2u) {  /* illegal instruction */
        return TCHK_INDEX_EEW64;
    }

    trap_action = TRAP_ACTION_ABORT;
    return SIM_EXIT_PASS;
}
