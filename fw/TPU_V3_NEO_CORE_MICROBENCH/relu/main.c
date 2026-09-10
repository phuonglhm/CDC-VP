/* SPDX-License-Identifier: Apache-2.0
 *
 * MB1: ReLU on one standalone NEO-CORE (§5.2).
 *
 *   y[i] = max(x[i], 0), signed INT32
 *
 * One image runs every case of the benchmark. The shape, implementation, mode
 * and buffer addresses arrive in a parameter block the runner stages in global
 * RAM, so the ten frozen sizes and the two implementations share one ELF — and
 * therefore one recorded ELF hash. Ten compiled images would put a different
 * hash in every row and make §8.1's identity field describe the case rather
 * than the code.
 *
 * What this image does *not* do is decide anything. It performs the operation
 * it was told to, reports what `vsetvli` actually returned, and stops. The
 * comparison against the golden happens on the host, from the same seed, in
 * code that cannot reach the core (§5.1).
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

/* A bound, not a timeout policy. An engine that never leaves BUSY is a defect,
 * and spinning forever would present it as a hung simulation instead. */
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
        /* W1C the completion so the next transfer starts from a clean status,
         * exactly as the asynchronous contract requires. */
        mmio_write(BENCH_DMA_CONTROL + DMA_REG_STATUS, DMA_STATUS_DONE);
        /* `BYTES_DONE` counts destination bytes committed, never bytes fetched
         * into a staging buffer, so this is the one number that says the
         * transfer actually landed. */
        if (mmio_read(BENCH_DMA_CONTROL + DMA_REG_BYTES_DONE) != length) {
            return BENCH_ERR_DMA_BYTES;
        }
        return 0u;
    }
    return BENCH_ERR_DMA_STATUS;
}

/* ── the two implementations ──────────────────────────────────────────────────
 *
 * `floor_value` is 0 for ReLU. The only other value it ever takes is 1, under
 * the `BENCH_FAULT_CORRUPT_ARITH` control, which corrupts the arithmetic
 * itself rather than the comparison that checks it — a control that perturbed
 * the golden would prove only that the comparison compares.
 */

static void relu_scalar(uint32_t source, uint32_t destination, uint32_t count,
                        int32_t floor_value)
{
    const volatile int32_t *in = (const volatile int32_t *)(uintptr_t)source;
    volatile int32_t *out = (volatile int32_t *)(uintptr_t)destination;

    for (uint32_t i = 0; i < count; ++i) {
        const int32_t value = in[i];
        out[i] = value > floor_value ? value : floor_value;
    }
}

/* The strip-mined RVV form, and the source of every vector number in the row.
 *
 * `vsetvli` and the body are one asm block on purpose: split into two, the
 * compiler is free to place something between them, and anything that changed
 * `vtype` in that gap would make the loop operate on a width nobody asked for.
 *
 * `vl` is reported rather than assumed. §8.2 wants active elements, tail
 * elements and lane utilization; deriving them from a VLEN the *host* believes
 * in would be deriving them from a configuration value, not from the machine.
 * What the guest reports here is what the vector unit actually granted.
 */
static void relu_rvv(uint32_t source, uint32_t destination, uint32_t count,
                     int32_t floor_value, uint32_t *vlmax_out,
                     uint32_t *vl_first_out, uint32_t *vl_last_out,
                     uint32_t *iterations_out)
{
    uint32_t vlmax;
    __asm__ volatile("vsetvli %0, x0, e32, m1, ta, ma" : "=r"(vlmax));
    *vlmax_out = vlmax;

    uint32_t remaining = count;
    uint32_t in = source;
    uint32_t out = destination;
    uint32_t iterations = 0;
    uint32_t vl_first = 0;
    uint32_t vl_last = 0;

    while (remaining > 0u) {
        uint32_t vl;
        __asm__ volatile(
            "vsetvli %0, %1, e32, m1, ta, ma\n"
            "vle32.v v8, (%2)\n"
            "vmax.vx v8, v8, %3\n"
            "vse32.v v8, (%4)\n"
            : "=&r"(vl)
            : "r"(remaining), "r"((uintptr_t)in), "r"(floor_value),
              "r"((uintptr_t)out)
            : "memory", "v8");

        if (iterations == 0u) {
            vl_first = vl;
        }
        vl_last = vl;
        ++iterations;

        in += vl * 4u;
        out += vl * 4u;
        remaining -= vl;
    }

    *vl_first_out = vl_first;
    *vl_last_out = vl_last;
    *iterations_out = iterations;
}

