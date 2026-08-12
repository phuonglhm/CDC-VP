/* SPDX-License-Identifier: Apache-2.0
 *
 * The Spike differential corpus.
 *
 * One image, run unmodified on RISC-V VP++ and on Spike. It fills the canonical
 * signature block (`sig_layout.h`) and exits through HTIF; the two blocks are
 * then compared word by word.
 *
 * ── how divergence is kept from cascading ───────────────────────────────────
 *
 * Several probes here are *expected* to behave differently on the two models —
 * that is the point. The danger is that one difference changes control flow and
 * every later word diverges too, turning one finding into fifty.
 *
 * So every probe that can trap on one model and not the other:
 *
 *   1. runs with `trap_action = SKIP`, so both models leave the probe at the
 *      same PC whether or not they trapped;
 *   2. records the outcome in its *own* named word rather than in shared state;
 *   3. restores any architectural state it disturbed (mstatus.FS, mstatus.VS,
 *      vstart, vtype) before the next phase.
 *
 * `phase_reached` is updated as each phase completes, so if a model does abort
 * the comparison still says where.
 *
 * ── why `.option norvc` around the probes ───────────────────────────────────
 *
 * `SKIP` advances `mepc` by 4. With the C extension enabled the assembler will
 * happily compress `flw` to `c.flw`, and skipping a 2-byte instruction by 4
 * lands in the middle of the next one. The probes are pinned to 32-bit
 * encodings rather than making the trap handler decode instruction length,
 * which would put a second decoder in the test.
 */

#include "sig_layout.h"

static unsigned int sig[TPU_V3_SIG_WORD_COUNT]
    __attribute__((section(".signature"), used, aligned(4))) = {0};

#define SIG(field) sig[TPU_V3_SIG_##field]

enum sig_check_id {
    SCHK_VLENB = 1,
    SCHK_VL = 2,
    SCHK_UNEXPECTED_TRAP = 3,
    SCHK_FAULT_MISSING = 4,
};

#define ELEMENTS 16u

/* mstatus.FS is bits [14:13], mstatus.VS bits [10:9], SD is bit 31. */
#define MSTATUS_FS_SHIFT 13u
#define MSTATUS_FS_MASK (3u << MSTATUS_FS_SHIFT)
#define MSTATUS_VS_SHIFT 9u
#define MSTATUS_VS_MASK (3u << MSTATUS_VS_SHIFT)
#define MSTATUS_SD_MASK 0x80000000u
/* MIE | MPIE | MPP — the bits a trap and an `mret` actually move. The rest of
 * mstatus is masked out because it encodes which privilege modes the machine
 * implements, and the two models are configured independently there; a
 * difference would be a configuration fact, not a defect, and it does not
 * belong in a gate that is meant to report defects. */
#define MSTATUS_TRAP_BITS 0x00001888u

#define FS_OFF 0u
#define FS_INITIAL 1u
#define FS_CLEAN 2u
#define FS_DIRTY 3u
#define VS_INITIAL 1u
#define VS_DIRTY 3u

#define FRM_RNE 0u
#define FRM_RTZ 1u

static unsigned int ibuf_a[ELEMENTS] __attribute__((section(".vdata"), aligned(64)));
static unsigned int ibuf_b[ELEMENTS] __attribute__((section(".vdata"), aligned(64)));
static unsigned int ibuf_r[ELEMENTS] __attribute__((section(".vdata"), aligned(64)));
static unsigned int ibuf_w[2 * ELEMENTS] __attribute__((section(".vdata"), aligned(64)));
static unsigned int fbuf_a[ELEMENTS] __attribute__((section(".vdata"), aligned(64)));
static unsigned int fbuf_b[ELEMENTS] __attribute__((section(".vdata"), aligned(64)));
static unsigned int fbuf_r[ELEMENTS] __attribute__((section(".vdata"), aligned(64)));
static unsigned int idxbuf[2 * ELEMENTS] __attribute__((section(".vdata"), aligned(64)));
static unsigned int scratch[8] __attribute__((section(".vdata"), aligned(64)));

/* End of RAM, from the linker script. The mid-vector fault probe works by
 * running a vector load off the end of memory, which is the only fault both
 * models can produce from the same image: Spike has no way to inject a bus
 * error at an arbitrary address, and an unmapped access is a fault on any
 * machine. */
extern char __ram_end[];

static volatile unsigned int trap_action = SIG_TRAP_ACTION_ABORT;
static volatile unsigned int trap_total;
static volatile unsigned int probe_traps;
static volatile unsigned int probe_mcause;
static volatile unsigned int probe_mepc;
static volatile unsigned int probe_mtval;
static volatile unsigned int probe_vstart;
static volatile unsigned int probe_vl;
static volatile unsigned int probe_vtype;

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

