/* SPDX-License-Identifier: Apache-2.0
 *
 * `rvv_vector_add` — the vector half of the Phase 4.5 handoff demonstration.
 *
 * Prints the four lines the plan requires and exits zero:
 *
 *     RVV=1.0
 *     VLEN=512
 *     vlenb=64
 *     VECTOR ADD: PASS
 *
 * ── three implementations, on purpose ───────────────────────────────────────
 *
 * The plan asks for a C RVV-intrinsic example for the handoff, and for "a known
 * assembly/inline-assembly reference so a compiler-codegen failure can be
 * distinguished from a simulator failure". Both are here, and so is a scalar
 * golden result, because two vector paths agreeing with each other proves less
 * than either agreeing with scalar arithmetic:
 *
 *   scalar   `g[i] = a[i] + b[i]`, ordinary C, the reference answer
 *   intrinsics  `__riscv_v*` from <riscv_vector.h> — the compiler's codegen
 *   inline asm  hand-written `vsetvli`/`vle32.v`/`vadd.vv`/`vse32.v`
 *
 * A failure is then attributable. Intrinsics wrong and asm right is a compiler
 * defect; both wrong the same way is the model; both wrong differently is this
 * program. That distinction is the entire reason a compiler team is given a
 * simulator, so it is built into the example rather than left to whoever
 * debugs it.
 *
 * ── "RVV=1.0" is a measurement, not a string ────────────────────────────────
 *
 * There is no CSR that reports the vector-extension version, so the line has to
 * be earned from behaviour that changed between 0.x and 1.0. This image uses
 * the reserved-`vsew` rule: in RVV 1.0 a `vsetvl` naming an unsupported vtype
 * must set `vtype.vill` and write `vl = 0` *without* raising an illegal
 * instruction (v-spec 1.0 §3.4.3). A 0.x-era model traps instead. The line is
 * printed only after that behaviour is observed, `misa.V` is set, and `vlenb`
 * matches the frozen VLEN.
 *
 * ── and the vector traffic is checked, not assumed ──────────────────────────
 *
 * The intrinsic loop is bracketed by a measurement window (see
 * `compiler_vp/host_io_map.h`, block D) and declares the number of RAM data
 * accesses it must have caused: two loads and one store per element. VP++
 * issues vector memory access element-wise, so that number is exact and the
 * platform enforces it. A model that serviced a whole vector register from one
 * transaction, or from a direct pointer, would move 1/16th of the traffic and
 * fail the run — which is the "no direct pointer from the CPU backend to RAM"
 * rule, checked from the guest side.
 */

#include <stdint.h>

#include <riscv_vector.h>

#include "host_io.h"

/* Frozen by the compiler contract; `vlenb` is checked against them at runtime
 * rather than trusted. */
#define EXPECTED_VLEN_BITS 512u
#define EXPECTED_VLENB 64u
#define EXPECTED_ELEN_BITS 64u

/* 1024 elements is four vector registers' worth per strip at e32,m1 and 64
 * strips, so the loop runs often enough for a per-strip defect to show up, and
 * the buffers still fit comfortably in the smallest RAM the platform accepts. */
#define N 1024u

enum {
    CHECK_PASS = 0,
    CHECK_IDENTITY = 1,
    CHECK_MISA_VECTOR = 2,
    CHECK_VLENB = 3,
    CHECK_VLEN_AGREEMENT = 4,
    CHECK_ELEN = 5,
    CHECK_VILL_NOT_SET = 6,
    CHECK_VILL_VL = 7,
    CHECK_VILL_TRAPPED = 8,
    CHECK_STRIP_LENGTH = 9,
    CHECK_INTRINSIC_RESULT = 10,
    CHECK_ASM_RESULT = 11
};

static uint32_t src_a[N] __attribute__((section(".vdata"), aligned(64)));
static uint32_t src_b[N] __attribute__((section(".vdata"), aligned(64)));
static uint32_t dst_intrinsic[N] __attribute__((section(".vdata"), aligned(64)));
static uint32_t dst_asm[N] __attribute__((section(".vdata"), aligned(64)));
static uint32_t golden[N] __attribute__((section(".vdata"), aligned(64)));