/* Wrapping unsigned accumulation over the result, computed with ordinary
 * scalar loads after the measured interval has closed.
 *
 * Wrapping, and the host does the same: the `boundary` input pattern contains
 * INT32_MAX and a signed sum of those is undefined behaviour. This is a
 * cross-check on what the guest itself sees in core SRAM, not the correctness
 * result — that is the element-by-element comparison the runner performs.
 */
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

    /* ── the parameter block ─────────────────────────────────────────────── */

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
    const uint32_t bytes = params[BENCH_PARAM_OFF_IN_BYTES / 4u];
    const uint32_t out_bytes = params[BENCH_PARAM_OFF_OUT_BYTES / 4u];
    const uint32_t faults = params[BENCH_PARAM_OFF_FAULT_FLAGS / 4u];
    const uint32_t host_src = params[BENCH_PARAM_OFF_HOST_SRC / 4u];
    const uint32_t host_dst = params[BENCH_PARAM_OFF_HOST_DST / 4u];
    const uint32_t sram_src = params[BENCH_PARAM_OFF_SRAM_SRC / 4u];
    const uint32_t sram_dst = params[BENCH_PARAM_OFF_SRAM_DST / 4u];

    /* Echoed so the runner can prove the block it staged is the block the guest
     * read. A parameter that never arrived would otherwise present as a
     * mysteriously wrong result. */
    mmio_write(SIM_PARAM_ECHO, elements);

    if (benchmark != BENCH_ID_RELU) {
        return BENCH_ERR_BENCHMARK;
    }
    if (elements == 0u || elements > BENCH_MAX_ELEMENTS
        || bytes != elements * 4u) {
        return BENCH_ERR_ELEMENTS;
    }
    /* ReLU is element-wise: it produces exactly what it consumes. Checked
     * rather than assumed, so a runner that staged a mismatched pair is caught
     * here instead of in a DMA leg of the wrong length. */
    if (out_bytes != bytes) {
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
        /* A zero identity means nothing is bound at that window: a wiring
         * failure, not a workload one, and worth separating because every
         * later error would look like a broken DMA. */
        return BENCH_ERR_DMA_ID;
    }

    const int32_t floor_value
        = (faults & BENCH_FAULT_CORRUPT_ARITH) != 0u ? 1 : 0;

    /* ── the measured interval ───────────────────────────────────────────────
     *
     * Opens here and closes after the last transport leg. Host debug writes
     * that placed the inputs are already done and are outside it (§6.1); the
     * checksum below is verification and is outside it too.
     */
    mmio_write(SIM_MEASURE_BEGIN, 1u);

    if (mode == BENCH_MODE_E2E
        && (faults & BENCH_FAULT_SKIP_DMA_IN) == 0u) {
        mark(BENCH_STAGE_DMA_IN);
        const uint32_t failed = dma_copy(host_src, sram_src, bytes);
        if (failed != 0u) {
            return failed;
        }
    }

    if ((faults & BENCH_FAULT_SKIP_STAGE) == 0u) {
        mark(BENCH_STAGE_KERNEL);
    }
    uint32_t vlmax = 0;
    uint32_t vl_first = 0;
    uint32_t vl_last = 0;
    uint32_t iterations = 0;
    if (implementation == BENCH_IMPL_RVV) {
        relu_rvv(sram_src, sram_dst, elements, floor_value, &vlmax, &vl_first,
                 &vl_last, &iterations);
    } else {
        relu_scalar(sram_src, sram_dst, elements, floor_value);
    }

    /* The extra pass is inside the interval and its result is discarded, so
     * the arithmetic stays correct and only the byte counters change. */
    if ((faults & BENCH_FAULT_EXTRA_READ) != 0u) {
        const volatile int32_t *again
            = (const volatile int32_t *)(uintptr_t)sram_src;
        /* The loads are volatile, so the compiler must perform them however
         * little it can do with the result. Reporting the sum anywhere would
         * be worse than useless: the only spare host-I/O word is the parameter
         * echo the runner checks, and writing it would make this control fail
         * as a parameter mismatch instead of as the traffic excess it is. */
        int32_t discard = 0;
        for (uint32_t i = 0; i < elements; ++i) {
            discard += again[i];
        }
        (void)discard;
    }

    if (mode == BENCH_MODE_E2E
        && (faults & BENCH_FAULT_SKIP_DMA_OUT) == 0u) {
        mark(BENCH_STAGE_DMA_OUT);
        const uint32_t failed = dma_copy(sram_dst, host_dst, bytes);
        if (failed != 0u) {
            return failed;
        }
    }

    if ((faults & BENCH_FAULT_SKIP_END_MARK) == 0u) {
        mmio_write(SIM_MEASURE_END, 1u);
    }

    /* ── verification, outside the interval ──────────────────────────────── */

    mark(BENCH_STAGE_VERIFY);
    mmio_write(SIM_VLMAX, vlmax);
    mmio_write(SIM_VL_FIRST, vl_first);
    mmio_write(SIM_VL_LAST, vl_last);
    mmio_write(SIM_VECTOR_ITERS, iterations);
    mmio_write(SIM_GUEST_ELEMENTS, elements);
    mmio_write(SIM_GUEST_CHECKSUM, wrapping_checksum(sram_dst, elements));

    mark(BENCH_STAGE_DONE);
    return SIM_EXIT_PASS;
}