static inline unsigned int read_mstatus(void)
{
    return CSR_READ(mstatus);
}

static inline void set_fs(unsigned int state)
{
    __asm__ volatile("csrc mstatus, %0" : : "r"(MSTATUS_FS_MASK));
    __asm__ volatile("csrs mstatus, %0"
                     :
                     : "r"((state << MSTATUS_FS_SHIFT) & MSTATUS_FS_MASK));
}

static inline unsigned int read_fs(void)
{
    return (read_mstatus() & MSTATUS_FS_MASK) >> MSTATUS_FS_SHIFT;
}

static inline void set_vs(unsigned int state)
{
    __asm__ volatile("csrc mstatus, %0" : : "r"(MSTATUS_VS_MASK));
    __asm__ volatile("csrs mstatus, %0"
                     :
                     : "r"((state << MSTATUS_VS_SHIFT) & MSTATUS_VS_MASK));
}

static inline unsigned int read_vs(void)
{
    return (read_mstatus() & MSTATUS_VS_MASK) >> MSTATUS_VS_SHIFT;
}

/* FNV-1a. Chosen because it is four lines and has no lookup table: the checksum
 * has to be identical on both models, so it must not depend on anything but the
 * bytes it is given. */
static unsigned int fnv1a(unsigned int hash, const unsigned int* words,
                          unsigned int count)
{
    for (unsigned int i = 0; i < count; ++i) {
        const unsigned int word = words[i];
        for (unsigned int byte = 0; byte < 4; ++byte) {
            hash ^= (word >> (8u * byte)) & 0xffu;
            hash *= 16777619u;
        }
    }
    return hash;
}

/* Overrides the weak default in crt0_sig.S. */
unsigned int trap_handler(unsigned int mcause, unsigned int mepc,
                          unsigned int mtval);

unsigned int trap_handler(unsigned int mcause, unsigned int mepc,
                          unsigned int mtval)
{
    ++trap_total;
    ++probe_traps;
    probe_mcause = mcause;
    probe_mepc = mepc;
    probe_mtval = mtval;

    /* Only when the vector unit is on. With `mstatus.VS = Off` a `csrr vstart`
     * is itself an illegal instruction, and a handler that read it
     * unconditionally would trap inside the trap and spin. */
    if (read_vs() != 0u) {
        probe_vstart = CSR_READ(vstart);
        probe_vl = CSR_READ(vl);
        probe_vtype = CSR_READ(vtype);
    }

    return trap_action;
}

/* Runs `body` with traps recorded and skipped, and reports whether one fired.
 * Every deliberate-trap probe goes through this so the two models always leave
 * the probe at the same PC. */
#define PROBE_BEGIN()                                                         \
    do {                                                                      \
        probe_traps = 0;                                                      \
        probe_mcause = 0;                                                     \
        trap_action = SIG_TRAP_ACTION_SKIP;                                   \
    } while (0)

#define PROBE_END()                                                           \
    do {                                                                      \
        trap_action = SIG_TRAP_ACTION_ABORT;                                  \
    } while (0)

static void phase_identity(void)
{
    SIG(magic) = TPU_V3_SIG_MAGIC;
    SIG(version) = TPU_V3_SIG_VERSION;
    SIG(vlenb) = CSR_READ(vlenb);
    SIG(mhartid) = CSR_READ(mhartid);
    SIG(phase_reached) = 1;
}

static void phase_vtype(void)
{
    unsigned int vl = 0;
    __asm__ volatile("vsetvli %0, %1, e32, m1, ta, ma"
                     : "=r"(vl)
                     : "r"(ELEMENTS));
    SIG(vl_normal) = vl;
    SIG(vtype_normal) = CSR_READ(vtype);
    SIG(vcsr) = CSR_READ(vcsr);
    SIG(phase_reached) = 2;
}

