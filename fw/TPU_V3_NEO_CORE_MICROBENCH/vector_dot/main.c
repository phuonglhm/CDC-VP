/* SPDX-License-Identifier: Apache-2.0
 *
 * MB2: vector dot product on one standalone NEO-CORE (§5.3).
 *
 *   result = sum(a[i] * b[i]), signed INT32
 *
 * Frozen as a reduction rather than an element-wise Hadamard product because
 * MB1 already supplies the element-wise bandwidth case. The two produce
 * different traffic — MB2 reads 2N elements and writes one — and the plan is
 * explicit that they are different experiments, not variants of one.
 *
 * The two operands arrive back to back in one source buffer: `a` at
 * `sram_src`, `b` at `sram_src + elements * 4`. One buffer keeps the DMA-in
 * leg a single transfer, which is what makes the end-to-end mode's byte
 * accounting a two-transfer structure the runner can check.
 *
 * As in MB1, this image decides nothing. It performs the reduction it was told
 * to, reports what `vsetvli` returned, and stops.
 */

#include <stdint.h>

#include "bench_map.h"

/* ── register and memory access ───────────────────────────────────────────── */

static inline void mmio_write(uint32_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
}

static inline uint32_t mmio_read(uint32_t address)
{
    return *(volatile uint32_t *)(uintptr_t)address;
}

static void mark(uint32_t stage)
{
    mmio_write(SIM_STAGE_MARK, stage);
}

#define POLL_LIMIT 2000000u

/* ── NEO DMA ──────────────────────────────────────────────────────────────── */

static uint32_t dma_copy(uint32_t source, uint32_t destination, uint32_t length)
{
    mmio_write(BENCH_DMA_CONTROL + DMA_REG_SRC_LO, source);
    mmio_write(BENCH_DMA_CONTROL + DMA_REG_SRC_HI, 0u);
    mmio_write(BENCH_DMA_CONTROL + DMA_REG_DST_LO, destination);
    mmio_write(BENCH_DMA_CONTROL + DMA_REG_DST_HI, 0u);
    mmio_write(BENCH_DMA_CONTROL + DMA_REG_LENGTH, length);
    mmio_write(BENCH_DMA_CONTROL + DMA_REG_IRQ_ENABLE, 0u);
    mmio_write(BENCH_DMA_CONTROL + DMA_REG_CONTROL, DMA_CTRL_START);

    for (uint32_t i = 0; i < POLL_LIMIT; ++i) {
        const uint32_t status = mmio_read(BENCH_DMA_CONTROL + DMA_REG_STATUS);
        if ((status & DMA_STATUS_BUSY) != 0u) {
            continue;
        }
        if ((status & DMA_STATUS_DONE) == 0u) {
            return BENCH_ERR_DMA_STATUS;
        }
        mmio_write(BENCH_DMA_CONTROL + DMA_REG_STATUS, DMA_STATUS_DONE);
        if (mmio_read(BENCH_DMA_CONTROL + DMA_REG_BYTES_DONE) != length) {
            return BENCH_ERR_DMA_BYTES;
        }
        return 0u;
    }
    return BENCH_ERR_DMA_STATUS;
}

/* ── the two implementations ──────────────────────────────────────────────────
 *
 * `bias` is 0 for a correct dot product. Its only other value is 1, under the
 * `BENCH_FAULT_CORRUPT_ARITH` control, which adds one to every product and so
 * shifts the sum by exactly N. That corrupts the arithmetic itself rather than
 * the comparison that checks it, and it is detectable at every frozen size
 * because N is never zero.
 */

static int32_t dot_scalar(uint32_t a_address, uint32_t b_address,
                          uint32_t count, int32_t bias)
{
    const volatile int32_t *a = (const volatile int32_t *)(uintptr_t)a_address;
    const volatile int32_t *b = (const volatile int32_t *)(uintptr_t)b_address;

    int32_t accumulator = 0;
    for (uint32_t i = 0; i < count; ++i) {
        accumulator += a[i] * b[i] + bias;
    }
    return accumulator;
}

