/* SPDX-License-Identifier: Apache-2.0
 *
 * RV32GCV architectural smoke image — the minimum program the Phase 2 gate
 * requires:
 *
 *     vsetvli for e8/e16/e32/e64, vector load and store, vadd, vmul, one
 *     widening operation, one mask operation, one reduction, one permutation,
 *     one FP operation, and vlenb read == 64
 *
 * Written in inline assembly rather than with `<riscv_vector.h>` intrinsics on
 * purpose. This image is also the corpus for the differential comparison
 * against Spike, and a diff is only interpretable if both models executed the
 * *same instructions*. Intrinsics leave that to the compiler's choice of LMUL,
 * tail policy and scheduling, which can change with an optimisation flag.
 *
 * Freestanding: no libc, no libm, no headers beyond our own. See D4.
 */

#include "sim_exit.h"

/* Each check has a stable id, reported as the exit status when it fails, so a
 * failure names the operation rather than just "nonzero". Ids are never reused
 * or renumbered — the differential corpus refers to them. */
enum check_id {
    CHK_VLENB = 1,
    CHK_VSETVLI_E8 = 2,
    CHK_VSETVLI_E16 = 3,
    CHK_VSETVLI_E32 = 4,
    CHK_VSETVLI_E64 = 5,
    CHK_LOAD_STORE = 6,
    CHK_VADD = 7,
    CHK_VMUL = 8,
    CHK_WIDENING = 9,
    CHK_MASK = 10,
    CHK_REDUCTION = 11,
    CHK_PERMUTATION = 12,
    CHK_FP = 13,
    CHK_VSTART = 14,
};

/* 64-byte aligned so a whole VLEN=512 register's worth of elements is one
 * naturally aligned block; see the .vdata section in link.ld. */
static unsigned int src_a[16] __attribute__((section(".vdata"), aligned(64)));
static unsigned int src_b[16] __attribute__((section(".vdata"), aligned(64)));
static unsigned int dst[16] __attribute__((section(".vdata"), aligned(64)));
static unsigned long long wide_dst[16]
    __attribute__((section(".vdata"), aligned(64)));

static inline unsigned int read_vlenb(void)
{
    unsigned int value;
    __asm__ volatile("csrr %0, vlenb" : "=r"(value));
    return value;
}

static inline unsigned int read_vstart(void)
{
    unsigned int value;
    __asm__ volatile("csrr %0, vstart" : "=r"(value));
    return value;
}

/* Low 32 bits of `mcycle`. Sampled by the firmware because the ISS only
 * produces a correct value through the CSR read path; see sim_exit.h. */
static inline unsigned int read_mcycle(void)
{
    unsigned int value;
    __asm__ volatile("csrr %0, mcycle" : "=r"(value));
    return value;
}

static inline void store_word(unsigned int address, unsigned int value)
{
    /* Via uintptr-sized integer: on RV32 `unsigned int` is pointer-sized, but
     * casting straight from it warns, and -Werror is on. */
    *(volatile unsigned int *)(unsigned long)address = value;
}

/* `vsetvli` with AVL larger than VLMAX returns VLMAX, so this reports the
 * machine's real vector length for a given SEW at LMUL=1. */
static inline unsigned int vlmax_for_e8(void)
{
    unsigned int vl;
    __asm__ volatile("vsetvli %0, %1, e8, m1, ta, ma" : "=r"(vl) : "r"(1024u));
    return vl;
}

static inline unsigned int vlmax_for_e16(void)
{
    unsigned int vl;
    __asm__ volatile("vsetvli %0, %1, e16, m1, ta, ma" : "=r"(vl) : "r"(1024u));
    return vl;
}

static inline unsigned int vlmax_for_e32(void)
{
    unsigned int vl;
    __asm__ volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(1024u));
    return vl;
}

static inline unsigned int vlmax_for_e64(void)
{
    unsigned int vl;
    __asm__ volatile("vsetvli %0, %1, e64, m1, ta, ma" : "=r"(vl) : "r"(1024u));
    return vl;
}

