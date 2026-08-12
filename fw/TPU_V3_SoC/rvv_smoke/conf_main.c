/* SPDX-License-Identifier: Apache-2.0
 *
 * Gate for the two downstream conformance patches, D12 and D13.
 *
 * Both fix deviations that the Spike differential corpus found (audit F12 and
 * F13). The corpus proves the *result* matches Spike; this image proves the
 * properties Spike cannot show us, because they are about what the model does
 * on the way to that result:
 *
 *   D12 — all 32 RV32 indexed encodings with index EEW=64 raise an illegal
 *         instruction, and raise it early enough that no load/store is counted,
 *         no bus request is issued, `vstart` is untouched, the destination
 *         register is untouched, and `mstatus.VS` is not dirtied. A check
 *         placed inside `vLoadStore()` would match Spike's `mcause` and still
 *         fail every one of those, because `prepInstr()` dirties VS first.
 *
 *   D13 — a failed bus access reports an access fault whose cause follows the
 *         *origin* of the access. All six origins are exercised: instruction
 *         fetch, scalar load, scalar store, vector load, vector store and AMO.
 *         Deriving the cause from the TLM command alone would get fetch and AMO
 *         wrong while looking correct for the other four.
 */

#include "sim_exit.h"

enum conf_check_id {
    CCHK_D12_NOT_ALL_TRAPPED = 1,
    CCHK_D12_WRONG_MCAUSE = 2,
    CCHK_D12_VSTART_CLOBBERED = 3,
    CCHK_D12_VD_CLOBBERED = 4,
    CCHK_D12_VS_DIRTIED = 5,
    CCHK_D13_FETCH = 10,
    CCHK_D13_LOAD = 11,
    CCHK_D13_STORE = 12,
    CCHK_D13_VLOAD = 13,
    CCHK_D13_VSTORE = 14,
    CCHK_D13_AMO = 15,
    CCHK_D13_GENERIC = 16,
};

#define ELEMENTS 16u

/* Well outside the harness's probe memory, for the probes that only need *an*
 * unmapped address. The harness asserts it never answered an access here. */
#define UNMAPPED_BASE 0x00400000u

/* A second unmapped address, which the harness refuses with
 * TLM_GENERIC_ERROR_RESPONSE instead of TLM_ADDRESS_ERROR_RESPONSE. Both are a
 * target declining the access, so both must produce the same guest access
 * fault. */
#define UNMAPPED_GENERIC 0x00500000u

/* The first address past the end of RAM, from the linker script.
 *
 * The vector probes need the *boundary*, not just any unmapped address: they
 * straddle it so that elements 0..3 succeed and element 4 is the first to fail,
 * which is what makes `vstart` meaningful. The first version of this file used
 * UNMAPPED_BASE here and faulted on element 0. */
extern char __ram_end[];

#define MSTATUS_VS_SHIFT 9u
#define MSTATUS_VS_MASK (3u << MSTATUS_VS_SHIFT)
#define VS_CLEAN 2u
#define VS_DIRTY 3u

#define EXC_INSTR_ACCESS_FAULT 1u
#define EXC_ILLEGAL_INSTR 2u
#define EXC_LOAD_ACCESS_FAULT 5u
#define EXC_STORE_AMO_ACCESS_FAULT 7u

static unsigned int vsrc[ELEMENTS] __attribute__((section(".vdata"), aligned(64)));
static unsigned int vdst[ELEMENTS] __attribute__((section(".vdata"), aligned(64)));
static unsigned int idx[2 * ELEMENTS] __attribute__((section(".vdata"), aligned(64)));

static volatile unsigned int trap_count;
static volatile unsigned int trap_mcause;
static volatile unsigned int trap_mtval;
static volatile unsigned int trap_vstart;
static volatile unsigned int trap_action;
/* Overrides the weak definition in crt0.S; read by the trap entry when the
 * handler returns TRAP_ACTION_RESUME_AT. */
unsigned int trap_resume_pc;
/* The vector-CSR sample in the handler is suppressed for the VS=Clean probe.
 * Reading a vector CSR must not dirty VS, but this gate is measuring exactly
 * whether VS stayed Clean, and it should not have to assume the answer to a
 * neighbouring question to do so. */