/* The strip-mined RVV reduction.
 *
 * `vredsum` is seeded with zero each iteration and the partial sums are
 * accumulated in a scalar register rather than carried in the vector
 * accumulator across iterations. Both are correct; this way the scalar and
 * vector implementations perform the same additions in the same order, so a
 * disagreement between them is a defect rather than a reassociation.
 *
 * The operand bound guarantees every product and the running sum stay inside
 * INT32, so there is no saturation or widening to reason about.
 */
static int32_t dot_rvv(uint32_t a_address, uint32_t b_address, uint32_t count,
                       int32_t bias, uint32_t *vlmax_out,
                       uint32_t *vl_first_out, uint32_t *vl_last_out,
                       uint32_t *iterations_out)
{
    uint32_t vlmax;
    __asm__ volatile("vsetvli %0, x0, e32, m1, ta, ma" : "=r"(vlmax));
    *vlmax_out = vlmax;

    uint32_t remaining = count;
    uint32_t a = a_address;
    uint32_t b = b_address;
    uint32_t iterations = 0;
    uint32_t vl_first = 0;
    uint32_t vl_last = 0;
    int32_t accumulator = 0;

    while (remaining > 0u) {
        uint32_t vl;
        int32_t chunk;
        __asm__ volatile(
            "vsetvli %0, %2, e32, m1, ta, ma\n"
            "vle32.v v8, (%3)\n"
            "vle32.v v9, (%4)\n"
            "vmul.vv v8, v8, v9\n"
            "vadd.vx v8, v8, %5\n"
            "vmv.s.x v16, zero\n"
            "vredsum.vs v16, v8, v16\n"
            "vmv.x.s %1, v16\n"
            : "=&r"(vl), "=&r"(chunk)
            : "r"(remaining), "r"((uintptr_t)a), "r"((uintptr_t)b), "r"(bias)
            : "memory", "v8", "v9", "v16");

        accumulator += chunk;

        if (iterations == 0u) {
            vl_first = vl;
        }
        vl_last = vl;
        ++iterations;

        a += vl * 4u;
        b += vl * 4u;
        remaining -= vl;
    }

    *vl_first_out = vl_first;
    *vl_last_out = vl_last;
    *iterations_out = iterations;
    return accumulator;
}

static uint32_t wrapping_checksum(uint32_t address, uint32_t count)
{
    const volatile int32_t *values = (const volatile int32_t *)(uintptr_t)address;
    uint32_t sum = 0;
    for (uint32_t i = 0; i < count; ++i) {
        sum += (uint32_t)values[i];
    }
    return sum;
}

/* ── the run ──────────────────────────────────────────────────────────────── */