static void phase_integer_vector(void)
{
    for (unsigned int i = 0; i < ELEMENTS; ++i) {
        ibuf_a[i] = 0x01020304u + i * 0x11111111u;
        /* Three elements are made equal to `a` so the mask from `vmseq` is
         * neither all-zero nor all-one; a degenerate mask would let a broken
         * `vcpop` still produce the right number. */
        ibuf_b[i] = (i % 5u == 0u) ? ibuf_a[i] : (i * 7u + 3u);
        ibuf_r[i] = 0;
    }
    for (unsigned int i = 0; i < 2 * ELEMENTS; ++i) {
        ibuf_w[i] = 0;
    }

    __asm__ volatile("vsetvli zero, %0, e32, m1, ta, ma" : : "r"(ELEMENTS));
    __asm__ volatile("vle32.v v1, (%0)\n\t"
                     "vle32.v v2, (%1)"
                     :
                     : "r"(ibuf_a), "r"(ibuf_b)
                     : "memory");

    __asm__ volatile("vadd.vv v3, v1, v2\n\t"
                     "vse32.v v3, (%0)"
                     :
                     : "r"(ibuf_r)
                     : "memory");
    unsigned int sum = 0;
    for (unsigned int i = 0; i < ELEMENTS; ++i) {
        sum += ibuf_r[i];
    }
    SIG(vadd_sum) = sum;
    SIG(vint_checksum) = fnv1a(2166136261u, ibuf_r, ELEMENTS);

    __asm__ volatile("vmul.vv v4, v1, v2\n\t"
                     "vse32.v v4, (%0)"
                     :
                     : "r"(ibuf_r)
                     : "memory");
    sum = 0;
    for (unsigned int i = 0; i < ELEMENTS; ++i) {
        sum += ibuf_r[i];
    }
    SIG(vmul_sum) = sum;
    SIG(vint_checksum) = fnv1a(SIG(vint_checksum), ibuf_r, ELEMENTS);

    /* Widening: the destination group is 2*LMUL, so v6 must be even-aligned and
     * the store has to switch to e64/m2 to see all 32 result halves. */
    __asm__ volatile("vwmulu.vv v6, v1, v2");
    __asm__ volatile("vsetvli zero, %0, e64, m2, ta, ma" : : "r"(ELEMENTS));
    __asm__ volatile("vse64.v v6, (%0)" : : "r"(ibuf_w) : "memory");
    __asm__ volatile("vsetvli zero, %0, e32, m1, ta, ma" : : "r"(ELEMENTS));
    SIG(vwmulu_lo) = ibuf_w[0];
    SIG(vwmulu_hi) = ibuf_w[2 * ELEMENTS - 1];
    SIG(vint_checksum) = fnv1a(SIG(vint_checksum), ibuf_w, 2 * ELEMENTS);

    /* Mask register, then a population count over it. */
    __asm__ volatile("vmseq.vv v0, v1, v2");
    for (unsigned int i = 0; i < ELEMENTS; ++i) {
        ibuf_r[i] = 0;
    }
    __asm__ volatile("vsetvli zero, %0, e32, m1, ta, ma" : : "r"(1u));
    __asm__ volatile("vmv.x.s %0, v0" : "=r"(SIG(vmseq_mask)));
    __asm__ volatile("vsetvli zero, %0, e32, m1, ta, ma" : : "r"(ELEMENTS));
    unsigned int popcount = 0;
    __asm__ volatile("vcpop.m %0, v0" : "=r"(popcount));
    SIG(vcpop_count) = popcount;

    /* Reduction. v9 element 0 supplies the scalar accumulator. */
    for (unsigned int i = 0; i < ELEMENTS; ++i) {
        ibuf_r[i] = 0;
    }
    __asm__ volatile("vle32.v v9, (%0)" : : "r"(ibuf_r) : "memory");
    __asm__ volatile("vredsum.vs v8, v1, v9");
    __asm__ volatile("vse32.v v8, (%0)" : : "r"(ibuf_r) : "memory");
    SIG(vredsum_result) = ibuf_r[0];

    __asm__ volatile("vmv.v.i v10, 0\n\t"
                     "vslideup.vi v10, v1, 4\n\t"
                     "vse32.v v10, (%0)"
                     :
                     : "r"(ibuf_r)
                     : "memory");
    SIG(vslideup_word) = ibuf_r[4];

    /* vrgather with a reversing index vector: element 0 of the result must be
     * element 15 of the source. */
    for (unsigned int i = 0; i < ELEMENTS; ++i) {
        idxbuf[i] = ELEMENTS - 1u - i;
    }
    __asm__ volatile("vle32.v v13, (%0)\n\t"
                     "vrgather.vv v12, v1, v13\n\t"
                     "vse32.v v12, (%1)"
                     :
                     : "r"(idxbuf), "r"(ibuf_r)
                     : "memory");
    SIG(vrgather_word) = ibuf_r[0];
    SIG(vint_checksum) = fnv1a(SIG(vint_checksum), ibuf_r, ELEMENTS);

    SIG(phase_reached) = 3;
}

