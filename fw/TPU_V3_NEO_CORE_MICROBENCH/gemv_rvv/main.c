/* SPDX-License-Identifier: Apache-2.0
 * MB3 RVV: A[1 x K] * B[K x N] -> C[1 x N], signed INT8 multiply and exact
 * INT32 accumulation. B is row-major, so each column uses a strided load.
 */
#include <stddef.h>
#include <stdint.h>
#include <riscv_vector.h>
#include "bench_map.h"

static inline void mmio_write(uint32_t a, uint32_t v)
{ *(volatile uint32_t *)(uintptr_t)a = v; }
static inline uint32_t mmio_read(uint32_t a)
{ return *(volatile uint32_t *)(uintptr_t)a; }
static void mark(uint32_t stage) { mmio_write(SIM_STAGE_MARK, stage); }
#define POLL_LIMIT 2000000u

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
        if ((status & DMA_STATUS_BUSY) != 0u) continue;
        if ((status & DMA_STATUS_DONE) == 0u) return BENCH_ERR_DMA_STATUS;
        mmio_write(BENCH_DMA_CONTROL + DMA_REG_STATUS, DMA_STATUS_DONE);
        if (mmio_read(BENCH_DMA_CONTROL + DMA_REG_BYTES_DONE) != length)
            return BENCH_ERR_DMA_BYTES;
        return 0u;
    }
    return BENCH_ERR_DMA_STATUS;
}

static void run_gemv(const int8_t *a, const int8_t *b, volatile int32_t *c,
                     uint32_t n, uint32_t k, uint32_t *vlmax_out,
                     uint32_t *vl_first_out, uint32_t *vl_last_out,
                     uint32_t *iterations_out)
{
    const uint32_t vlmax = (uint32_t)__riscv_vsetvlmax_e8m1();
    uint32_t first = 0, last = 0, iterations = 0;
    for (uint32_t column = 0; column < n; ++column) {
        const int8_t *a_cursor = a;
        const int8_t *b_cursor = b + column;
        uint32_t remaining = k;
        int32_t total = 0;
        while (remaining != 0u) {
            const size_t vl = __riscv_vsetvl_e8m1(remaining);
            const vint8m1_t va = __riscv_vle8_v_i8m1(a_cursor, vl);
            const vint8m1_t vb
                = __riscv_vlse8_v_i8m1(b_cursor, (ptrdiff_t)n, vl);
            const vint16m2_t va16 = __riscv_vsext_vf2_i16m2(va, vl);
            const vint16m2_t vb16 = __riscv_vsext_vf2_i16m2(vb, vl);
            const vint32m4_t products
                = __riscv_vwmul_vv_i32m4(va16, vb16, vl);
            const vint32m1_t zero = __riscv_vmv_v_x_i32m1(0, 1);
            const vint32m1_t reduced
                = __riscv_vredsum_vs_i32m4_i32m1(products, zero, vl);
            total += __riscv_vmv_x_s_i32m1_i32(reduced);
            if (iterations == 0u) first = (uint32_t)vl;
            last = (uint32_t)vl;
            ++iterations;
            a_cursor += vl;
            b_cursor += vl * n;
            remaining -= (uint32_t)vl;
        }
        c[column] = total;
    }
    *vlmax_out = vlmax;
    *vl_first_out = first;
    *vl_last_out = last;
    *iterations_out = iterations;
}

static uint32_t checksum(uint32_t address, uint32_t count)
{
    const volatile int32_t *v = (const volatile int32_t *)(uintptr_t)address;
    uint32_t sum = 0;
    for (uint32_t i = 0; i < count; ++i) sum += (uint32_t)v[i];
    return sum;
}