static volatile unsigned int sample_vector_csrs = 1;

#define CSR_READ(name)                                                        \
    ({                                                                        \
        unsigned int v_;                                                      \
        __asm__ volatile("csrr %0, " #name : "=r"(v_));                       \
        v_;                                                                   \
    })

#define CSR_WRITE(name, value)                                                \
    do {                                                                      \
        __asm__ volatile("csrw " #name ", %0" : : "r"(value));                \
    } while (0)

static inline void store_word(unsigned int address, unsigned int value)
{
    *(volatile unsigned int *)(unsigned long)address = value;
}

static inline unsigned int read_vs(void)
{
    return (CSR_READ(mstatus) & MSTATUS_VS_MASK) >> MSTATUS_VS_SHIFT;
}

static inline void write_vs(unsigned int state)
{
    __asm__ volatile("csrc mstatus, %0" : : "r"(MSTATUS_VS_MASK));
    __asm__ volatile("csrs mstatus, %0"
                     :
                     : "r"((state << MSTATUS_VS_SHIFT) & MSTATUS_VS_MASK));
}

unsigned int trap_handler(unsigned int mcause, unsigned int mepc,
                          unsigned int mtval);

unsigned int trap_handler(unsigned int mcause, unsigned int mepc,
                          unsigned int mtval)
{
    (void)mepc;
    ++trap_count;
    trap_mcause = mcause;
    trap_mtval = mtval;
    if (sample_vector_csrs && read_vs() != 0u) {
        trap_vstart = CSR_READ(vstart);
    }
    return trap_action;
}

/* ── D12 ─────────────────────────────────────────────────────────────────────
 *
 * All **32** encodings that carry an index EEW, not just the four unit forms:
 * `v[sl][ou]xei64.v` and the 28 segment forms `v[sl][ou]xseg[2-8]ei64.v`. The
 * specification restricts the *index* EEW, so the segment forms are equally
 * illegal at XLEN=32, and a probe that only tried `vluxei64.v` would have
 * declared the model conformant while 28 encodings were still wrong.
 *
 * Each is emitted as a `.word` followed by `ret`, giving a table of 8-byte
 * stubs called by index. `.word` because the point of the gate is that the
 * *model* refuses them, and `ret` because the trap handler skips 4 bytes and
 * has to land on something. `.option norvc` keeps `ret` at 4 bytes so every
 * stub is exactly 8.
 *
 * The stub is called with `vsrc` as its argument, so `a0` holds a real address:
 * with the patch reverted the encodings execute and the accesses land in the
 * window the host is counting, which is what makes the negative control
 * meaningful.
 *
 * The encodings come from the assembler, and `make verify` re-derives them from
 * a disassembly rather than trusting this list.
 */
__asm__(".section .text.d12,\"ax\"\n"
        ".option push\n"
        ".option norvc\n"
        ".balign 8\n"
        ".globl d12_stubs\n"
        "d12_stubs:\n"
    ".word 0x07057407\n    ret\n"   /* vluxei64.v       */
    ".word 0x0f057407\n    ret\n"   /* vloxei64.v       */
    ".word 0x07057427\n    ret\n"   /* vsuxei64.v       */
    ".word 0x0f057427\n    ret\n"   /* vsoxei64.v       */
    ".word 0x27057407\n    ret\n"   /* vluxseg2ei64.v   */
    ".word 0x2f057407\n    ret\n"   /* vloxseg2ei64.v   */
    ".word 0x27057427\n    ret\n"   /* vsuxseg2ei64.v   */
    ".word 0x2f057427\n    ret\n"   /* vsoxseg2ei64.v   */
    ".word 0x47057407\n    ret\n"   /* vluxseg3ei64.v   */
    ".word 0x4f057407\n    ret\n"   /* vloxseg3ei64.v   */
    ".word 0x47057427\n    ret\n"   /* vsuxseg3ei64.v   */
    ".word 0x4f057427\n    ret\n"   /* vsoxseg3ei64.v   */
    ".word 0x67057407\n    ret\n"   /* vluxseg4ei64.v   */
    ".word 0x6f057407\n    ret\n"   /* vloxseg4ei64.v   */
    ".word 0x67057427\n    ret\n"   /* vsuxseg4ei64.v   */
    ".word 0x6f057427\n    ret\n"   /* vsoxseg4ei64.v   */
    ".word 0x87057407\n    ret\n"   /* vluxseg5ei64.v   */
    ".word 0x8f057407\n    ret\n"   /* vloxseg5ei64.v   */
    ".word 0x87057427\n    ret\n"   /* vsuxseg5ei64.v   */
    ".word 0x8f057427\n    ret\n"   /* vsoxseg5ei64.v   */
    ".word 0xa7057407\n    ret\n"   /* vluxseg6ei64.v   */
    ".word 0xaf057407\n    ret\n"   /* vloxseg6ei64.v   */
    ".word 0xa7057427\n    ret\n"   /* vsuxseg6ei64.v   */
    ".word 0xaf057427\n    ret\n"   /* vsoxseg6ei64.v   */
    ".word 0xc7057407\n    ret\n"   /* vluxseg7ei64.v   */
    ".word 0xcf057407\n    ret\n"   /* vloxseg7ei64.v   */
    ".word 0xc7057427\n    ret\n"   /* vsuxseg7ei64.v   */
    ".word 0xcf057427\n    ret\n"   /* vsoxseg7ei64.v   */
    ".word 0xe7057407\n    ret\n"   /* vluxseg8ei64.v   */
    ".word 0xef057407\n    ret\n"   /* vloxseg8ei64.v   */
    ".word 0xe7057427\n    ret\n"   /* vsuxseg8ei64.v   */
    ".word 0xef057427\n    ret\n"   /* vsoxseg8ei64.v   */
        ".option pop\n"
        ".previous\n");

extern const unsigned char d12_stubs[];

#define D12_ENCODINGS 32u
#define D12_STUB_STRIDE 8u

static void d12_run(unsigned int which)
{
    ((void (*)(unsigned int))(const void *)(d12_stubs + D12_STUB_STRIDE * which))(
        (unsigned int)(unsigned long)vsrc);
}

static int phase_d12(void)
{
    const unsigned int kVstartMark = 3u;
    unsigned int trapped_mask = 0;
    unsigned int mcause_ok = 1;
    unsigned int vstart_kept = 1;
    unsigned int vd_kept = 1;
    unsigned int vs_kept = 1;

    for (unsigned int i = 0; i < ELEMENTS; ++i) {
        vsrc[i] = 0x2000u + i;
        vdst[i] = 0;
        idx[i] = 0;
        idx[i + ELEMENTS] = 0;
    }

    __asm__ volatile("vsetvli zero, %0, e64, m2, ta, ma" : : "r"(ELEMENTS));
    __asm__ volatile("vle64.v v16, (%0)" : : "r"(idx) : "memory");
    __asm__ volatile("vsetvli zero, %0, e32, m1, ta, ma" : : "r"(ELEMENTS));

    /* A recognisable pattern in the destination register, so "unchanged" is a
     * positive statement rather than "still zero". */
    for (unsigned int i = 0; i < ELEMENTS; ++i) {
        vdst[i] = 0xc0de0000u + i;
    }
    __asm__ volatile("vle32.v v8, (%0)" : : "r"(vdst) : "memory");

    /* From here on, no access to `vsrc` is legitimate: the probes name it as a
     * base register but must never execute. The host zeroes its counter here
     * rather than at the start of the phase, because the setup above writes the
     * buffer sixteen times and counting those would make the check impossible
     * to satisfy. */
    store_word(SIM_D12_PHASE_BEGIN, 1u);

    /* Do not let the trap entry itself affect the state under test. Reading a
     * vector CSR should not dirty VS, but the property being measured here is
     * stronger and simpler: each illegal encoding must leave VS exactly Clean. */
    sample_vector_csrs = 0;

    for (unsigned int which = 0; which < D12_ENCODINGS; ++which) {
        trap_action = TRAP_ACTION_SKIP;
        trap_count = 0;
        trap_mcause = 0;
        CSR_WRITE(vstart, kVstartMark);
        write_vs(VS_CLEAN);

        d12_run(which);

        if (trap_count == 1u) {
            trapped_mask |= 1u << which;
        }
        if (trap_mcause != EXC_ILLEGAL_INSTR) {
            mcause_ok = 0;
        }
        /* An illegal instruction never begins executing, so it cannot have
         * consumed elements: `vstart` must still hold what was written. */
        if (CSR_READ(vstart) != kVstartMark) {
            vstart_kept = 0;
        }
        /* `prepInstr()` sets VS to Dirty. Checking every encoding, rather than
         * one representative, pins all 32 guards before that first side effect. */
        if (read_vs() != VS_CLEAN) {
            vs_kept = 0;
        }
        write_vs(VS_DIRTY);
    }
    trap_action = TRAP_ACTION_ABORT;
    CSR_WRITE(vstart, 0);
    sample_vector_csrs = 1;

    /* The destination register must be byte-for-byte what it was. */
    for (unsigned int i = 0; i < ELEMENTS; ++i) {
        vdst[i] = 0;
    }
    __asm__ volatile("vse32.v v8, (%0)" : : "r"(vdst) : "memory");
    for (unsigned int i = 0; i < ELEMENTS; ++i) {
        if (vdst[i] != 0xc0de0000u + i) {
            vd_kept = 0;
        }
    }

    store_word(SIM_D12_TRAP_MASK, trapped_mask);
    store_word(SIM_D12_MCAUSE_OK, mcause_ok);
    store_word(SIM_D12_VSTART_KEPT, vstart_kept);
    store_word(SIM_D12_VD_KEPT, vd_kept);
    store_word(SIM_D12_VS_KEPT, vs_kept ? VS_CLEAN : VS_DIRTY);
    /* The host snapshots its access counts here: everything above must have
     * produced no bus traffic to `vsrc`. */
    store_word(SIM_D12_PHASE_END, 1u);

    if (trapped_mask != 0xffffffffu) {
        return CCHK_D12_NOT_ALL_TRAPPED;
    }
    if (!mcause_ok) {
        return CCHK_D12_WRONG_MCAUSE;
    }
    if (!vstart_kept) {
        return CCHK_D12_VSTART_CLOBBERED;
    }
    if (!vd_kept) {
        return CCHK_D12_VD_CLOBBERED;
    }
    if (!vs_kept) {
        return CCHK_D12_VS_DIRTIED;
    }
    return 0;
}

/* ── D13 ──────────────────────────────────────────────────────────────────── */

static int phase_d13(void)
{
    volatile unsigned int* const bad =
        (volatile unsigned int*)(unsigned long)UNMAPPED_BASE;

    /* 1. instruction fetch → cause 1.
     *
     * Cannot be stepped over: `mepc` points into unmapped memory, so `mepc + 4`
     * faults again. The handler is given a recovery point instead. */
    trap_action = TRAP_ACTION_RESUME_AT;
    trap_count = 0;
    trap_resume_pc = (unsigned int)(unsigned long)&&after_fetch;
    ((void (*)(void))(unsigned long)UNMAPPED_BASE)();
after_fetch:
    trap_action = TRAP_ACTION_ABORT;
    store_word(SIM_D13_FETCH_MCAUSE, trap_mcause);
    store_word(SIM_D13_FETCH_MTVAL, trap_mtval);
    if (trap_count == 0u || trap_mcause != EXC_INSTR_ACCESS_FAULT) {
        return CCHK_D13_FETCH;
    }

    /* 2. scalar load → cause 5. */
    trap_action = TRAP_ACTION_SKIP;
    trap_count = 0;
    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "lw t0, 0(%0)\n\t"
                     ".option pop"
                     :
                     : "r"(bad)
                     : "t0", "memory");
    store_word(SIM_D13_LOAD_MCAUSE, trap_mcause);
    store_word(SIM_D13_LOAD_MTVAL, trap_mtval);
    if (trap_count != 1u || trap_mcause != EXC_LOAD_ACCESS_FAULT
        || trap_mtval != UNMAPPED_BASE) {
        return CCHK_D13_LOAD;
    }

    /* 3. scalar store → cause 7. */
    trap_count = 0;
    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "sw zero, 0(%0)\n\t"
                     ".option pop"
                     :
                     : "r"(bad)
                     : "memory");
    store_word(SIM_D13_STORE_MCAUSE, trap_mcause);
    store_word(SIM_D13_STORE_MTVAL, trap_mtval);
    if (trap_count != 1u || trap_mcause != EXC_STORE_AMO_ACCESS_FAULT
        || trap_mtval != UNMAPPED_BASE) {
        return CCHK_D13_STORE;
    }

    /* 4. vector load → cause 5, with `mtval` and `vstart` naming the element
     *    that failed. The base is placed relative to the *end of RAM* so that
     *    elements 0..3 are inside the probe memory and element 4 is the first
     *    one outside it. Using UNMAPPED_BASE here instead would fault on
     *    element 0 and prove nothing about `vstart`. */
    const unsigned int fault_element = 4u;
    const unsigned int ram_end = (unsigned int)(unsigned long)__ram_end;
    __asm__ volatile("vsetvli zero, %0, e32, m1, ta, ma" : : "r"(ELEMENTS));
    volatile unsigned int* const edge =
        (volatile unsigned int*)(unsigned long)(ram_end - 4u * fault_element);
    trap_count = 0;
    trap_vstart = 0xffffffffu;
    __asm__ volatile("vle32.v v20, (%0)" : : "r"(edge) : "memory");
    store_word(SIM_D13_VLOAD_MCAUSE, trap_mcause);
    store_word(SIM_D13_VLOAD_MTVAL, trap_mtval);
    store_word(SIM_D13_VLOAD_VSTART, trap_vstart);
    CSR_WRITE(vstart, 0);
    if (trap_count != 1u || trap_mcause != EXC_LOAD_ACCESS_FAULT
        || trap_mtval != ram_end || trap_vstart != fault_element) {
        return CCHK_D13_VLOAD;
    }

    /* 5. vector store → cause 7, same shape. */
    trap_count = 0;
    trap_vstart = 0xffffffffu;
    __asm__ volatile("vse32.v v20, (%0)" : : "r"(edge) : "memory");
    store_word(SIM_D13_VSTORE_MCAUSE, trap_mcause);
    store_word(SIM_D13_VSTORE_MTVAL, trap_mtval);
    store_word(SIM_D13_VSTORE_VSTART, trap_vstart);
    CSR_WRITE(vstart, 0);
    if (trap_count != 1u || trap_mcause != EXC_STORE_AMO_ACCESS_FAULT
        || trap_mtval != ram_end || trap_vstart != fault_element) {
        return CCHK_D13_VSTORE;
    }

    /* 6. AMO → cause 7, even though the access begins as a read. There is no
     *    "AMO load access fault" in the privileged specification. */
    trap_count = 0;
    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "li t1, 1\n\t"
                     "amoadd.w t0, t1, (%0)\n\t"
                     ".option pop"
                     :
                     : "r"(bad)
                     : "t0", "t1", "memory");
    trap_action = TRAP_ACTION_ABORT;
    store_word(SIM_D13_AMO_MCAUSE, trap_mcause);
    store_word(SIM_D13_AMO_MTVAL, trap_mtval);
    if (trap_count != 1u || trap_mcause != EXC_STORE_AMO_ACCESS_FAULT
        || trap_mtval != UNMAPPED_BASE) {
        return CCHK_D13_AMO;
    }

    /* 7. a target refusing with a generic rather than an address error. Same
     *    guest-visible outcome: the response status distinguishes "the target
     *    said no" from "the transaction was malformed", and only the second is
     *    a model defect. */
    trap_action = TRAP_ACTION_SKIP;
    trap_count = 0;
    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "lw t0, 0(%0)\n\t"
                     ".option pop"
                     :
                     : "r"((volatile unsigned int *)(unsigned long)UNMAPPED_GENERIC)
                     : "t0", "memory");
    trap_action = TRAP_ACTION_ABORT;
    store_word(SIM_D13_GENERIC_MCAUSE, trap_mcause);
    store_word(SIM_D13_GENERIC_MTVAL, trap_mtval);
    if (trap_count != 1u || trap_mcause != EXC_LOAD_ACCESS_FAULT
        || trap_mtval != UNMAPPED_GENERIC) {
        return CCHK_D13_GENERIC;
    }

    return 0;
}

int main(void)
{
    int status = phase_d12();
    if (status != 0) {
        return status;
    }
    return phase_d13();
}