static void phase_vector_fp(void)
{
    /* 1.0f and 1.5 * 2^-24. ulp(1.0f) is 2^-23, so the exact sum sits 0.75 ulp
     * above 1.0 and the two rounding modes cannot agree:
     *   RNE -> 0x3f800001, RTZ -> 0x3f800000. */
    for (unsigned int i = 0; i < ELEMENTS; ++i) {
        fbuf_a[i] = 0x3f800000u;
        fbuf_b[i] = 0x33c00000u;
        fbuf_r[i] = 0;
    }

    __asm__ volatile("vsetvli zero, %0, e32, m1, ta, ma" : : "r"(ELEMENTS));
    __asm__ volatile("vle32.v v1, (%0)\n\t"
                     "vle32.v v2, (%1)"
                     :
                     : "r"(fbuf_a), "r"(fbuf_b)
                     : "memory");

    CSR_WRITE(frm, FRM_RNE);
    CSR_WRITE(fflags, 0);
    __asm__ volatile("vfadd.vv v3, v1, v2\n\t"
                     "vse32.v v3, (%0)"
                     :
                     : "r"(fbuf_r)
                     : "memory");
    SIG(vfadd_rne) = fbuf_r[0];

    CSR_WRITE(frm, FRM_RTZ);
    CSR_WRITE(fflags, 0);
    __asm__ volatile("vfadd.vv v3, v1, v2\n\t"
                     "vse32.v v3, (%0)"
                     :
                     : "r"(fbuf_r)
                     : "memory");
    SIG(vfadd_rtz) = fbuf_r[0];
    SIG(vfadd_flags) = CSR_READ(fflags);
    CSR_WRITE(frm, FRM_RNE);

    /* Drive the product subnormal. The exact product is 2^-150 * (1 + 2^-23),
     * a shade above half the smallest subnormal, so it must round *up* to
     * 0x00000001 with underflow and inexact both raised.
     *
     * The operands are deliberately not 2^-126 * 2^-24 exactly: that product
     * is exactly half the smallest subnormal, round-to-nearest-even sends it
     * to zero, and a model with no subnormal support at all would produce the
     * same zero and look correct. */
    for (unsigned int i = 0; i < ELEMENTS; ++i) {
        fbuf_a[i] = 0x00800001u;  /* 2^-126 * (1 + 2^-23) */
        fbuf_b[i] = 0x33800000u;  /* 2^-24 */
    }
    CSR_WRITE(fflags, 0);
    __asm__ volatile("vle32.v v1, (%0)\n\t"
                     "vle32.v v2, (%1)\n\t"
                     "vfmul.vv v4, v1, v2\n\t"
                     "vse32.v v4, (%2)"
                     :
                     : "r"(fbuf_a), "r"(fbuf_b), "r"(fbuf_r)
                     : "memory");
    SIG(vfmul_subnormal) = fbuf_r[0];
    SIG(vfmul_flags) = CSR_READ(fflags);

    /* Fused multiply-accumulate: vd += vs1 * vs2, with a single rounding. An
     * implementation that rounds the product first gives a different answer for
     * these operands. */
    for (unsigned int i = 0; i < ELEMENTS; ++i) {
        fbuf_a[i] = 0x3f800001u;  /* 1.0 + 1 ulp */
        fbuf_b[i] = 0x33800000u;  /* 2^-24 */
        fbuf_r[i] = 0x3f800000u;  /* 1.0 */
    }
    CSR_WRITE(fflags, 0);
    __asm__ volatile("vle32.v v1, (%0)\n\t"
                     "vle32.v v2, (%1)\n\t"
                     "vle32.v v5, (%2)\n\t"
                     "vfmacc.vv v5, v1, v2\n\t"
                     "vse32.v v5, (%2)"
                     :
                     : "r"(fbuf_a), "r"(fbuf_b), "r"(fbuf_r)
                     : "memory");
    SIG(vfmacc_result) = fbuf_r[0];

    /* Reductions.
     *
     * All sixteen elements are equal *on purpose*. `vfredusum` is explicitly
     * unordered: the spec lets an implementation pick any reduction tree, so
     * two conforming models may legitimately produce different results for
     * operands where the order matters. Equal operands make the sum
     * order-independent, which keeps this a check on the reduction rather than
     * on a choice the spec leaves free. `vfredosum` is ordered and must match
     * exactly whatever the operands are. */
    for (unsigned int i = 0; i < ELEMENTS; ++i) {
        fbuf_a[i] = 0x3e800000u;  /* 0.25f */
        fbuf_r[i] = 0;
    }
    __asm__ volatile("vle32.v v1, (%0)\n\t"
                     "vle32.v v9, (%1)\n\t"
                     "vfredusum.vs v8, v1, v9\n\t"
                     "vse32.v v8, (%1)"
                     :
                     : "r"(fbuf_a), "r"(fbuf_r)
                     : "memory");
    SIG(vfredusum_result) = fbuf_r[0];

    for (unsigned int i = 0; i < ELEMENTS; ++i) {
        fbuf_r[i] = 0;
    }
    __asm__ volatile("vle32.v v9, (%0)\n\t"
                     "vfredosum.vs v8, v1, v9\n\t"
                     "vse32.v v8, (%0)"
                     :
                     : "r"(fbuf_r)
                     : "memory");
    SIG(vfredosum_result) = fbuf_r[0];
    SIG(vfp_checksum) = fnv1a(2166136261u, fbuf_r, ELEMENTS);

    SIG(phase_reached) = 4;
}