int main(void)
{
    mark(BENCH_STAGE_START);
    uint32_t mhartid, vlenb;
    __asm__ volatile("csrr %0, mhartid" : "=r"(mhartid));
    __asm__ volatile("csrr %0, vlenb" : "=r"(vlenb));
    mmio_write(SIM_MHARTID, mhartid);
    mmio_write(SIM_VLENB, vlenb);
    mark(BENCH_STAGE_PARAMS);
    const volatile uint32_t *p
        = (const volatile uint32_t *)(uintptr_t)BENCH_PARAMS_ADDR;
    if (p[BENCH_PARAM_OFF_MAGIC / 4u] != BENCH_PARAM_MAGIC)
        return BENCH_ERR_MAGIC;
    const uint32_t benchmark = p[BENCH_PARAM_OFF_BENCHMARK / 4u];
    const uint32_t implementation = p[BENCH_PARAM_OFF_IMPL / 4u];
    const uint32_t mode = p[BENCH_PARAM_OFF_MODE / 4u];
    const uint32_t in_bytes = p[BENCH_PARAM_OFF_IN_BYTES / 4u];
    const uint32_t out_bytes = p[BENCH_PARAM_OFF_OUT_BYTES / 4u];
    const uint32_t faults = p[BENCH_PARAM_OFF_FAULT_FLAGS / 4u];
    const uint32_t host_src = p[BENCH_PARAM_OFF_HOST_SRC / 4u];
    const uint32_t host_dst = p[BENCH_PARAM_OFF_HOST_DST / 4u];
    const uint32_t sram_src = p[BENCH_PARAM_OFF_SRAM_SRC / 4u];
    const uint32_t sram_dst = p[BENCH_PARAM_OFF_SRAM_DST / 4u];
    const uint32_t m = p[BENCH_PARAM_OFF_M / 4u];
    const uint32_t n = p[BENCH_PARAM_OFF_N / 4u];
    const uint32_t k = p[BENCH_PARAM_OFF_K / 4u];
    const uint32_t output_elements = m * n;
    mmio_write(SIM_PARAM_ECHO, output_elements);
    if (benchmark != BENCH_ID_GEMV_RVV) return BENCH_ERR_BENCHMARK;
    if (implementation != BENCH_IMPL_RVV) return BENCH_ERR_IMPL;
    if (mode != BENCH_MODE_KERNEL && mode != BENCH_MODE_E2E)
        return BENCH_ERR_MODE;
    if (m != 1u || n == 0u || n > BENCH_MXU_COLUMNS || k == 0u
        || k > BENCH_MATRIX_MAX_K)
        return BENCH_ERR_SHAPE;
    if (in_bytes != m * k + k * n || out_bytes != output_elements * 4u)
        return BENCH_ERR_OUT_BYTES;
    if (mode == BENCH_MODE_E2E
        && mmio_read(BENCH_DMA_CONTROL + DMA_REG_ID) == 0u)
        return BENCH_ERR_DMA_ID;

    mmio_write(SIM_MEASURE_BEGIN, 1u);
    if (mode == BENCH_MODE_E2E
        && (faults & BENCH_FAULT_SKIP_DMA_IN) == 0u) {
        mark(BENCH_STAGE_DMA_IN);
        const uint32_t failed = dma_copy(host_src, sram_src, in_bytes);
        if (failed != 0u) return failed;
    }
    mark(BENCH_STAGE_KERNEL);
    const uint32_t effective_k
        = (faults & BENCH_FAULT_CORRUPT_ARITH) != 0u ? k - 1u : k;
    const int8_t *a = (const int8_t *)(uintptr_t)sram_src;
    const int8_t *b = (const int8_t *)(uintptr_t)(sram_src + m * k);
    volatile int32_t *c = (volatile int32_t *)(uintptr_t)sram_dst;
    uint32_t vlmax = 0, vl_first = 0, vl_last = 0, iterations = 0;
    run_gemv(a, b, c, n, effective_k, &vlmax, &vl_first, &vl_last,
             &iterations);
    if (mode == BENCH_MODE_E2E
        && (faults & BENCH_FAULT_SKIP_DMA_OUT) == 0u) {
        mark(BENCH_STAGE_DMA_OUT);
        const uint32_t failed = dma_copy(sram_dst, host_dst, out_bytes);
        if (failed != 0u) return failed;
    }
    if ((faults & BENCH_FAULT_SKIP_END_MARK) == 0u)
        mmio_write(SIM_MEASURE_END, 1u);
    mark(BENCH_STAGE_VERIFY);
    mmio_write(SIM_VLMAX, vlmax);
    mmio_write(SIM_VL_FIRST, vl_first);
    mmio_write(SIM_VL_LAST, vl_last);
    mmio_write(SIM_VECTOR_ITERS, iterations);
    mmio_write(SIM_GUEST_ELEMENTS, output_elements);
    mmio_write(SIM_GUEST_CHECKSUM, checksum(sram_dst, output_elements));
    mark(BENCH_STAGE_DONE);
    return SIM_EXIT_PASS;
}