int main(void)
{
    /* First `mcycle` sample: the F11 baseline check. A sane machine reports a
     * small number here; the pre-backport ISS reported hundreds of millions of
     * cycles before executing anything. */
    store_word(SIM_EXIT_MCYCLE_START, read_mcycle());

    /* --- vlenb ------------------------------------------------------------
     * VLEN=512 must present vlenb=64 to firmware. Read from the CSR, because
     * that is what firmware would read; a constant here would test nothing. */
    if (read_vlenb() != 64u) {
        return CHK_VLENB;
    }

    /* --- vsetvli across every required SEW --------------------------------
     * At VLEN=512, LMUL=1: 64/32/16/8 elements for e8/e16/e32/e64. e64 also
     * exercises ELEN=64 on an RV32 hart. */
    if (vlmax_for_e8() != 64u) {
        return CHK_VSETVLI_E8;
    }
    if (vlmax_for_e16() != 32u) {
        return CHK_VSETVLI_E16;
    }
    if (vlmax_for_e32() != 16u) {
        return CHK_VSETVLI_E32;
    }
    if (vlmax_for_e64() != 8u) {
        return CHK_VSETVLI_E64;
    }

    for (unsigned int i = 0; i < 16u; ++i) {
        src_a[i] = i + 1u;         /* 1..16   */
        src_b[i] = (i + 1u) * 3u;  /* 3..48   */
        dst[i] = 0xdeadbeefu;
        wide_dst[i] = 0ull;
    }

    unsigned int vl = 0;
    __asm__ volatile("vsetvli %0, %1, e32, m1, ta, ma" : "=r"(vl) : "r"(16u));
    if (vl != 16u) {
        return CHK_VSETVLI_E32;
    }

    /* --- vle32 / vse32 round trip -----------------------------------------
     * Proves the vector load/store path reaches memory at all before any
     * arithmetic result is trusted. Under VP++ each element is a separate TLM
     * transaction (decision record D7); that is invisible here and must stay
     * invisible — this image must never assume a transaction granularity. */
    __asm__ volatile(
        "vle32.v v1, (%0)\n\t"
        "vse32.v v1, (%1)\n\t"
        :
        : "r"(src_a), "r"(dst)
        : "memory");
    for (unsigned int i = 0; i < 16u; ++i) {
        if (dst[i] != src_a[i]) {
            return CHK_LOAD_STORE;
        }
    }

    /* --- vadd.vv ----------------------------------------------------------- */
    __asm__ volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vadd.vv v3, v1, v2\n\t"
        "vse32.v v3, (%2)\n\t"
        :
        : "r"(src_a), "r"(src_b), "r"(dst)
        : "memory");
    for (unsigned int i = 0; i < 16u; ++i) {
        if (dst[i] != src_a[i] + src_b[i]) {
            return CHK_VADD;
        }
    }

    /* --- vmul.vv ----------------------------------------------------------- */
    __asm__ volatile(
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vmul.vv v3, v1, v2\n\t"
        "vse32.v v3, (%2)\n\t"
        :
        : "r"(src_a), "r"(src_b), "r"(dst)
        : "memory");
    for (unsigned int i = 0; i < 16u; ++i) {
        if (dst[i] != src_a[i] * src_b[i]) {
            return CHK_VMUL;
        }
    }

    /* --- widening: vwmulu.vv, e32 operands into e64 results ----------------
     * The destination group is EMUL=2, so v4 (an even register) is required.
     * This is the case that would silently truncate if the model got widening
     * wrong, and 32x32 products are exactly where truncation hides. */
    __asm__ volatile(
        "vsetvli zero, %3, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vwmulu.vv v4, v1, v2\n\t"
        "vsetvli zero, %3, e64, m2, ta, ma\n\t"
        "vse64.v v4, (%2)\n\t"
        :
        : "r"(src_a), "r"(src_b), "r"(wide_dst), "r"(16u)
        : "memory");
    for (unsigned int i = 0; i < 16u; ++i) {
        const unsigned long long expected =
            (unsigned long long)src_a[i] * (unsigned long long)src_b[i];
        if (wide_dst[i] != expected) {
            return CHK_WIDENING;
        }
    }

    /* --- mask: vmseq.vv then vcpop.m --------------------------------------
     * src_a == src_a for all 16 elements, so the mask is all ones and the
     * population count is the vector length. Comparing against src_b as well
     * would be stronger, but this is a smoke image: the differential run
     * against Spike is where mask corner cases belong. */
    unsigned int popcount = 0;
    __asm__ volatile(
        "vsetvli zero, %2, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vmseq.vv v0, v1, v1\n\t"
        "vcpop.m %0, v0\n\t"
        : "=r"(popcount)
        : "r"(src_a), "r"(16u)
        : "memory");
    if (popcount != 16u) {
        return CHK_MASK;
    }

    /* --- reduction: vredsum.vs --------------------------------------------
     * sum(1..16) = 136, plus the scalar seed in v2[0], which is zero here.
     * The result lands in element 0 of the destination. */
    unsigned int sum = 0;
    __asm__ volatile(
        "vsetvli zero, %2, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%1)\n\t"
        "vmv.v.i v2, 0\n\t"
        "vredsum.vs v3, v1, v2\n\t"
        "vmv.x.s %0, v3\n\t"
        : "=r"(sum)
        : "r"(src_a), "r"(16u)
        : "memory");
    if (sum != 136u) {
        return CHK_REDUCTION;
    }

    /* --- permutation: vslideup.vi ------------------------------------------
     * Slide up by 2: dst[i] = src_a[i-2] for i >= 2. Elements 0 and 1 are
     * untouched by the slide, so they are preloaded with a sentinel and must
     * survive — a model that cleared them would pass a weaker check. */
    for (unsigned int i = 0; i < 16u; ++i) {
        dst[i] = 0xa5a5a5a5u;
    }
    __asm__ volatile(
        "vsetvli zero, %2, e32, m1, tu, ma\n\t"
        "vle32.v v1, (%0)\n\t"
        "vle32.v v3, (%1)\n\t"
        "vslideup.vi v3, v1, 2\n\t"
        "vse32.v v3, (%1)\n\t"
        :
        : "r"(src_a), "r"(dst), "r"(16u)
        : "memory");
    if (dst[0] != 0xa5a5a5a5u || dst[1] != 0xa5a5a5a5u) {
        return CHK_PERMUTATION;
    }
    for (unsigned int i = 2; i < 16u; ++i) {
        if (dst[i] != src_a[i - 2u]) {
            return CHK_PERMUTATION;
        }
    }

    store_word(SIM_EXIT_MCYCLE_MID, read_mcycle());

    /* --- floating point: vfadd.vv ------------------------------------------
     * 1.0 + 2.0 == 3.0, checked through the raw bit pattern so no host float
     * comparison is involved. 0x3f800000 = 1.0f, 0x40000000 = 2.0f,
     * 0x40400000 = 3.0f. */
    for (unsigned int i = 0; i < 16u; ++i) {
        src_a[i] = 0x3f800000u;
        src_b[i] = 0x40000000u;
        dst[i] = 0u;
    }
    __asm__ volatile(
        "vsetvli zero, %3, e32, m1, ta, ma\n\t"
        "vle32.v v1, (%0)\n\t"
        "vle32.v v2, (%1)\n\t"
        "vfadd.vv v3, v1, v2\n\t"
        "vse32.v v3, (%2)\n\t"
        :
        : "r"(src_a), "r"(src_b), "r"(dst), "r"(16u)
        : "memory");
    for (unsigned int i = 0; i < 16u; ++i) {
        if (dst[i] != 0x40400000u) {
            return CHK_FP;
        }
    }

    /* --- vstart -------------------------------------------------------------
     * Every vector instruction above completed, so `vstart` must be zero. A
     * non-zero value here means the model left restart state behind, which
     * would corrupt the next interrupted instruction rather than this one —
     * the kind of defect that surfaces three phases later. */
    if (read_vstart() != 0u) {
        return CHK_VSTART;
    }

    store_word(SIM_EXIT_MCYCLE_END, read_mcycle());

    return SIM_EXIT_PASS;
}