/* Scalar-FP probes tied to decision record D9. Each one names the upstream
 * VP++ commit that would change its outcome, so a diff points at a decision. */
static void phase_scalar_fp(void)
{
    static const unsigned int f32_two = 0x40000000u;
    static const unsigned int f32_qnan = 0x7fc00000u;
    static const unsigned int f32_snan = 0x7f800001u;
    static const unsigned int f32_one = 0x3f800000u;
    static const unsigned int f32_three = 0x40400000u;
    static const unsigned int f32_inf = 0x7f800000u;
    /* 8-byte aligned, because `fld` takes a misaligned-load trap otherwise:
     * these are `unsigned int` pairs so the compiler would only give them
     * 4-byte alignment, and the first version of this file duly aborted here
     * on Spike with mcause 4. */
    static const unsigned int f64_two[2] __attribute__((aligned(8))) = {
        0x00000000u, 0x40000000u};
    static const unsigned int f64_qnan[2] __attribute__((aligned(8))) = {
        0x00000000u, 0x7ff80000u};
    /* Deliberately not a valid f32 NaN-box and not a canonical anything: the
     * point of the raw-store probes is that the bits survive untouched. */
    static const unsigned int f64_raw[2] __attribute__((aligned(8))) = {
        0x89abcdefu, 0x01234567u};
    /* A 32-bit pattern that is not a NaN and not zero, for the NaN-boxing
     * probe: after `flw` the register's upper half must be all ones whatever
     * the loaded bits were. */
    static const unsigned int f32_raw_probe = 0x89abcdefu;

    CSR_WRITE(frm, FRM_RNE);

    /* 63524fbb — fmin/fmax with one quiet NaN.
     *
     * The v2.2 semantics return the non-NaN operand. VP++ at this pin returns
     * `rs2` whenever `rs1` is not smaller, so it hands back the NaN. Only the
     * rs2-is-NaN ordering diverges; with the NaN in rs1 the old code already
     * returns the right answer, which is why the operands are in this order. */
    CSR_WRITE(fflags, 0);
    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "flw fa0, 0(%1)\n\t"
                     "flw fa1, 0(%2)\n\t"
                     "fmin.s fa2, fa0, fa1\n\t"
                     "fsw fa2, 0(%0)\n\t"
                     ".option pop"
                     :
                     : "r"(scratch), "r"(&f32_two), "r"(&f32_qnan)
                     : "memory", "fa0", "fa1", "fa2");
    SIG(fmin_s_qnan) = scratch[0];

    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "flw fa0, 0(%1)\n\t"
                     "flw fa1, 0(%2)\n\t"
                     "fmax.s fa2, fa0, fa1\n\t"
                     "fsw fa2, 0(%0)\n\t"
                     ".option pop"
                     :
                     : "r"(scratch), "r"(&f32_two), "r"(&f32_qnan)
                     : "memory", "fa0", "fa1", "fa2");
    SIG(fmax_s_qnan) = scratch[0];
    /* A quiet NaN operand must not raise anything. */
    SIG(fminmax_s_flags) = CSR_READ(fflags);

    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "fld fa0, 0(%1)\n\t"
                     "fld fa1, 0(%2)\n\t"
                     "fmin.d fa2, fa0, fa1\n\t"
                     "fsd fa2, 0(%0)\n\t"
                     ".option pop"
                     :
                     : "r"(scratch), "r"(f64_two), "r"(f64_qnan)
                     : "memory", "fa0", "fa1", "fa2");
    SIG(fmin_d_qnan_hi) = scratch[1];

    CSR_WRITE(fflags, 0);
    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "flw fa0, 0(%1)\n\t"
                     "flw fa1, 0(%2)\n\t"
                     "fmin.s fa2, fa0, fa1\n\t"
                     "fsw fa2, 0(%0)\n\t"
                     ".option pop"
                     :
                     : "r"(scratch), "r"(&f32_two), "r"(&f32_snan)
                     : "memory", "fa0", "fa1", "fa2");
    SIG(fmin_s_snan) = scratch[0];
    /* A signalling NaN operand must raise invalid on both models even though
     * they disagree about the result. */
    SIG(fminmax_snan_flags) = CSR_READ(fflags);

    /* c7140542 — raw value on float stores. `fsd` must write the register's
     * bits unchanged, including a pattern that is not a valid NaN-box. */
    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "fld fa0, 0(%1)\n\t"
                     "fsd fa0, 0(%0)\n\t"
                     ".option pop"
                     :
                     : "r"(scratch), "r"(f64_raw)
                     : "memory", "fa0");
    SIG(fsd_raw_lo) = scratch[0];
    SIG(fsd_raw_hi) = scratch[1];

    /* `fsw` of a register whose upper half is not all-ones. The architectural
     * answer is the raw low 32 bits; a model that NaN-boxes on the way out
     * writes the canonical NaN instead. */
    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "fld fa0, 0(%1)\n\t"
                     "fsw fa0, 0(%0)\n\t"
                     ".option pop"
                     :
                     : "r"(scratch), "r"(f64_raw)
                     : "memory", "fa0");
    SIG(fsw_raw) = scratch[0];

    /* `flw` must NaN-box: the upper 32 bits of the register become all ones. */
    scratch[0] = 0;
    scratch[1] = 0;
    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "flw fa0, 0(%1)\n\t"
                     "fsd fa0, 0(%0)\n\t"
                     ".option pop"
                     :
                     : "r"(scratch), "r"(&f32_raw_probe)
                     : "memory", "fa0");
    SIG(flw_nanbox_hi) = scratch[1];

    CSR_WRITE(fflags, 0);
    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "flw fa0, 0(%1)\n\t"
                     "flw fa1, 0(%2)\n\t"
                     "fdiv.s fa2, fa0, fa1\n\t"
                     "fsw fa2, 0(%0)\n\t"
                     ".option pop"
                     :
                     : "r"(scratch), "r"(&f32_one), "r"(&f32_three)
                     : "memory", "fa0", "fa1", "fa2");
    SIG(fdiv_inexact) = scratch[0];

    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "flw fa0, 0(%1)\n\t"
                     "fsqrt.s fa2, fa0\n\t"
                     "fsw fa2, 0(%0)\n\t"
                     ".option pop"
                     :
                     : "r"(scratch), "r"(&f32_two)
                     : "memory", "fa0", "fa2");
    SIG(fsqrt_result) = scratch[0];

    /* Converting +inf must saturate to INT32_MAX and raise invalid, not wrap. */
    CSR_WRITE(fflags, 0);
    unsigned int converted = 0;
    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "flw fa0, 0(%1)\n\t"
                     "fcvt.w.s %0, fa0, rtz\n\t"
                     ".option pop"
                     : "=r"(converted)
                     : "r"(&f32_inf)
                     : "fa0");
    SIG(fcvt_w_s_sat) = converted;
    SIG(fcvt_flags) = CSR_READ(fflags);

    unsigned int classified = 0;
    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "flw fa0, 0(%1)\n\t"
                     "fclass.s %0, fa0\n\t"
                     ".option pop"
                     : "=r"(classified)
                     : "r"(&f32_snan)
                     : "fa0");
    SIG(fclass_snan) = classified;

    SIG(phase_reached) = 5;
}