static void fail(uint32_t check, const char *what, uint32_t got,
                 uint32_t expected)
{
    hio_puts("VECTOR ADD: FAIL (");
    hio_puts(what);
    hio_puts(" is ");
    hio_put_hex32(got);
    hio_puts(", expected ");
    hio_put_hex32(expected);
    hio_putline(")");
    hio_exit(check);
}

static void fail_at(uint32_t check, const char *what, uint32_t index,
                    uint32_t got, uint32_t expected)
{
    hio_puts("VECTOR ADD: FAIL (");
    hio_puts(what);
    hio_puts(" at element ");
    hio_put_u32(index);
    hio_puts(" is ");
    hio_put_hex32(got);
    hio_puts(", expected ");
    hio_put_hex32(expected);
    hio_putline(")");
    hio_exit(check);
}

/* ── the reserved-vtype probe ────────────────────────────────────────────────
 *
 * `vsetvl` is used rather than `vsetvli` because the reserved `vsew` has to
 * come from a register: the immediate form encodes vtype in the instruction and
 * the assembler refuses to emit a reserved one, which would make this a
 * compile-time error instead of the runtime observation it needs to be.
 *
 * vtype layout (RVV 1.0): vlmul[2:0], vsew[5:3], vta[6], vma[7], vill[31].
 * vsew = 7 is reserved at every ELEN this platform can have.
 */
static uint32_t probe_reserved_vtype(uint32_t *vl_out)
{
    const uint32_t reserved_vtype = (7u << 3);
    uint32_t vtype;
    uint32_t vl;

    __asm__ volatile("vsetvl %0, %2, %3\n\t"
                     "csrr   %1, vtype"
                     : "=r"(vl), "=r"(vtype)
                     : "r"(16u), "r"(reserved_vtype));
    *vl_out = vl;
    return vtype;
}

/* ── the inline-assembly reference ───────────────────────────────────────────
 *
 * Deliberately written the way the ISA manual writes a strip-mined loop, with
 * no intrinsics and no compiler assistance beyond register allocation. When
 * this agrees with scalar and the intrinsic version does not, the defect is in
 * code generation.
 *
 * Both loops are `noinline`. At -O2 GCC inlines them into `main` and the two
 * implementations stop being separable — which costs the handoff its whole
 * point, because a compiler team reading the disassembly can no longer see
 * which instructions came from the intrinsics and which they wrote themselves.
 * It also makes the `verify` symbol check possible: with the symbols gone,
 * "both paths are still in the image" is not something the build can confirm.
 */
__attribute__((noinline)) static void vector_add_asm(const uint32_t *a,
                                                     const uint32_t *b,
                                                     uint32_t *c,
                                                     uint32_t count)
{
    while (count > 0u) {
        uint32_t vl;

        __asm__ volatile(
            "vsetvli %0, %4, e32, m1, ta, ma\n\t"
            "vle32.v v8, (%1)\n\t"
            "vle32.v v9, (%2)\n\t"
            "vadd.vv v10, v8, v9\n\t"
            "vse32.v v10, (%3)"
            : "=&r"(vl)
            : "r"(a), "r"(b), "r"(c), "r"(count)
            : "memory", "v8", "v9", "v10");

        a += vl;
        b += vl;
        c += vl;
        count -= vl;
    }
}

/* ── the intrinsic version: what the compiler team actually ships ─────────── */
__attribute__((noinline)) static void vector_add_intrinsic(const uint32_t *a,
                                                           const uint32_t *b,
                                                           uint32_t *c,
                                                           uint32_t count)
{
    size_t remaining = count;

    while (remaining > 0u) {
        const size_t vl = __riscv_vsetvl_e32m1(remaining);
        const vuint32m1_t va = __riscv_vle32_v_u32m1(a, vl);
        const vuint32m1_t vb = __riscv_vle32_v_u32m1(b, vl);

        __riscv_vse32_v_u32m1(c, __riscv_vadd_vv_u32m1(va, vb, vl), vl);

        a += vl;
        b += vl;
        c += vl;
        remaining -= vl;
    }
}