int main(void)
{
    mark(BENCH_STAGE_START);

    uint32_t mhartid;
    __asm__ volatile("csrr %0, mhartid" : "=r"(mhartid));
    mmio_write(SIM_MHARTID, mhartid);

    uint32_t vlenb;
    __asm__ volatile("csrr %0, vlenb" : "=r"(vlenb));
    mmio_write(SIM_VLENB, vlenb);

    mark(BENCH_STAGE_PARAMS);
    const volatile uint32_t *params
        = (const volatile uint32_t *)(uintptr_t)BENCH_PARAMS_ADDR;

    if (params[BENCH_PARAM_OFF_MAGIC / 4u] != BENCH_PARAM_MAGIC) {
        return BENCH_ERR_MAGIC;
    }
    const uint32_t benchmark = params[BENCH_PARAM_OFF_BENCHMARK / 4u];
    const uint32_t implementation = params[BENCH_PARAM_OFF_IMPL / 4u];
    const uint32_t mode = params[BENCH_PARAM_OFF_MODE / 4u];
    const uint32_t elements = params[BENCH_PARAM_OFF_ELEMENTS / 4u];
    const uint32_t in_bytes = params[BENCH_PARAM_OFF_IN_BYTES / 4u];
    const uint32_t out_bytes = params[BENCH_PARAM_OFF_OUT_BYTES / 4u];
    const uint32_t faults = params[BENCH_PARAM_OFF_FAULT_FLAGS / 4u];
    const uint32_t host_src = params[BENCH_PARAM_OFF_HOST_SRC / 4u];
    const uint32_t host_dst = params[BENCH_PARAM_OFF_HOST_DST / 4u];
    const uint32_t sram_src = params[BENCH_PARAM_OFF_SRAM_SRC / 4u];
    const uint32_t sram_dst = params[BENCH_PARAM_OFF_SRAM_DST / 4u];

    mmio_write(SIM_PARAM_ECHO, elements);

    /* This image is MB2 and nothing else. A runner that pointed the MB1 ELF at
     * a vector_dot case, or the reverse, is caught here rather than producing a
     * result for the benchmark nobody asked for. */
    if (benchmark != BENCH_ID_VECTOR_DOT) {
        return BENCH_ERR_BENCHMARK;
    }
    /* Two operand vectors in, one INT32 out. */
    if (elements == 0u || elements > BENCH_MAX_ELEMENTS
        || in_bytes != elements * 8u) {
        return BENCH_ERR_ELEMENTS;
    }
    if (out_bytes != 4u) {
        return BENCH_ERR_OUT_BYTES;
    }
    if (implementation != BENCH_IMPL_SCALAR
        && implementation != BENCH_IMPL_RVV) {
        return BENCH_ERR_IMPL;
    }
    if (mode != BENCH_MODE_KERNEL && mode != BENCH_MODE_E2E) {
        return BENCH_ERR_MODE;
    }
    if (mode == BENCH_MODE_E2E
        && mmio_read(BENCH_DMA_CONTROL + DMA_REG_ID) == 0u) {
        return BENCH_ERR_DMA_ID;
    }

    const int32_t bias = (faults & BENCH_FAULT_CORRUPT_ARITH) != 0u ? 1 : 0;
    const uint32_t a_address = sram_src;
    const uint32_t b_address = sram_src + elements * 4u;

    mmio_write(SIM_MEASURE_BEGIN, 1u);

    if (mode == BENCH_MODE_E2E
        && (faults & BENCH_FAULT_SKIP_DMA_IN) == 0u) {
        mark(BENCH_STAGE_DMA_IN);
        const uint32_t failed = dma_copy(host_src, sram_src, in_bytes);
        if (failed != 0u) {
            return failed;
        }
    }

    mark(BENCH_STAGE_KERNEL);
    uint32_t vlmax = 0;
    uint32_t vl_first = 0;
    uint32_t vl_last = 0;
    uint32_t iterations = 0;
    int32_t result;
    if (implementation == BENCH_IMPL_RVV) {
        result = dot_rvv(a_address, b_address, elements, bias, &vlmax,
                         &vl_first, &vl_last, &iterations);
    } else {
        result = dot_scalar(a_address, b_address, elements, bias);
    }
    *(volatile int32_t *)(uintptr_t)sram_dst = result;

    if (mode == BENCH_MODE_E2E
        && (faults & BENCH_FAULT_SKIP_DMA_OUT) == 0u) {
        mark(BENCH_STAGE_DMA_OUT);
        const uint32_t failed = dma_copy(sram_dst, host_dst, out_bytes);
        if (failed != 0u) {
            return failed;
        }
    }

    if ((faults & BENCH_FAULT_SKIP_END_MARK) == 0u) {
        mmio_write(SIM_MEASURE_END, 1u);
    }

    mark(BENCH_STAGE_VERIFY);
    mmio_write(SIM_VLMAX, vlmax);
    mmio_write(SIM_VL_FIRST, vl_first);
    mmio_write(SIM_VL_LAST, vl_last);
    mmio_write(SIM_VECTOR_ITERS, iterations);
    mmio_write(SIM_GUEST_ELEMENTS, elements);
    mmio_write(SIM_GUEST_CHECKSUM, wrapping_checksum(sram_dst, out_bytes / 4u));

    mark(BENCH_STAGE_DONE);
    return SIM_EXIT_PASS;
}