/* Instructions that must be illegal at rv32gcv.
 *
 * Zfh is not in the target ISA, so these are *not* expected diffs: both models
 * are required to refuse them, and a model that executes one is wrong. Encoded
 * as `.word` because the assembler will not emit them without `zfh` in
 * `-march`, and adding it would change the ISA the whole image is built for.
 */
static void phase_zfh_illegal(void)
{
    PROBE_BEGIN();
    __asm__ volatile(".word 0x04000053");  /* fadd.h f0, f0, f0 */
    SIG(zfh_fadd_h_trapped) = probe_traps != 0u;
    SIG(zfh_fadd_h_mcause) = probe_mcause;

    PROBE_BEGIN();
    __asm__ volatile(".word 0xe4000553");  /* fmv.x.h a0, f0  (91777991) */
    SIG(zfh_fmv_x_h_trapped) = probe_traps != 0u;

    PROBE_BEGIN();
    __asm__ volatile("mv a0, %0\n\t"
                     ".word 0x00051007" /* flh f0, 0(a0) */
                     :
                     : "r"(scratch)
                     : "a0", "memory");
    SIG(zfh_flh_trapped) = probe_traps != 0u;

    PROBE_BEGIN();
    __asm__ volatile("mv a0, %0\n\t"
                     ".word 0x00051027" /* fsh f0, 0(a0)  (c7140542) */
                     :
                     : "r"(scratch)
                     : "a0", "memory");
    SIG(zfh_fsh_trapped) = probe_traps != 0u;
    PROBE_END();

    SIG(phase_reached) = 6;
}