int main(void)
{
    const uint32_t identity = hio_read32(COMPILER_VP_ID_IDENTITY);
    const uint32_t host_vlen = hio_read32(COMPILER_VP_ID_VLEN_BITS);
    const uint32_t host_elen = hio_read32(COMPILER_VP_ID_ELEN_BITS);
    const uint32_t host_vlenb = hio_read32(COMPILER_VP_ID_VLENB);
    const uint32_t misa = hio_csr_misa();
    const uint32_t vlenb = hio_csr_vlenb();
    uint32_t vill_vl = 0;
    uint32_t vill_vtype;
    uint32_t strip;
    uint32_t i;

    if (identity != (uint32_t)COMPILER_VP_IDENTITY_VALUE) {
        fail(CHECK_IDENTITY, "host identity", identity,
             (uint32_t)COMPILER_VP_IDENTITY_VALUE);
    }
    if ((misa & HIO_MISA_BIT('v')) == 0u) {
        fail(CHECK_MISA_VECTOR, "misa.V", misa, HIO_MISA_BIT('v'));
    }

    /* The hart's own `vlenb`, then the host's view of it, then the frozen
     * contract. All three have to agree before anything is printed. */
    if (vlenb != EXPECTED_VLENB) {
        fail(CHECK_VLENB, "vlenb", vlenb, EXPECTED_VLENB);
    }
    if (host_vlenb != vlenb || host_vlen != EXPECTED_VLEN_BITS
        || host_vlen != vlenb * 8u) {
        fail(CHECK_VLEN_AGREEMENT, "host VLEN", host_vlen, vlenb * 8u);
    }
    if (host_elen != EXPECTED_ELEN_BITS) {
        fail(CHECK_ELEN, "ELEN", host_elen, EXPECTED_ELEN_BITS);
    }

    /* RVV 1.0 §3.4.3: a reserved vtype sets vill and zeroes vl; it does not
     * trap. `crt0`'s trap handler ends the run, so reaching the next line at
     * all is the "did not trap" half of the check. */
    vill_vtype = probe_reserved_vtype(&vill_vl);
    if ((vill_vtype & 0x80000000u) == 0u) {
        fail(CHECK_VILL_NOT_SET, "vtype.vill after a reserved vsew", vill_vtype,
             0x80000000u);
    }
    if (vill_vl != 0u) {
        fail(CHECK_VILL_VL, "vl after a reserved vsew", vill_vl, 0u);
    }

    /* One strip at e32,m1 must be VLEN/32 elements. This is what makes the
     * measurement window's expected access count exact. */
    strip = (uint32_t)__riscv_vsetvl_e32m1(N);
    if (strip != EXPECTED_VLEN_BITS / 32u) {
        fail(CHECK_STRIP_LENGTH, "vl at e32,m1", strip,
             EXPECTED_VLEN_BITS / 32u);
    }

    for (i = 0; i < N; ++i) {
        src_a[i] = i * 7u + 3u;
        src_b[i] = 0xA5A50000u - i;
        golden[i] = src_a[i] + src_b[i];
    }

    /* Two element loads and one element store per element, and nothing else in
     * the window. VP++ issues vector memory access one element at a time, so
     * this is the exact traffic the loop must produce; the platform refuses the
     * run if it sees less. */
    hio_mark_begin(1);
    hio_expect_accesses(3u * N);
    vector_add_intrinsic(src_a, src_b, dst_intrinsic, N);
    hio_mark_end();

    vector_add_asm(src_a, src_b, dst_asm, N);

    for (i = 0; i < N; ++i) {
        if (dst_intrinsic[i] != golden[i]) {
            fail_at(CHECK_INTRINSIC_RESULT, "intrinsic result", i,
                    dst_intrinsic[i], golden[i]);
        }
        if (dst_asm[i] != golden[i]) {
            fail_at(CHECK_ASM_RESULT, "inline-asm result", i, dst_asm[i],
                    golden[i]);
        }
    }

    hio_putline("RVV=1.0");
    hio_put_kv_u32("VLEN", vlenb * 8u);
    hio_put_kv_u32("vlenb", vlenb);
    hio_put_kv_u32("elements", N);
    hio_putline("VECTOR ADD: PASS");

    return CHECK_PASS;
}