/* Audit finding F12: indexed access with index EEW=64 on RV32.
 *
 * v-spec 1.0 §18.2 states that the V extension does not support EEW=64 for
 * index values when XLEN=32, and §7.3 requires an illegal-instruction
 * exception when an offset EEW is not supported. So this must trap.
 */
static void phase_index_eew64(void)
{
    for (unsigned int i = 0; i < 2 * ELEMENTS; ++i) {
        idxbuf[i] = 0;
    }
    /* The index group for EEW=64 at SEW=32 is EMUL=2, so v16 and v17 both have
     * to be defined; zeroing them at e64/m2 covers the pair. */
    __asm__ volatile("vsetvli zero, %0, e64, m2, ta, ma" : : "r"(ELEMENTS));
    __asm__ volatile("vle64.v v16, (%0)" : : "r"(idxbuf) : "memory");
    __asm__ volatile("vsetvli zero, %0, e32, m1, ta, ma" : : "r"(ELEMENTS));

    PROBE_BEGIN();
    __asm__ volatile("vluxei64.v v8, (%0), v16" : : "r"(ibuf_a) : "memory");
    SIG(eew64_trapped) = probe_traps != 0u;
    SIG(eew64_mcause) = probe_mcause;
    PROBE_END();

    /* A trap leaves vstart at the element it stopped on; clear it so the next
     * vector instruction starts from the beginning on both models. */
    CSR_WRITE(vstart, 0);

    SIG(phase_reached) = 7;
}

/* mstatus.FS behaviour around float loads and stores — upstream 14e7fff5.
 *
 * At this pin VP++ omits both `fp_require_not_off()` and `fp_set_dirty()` from
 * the F and D load/store cases, so it neither traps with FS=Off nor marks the
 * state dirty. Spike does both. Two named words, one commit.
 */
static void phase_fp_state(void)
{
    /* Cleared here so `fflags_final` reports only what the remaining phases
     * raise; the earlier phases have already recorded their own flags. */
    CSR_WRITE(fflags, 0);

    static const unsigned int fp_state_pattern = 0x3fc00000u; /* 1.5f */

    /* FS=Clean, then a float **load**: the load writes an FP register, so the
     * state must become Dirty.
     *
     * A load, not a store, and the distinction matters. `fsw` only reads the
     * register file, so leaving FS at Clean after one is architecturally
     * correct — and the privileged spec additionally allows an implementation
     * to report Dirty imprecisely, which would make a store-based probe
     * unable to distinguish a defect from a permitted choice. A load has one
     * correct answer. Measured: Spike reports Clean after `fsw`, which is why
     * this probe is not the `fsw` one it started as. */
    set_fs(FS_CLEAN);
    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "flw fa0, 0(%0)\n\t"
                     ".option pop"
                     :
                     : "r"(&fp_state_pattern)
                     : "fa0");
    SIG(mstatus_fs_after_fp_ls) = read_fs();
    set_fs(FS_DIRTY);

    /* FS=Off, then a float load: the load must raise an illegal instruction.
     * Skipped either way, so both models resume at the same PC — but on the
     * model that did not trap, fa0 has been overwritten, so nothing after this
     * point may depend on it. */
    PROBE_BEGIN();
    set_fs(FS_OFF);
    __asm__ volatile(".option push\n\t"
                     ".option norvc\n\t"
                     "flw fa0, 0(%0)\n\t"
                     ".option pop"
                     :
                     : "r"(scratch)
                     : "memory");
    set_fs(FS_DIRTY);
    SIG(fp_ls_off_trapped) = probe_traps != 0u;
    SIG(fp_ls_off_mcause) = probe_mcause;
    PROBE_END();

    /* From Initial, a vector instruction must leave VS Dirty. */
    set_vs(VS_INITIAL);
    __asm__ volatile("vadd.vi v1, v1, 1");
    SIG(mstatus_vs_initial) = read_vs();
    set_vs(VS_DIRTY);

    SIG(phase_reached) = 8;
}

/* A reserved `vtype` must set `vill` and zero `vl` — not trap. A trap here
 * would itself be the defect, which is why `vill_trapped` is compared. */
static void phase_vill(void)
{
    unsigned int got_vl = 0;
    const unsigned int bad_vtype = (7u << 3); /* vsew = 0b111 is reserved */

    PROBE_BEGIN();
    __asm__ volatile("vsetvl %0, %1, %2"
                     : "=r"(got_vl)
                     : "r"(ELEMENTS), "r"(bad_vtype));
    SIG(vill_trapped) = probe_traps != 0u;
    PROBE_END();

    SIG(vill_vtype) = CSR_READ(vtype);
    SIG(vill_vl) = got_vl | CSR_READ(vl);

    /* Recover before using the vector unit again. */
    __asm__ volatile("vsetvli zero, %0, e32, m1, ta, ma" : : "r"(ELEMENTS));

    SIG(phase_reached) = 9;
}

/* Mid-vector fault and `vstart` resumption.
 *
 * The load runs off the end of RAM. Neither model can be told to fail a
 * specific address, but both fail an unmapped one, and the buffer is placed so
 * that elements 0..4 are inside memory and element 5 is the first word past it.
 *
 * The instruction is skipped rather than resumed: the address stays bad, so
 * resuming would loop. What is checked is that the trap named element 5, that
 * the elements below it were loaded, and that `vstart` can be cleared.
 */
static void phase_mid_vector_fault(void)
{
    const unsigned int fault_element = 5u;
    unsigned int* const edge =
        (unsigned int*)((unsigned long)__ram_end - 4u * fault_element);

    for (unsigned int i = 0; i < fault_element; ++i) {
        edge[i] = 0xa5a50000u + i;
    }
    for (unsigned int i = 0; i < ELEMENTS; ++i) {
        ibuf_r[i] = 0;
    }

    __asm__ volatile("vsetvli zero, %0, e32, m1, ta, ma" : : "r"(ELEMENTS));
    __asm__ volatile("vmv.v.i v20, 0");

    PROBE_BEGIN();
    probe_vstart = 0xffffffffu;
    __asm__ volatile("vle32.v v20, (%0)" : : "r"(edge) : "memory");
    PROBE_END();

    SIG(fault_trap_count) = probe_traps;
    SIG(fault_mcause) = probe_mcause;
    SIG(fault_mtval) = probe_mtval;
    SIG(fault_vstart) = probe_vstart;
    SIG(fault_vl) = probe_vl;
    SIG(fault_vtype) = probe_vtype;

    /* The store below would otherwise start at the element the trap stopped on. */
    CSR_WRITE(vstart, 0);
    SIG(vstart_after_clear) = CSR_READ(vstart);

    __asm__ volatile("vse32.v v20, (%0)" : : "r"(ibuf_r) : "memory");

    /* Only the elements below the faulting one are architecturally defined:
     * the instruction never completed, so elements from `vstart` upward keep
     * whatever the destination register held. */
    unsigned int ok = 1;
    for (unsigned int i = 0; i < fault_element; ++i) {
        if (ibuf_r[i] != 0xa5a50000u + i) {
            ok = 0;
        }
    }
    SIG(fault_elem_ok) = ok;

    SIG(phase_reached) = 10;
}

int main(void)
{
    phase_identity();
    phase_vtype();

    if (SIG(vlenb) != 64u) {
        return SCHK_VLENB;
    }
    if (SIG(vl_normal) != ELEMENTS) {
        return SCHK_VL;
    }

    phase_integer_vector();
    phase_vector_fp();
    phase_scalar_fp();
    phase_zfh_illegal();
    phase_index_eew64();
    phase_fp_state();
    phase_vill();
    phase_mid_vector_fault();

    SIG(mstatus_other) = read_mstatus() & MSTATUS_TRAP_BITS;
    SIG(frm_final) = CSR_READ(frm);
    SIG(fflags_final) = CSR_READ(fflags);
    SIG(trap_total) = trap_total;

    unsigned int hash = fnv1a(2166136261u, ibuf_a, ELEMENTS);
    hash = fnv1a(hash, ibuf_b, ELEMENTS);
    hash = fnv1a(hash, ibuf_r, ELEMENTS);
    hash = fnv1a(hash, ibuf_w, 2 * ELEMENTS);
    hash = fnv1a(hash, fbuf_r, ELEMENTS);
    SIG(mem_checksum) = hash;

    if (SIG(fault_trap_count) == 0u) {
        return SCHK_FAULT_MISSING;
    }

    SIG(phase_reached) = 11;
    return 0;
}
