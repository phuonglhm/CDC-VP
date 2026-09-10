/* SPDX-License-Identifier: Apache-2.0
 * Shared MXU driver body for MB3/MXU and MB4. Each Makefile supplies a distinct
 * BENCH_EXPECTED_ID, so each ELF refuses the other benchmark identifier.
 */
#include <stdint.h>
#include "bench_map.h"
#ifndef BENCH_EXPECTED_ID
#error "BENCH_EXPECTED_ID must name this firmware image"
#endif
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

static uint32_t matrix_multiply(uint32_t a, uint32_t b, uint32_t c,
                                uint32_t m, uint32_t n, uint32_t programmed_k,
                                uint32_t a_stride)
{
    mmio_write(BENCH_SA_CONTROL + SA_REG_DIM_M, m);
    mmio_write(BENCH_SA_CONTROL + SA_REG_DIM_N, n);
    mmio_write(BENCH_SA_CONTROL + SA_REG_DIM_K, programmed_k);
    mmio_write(BENCH_SA_CONTROL + SA_REG_A_LO, a);
    mmio_write(BENCH_SA_CONTROL + SA_REG_A_HI, 0u);
    mmio_write(BENCH_SA_CONTROL + SA_REG_B_LO, b);
    mmio_write(BENCH_SA_CONTROL + SA_REG_B_HI, 0u);
    mmio_write(BENCH_SA_CONTROL + SA_REG_C_LO, c);
    mmio_write(BENCH_SA_CONTROL + SA_REG_C_HI, 0u);
    mmio_write(BENCH_SA_CONTROL + SA_REG_A_STRIDE, a_stride);
    mmio_write(BENCH_SA_CONTROL + SA_REG_B_STRIDE, n);
    mmio_write(BENCH_SA_CONTROL + SA_REG_C_STRIDE, n * 4u);
    mmio_write(BENCH_SA_CONTROL + SA_REG_DATATYPE, SA_DATATYPE_INT8_INT32);
    mmio_write(BENCH_SA_CONTROL + SA_REG_IRQ_ENABLE, 0u);
    mmio_write(BENCH_SA_CONTROL + SA_REG_CONTROL, SA_CTRL_START);
    for (uint32_t i = 0; i < POLL_LIMIT; ++i) {
        const uint32_t status = mmio_read(BENCH_SA_CONTROL + SA_REG_STATUS);
        if ((status & SA_STATUS_BUSY) != 0u) continue;
        if ((status & SA_STATUS_DONE) == 0u
            || (status & SA_STATUS_ERROR) != 0u
            || mmio_read(BENCH_SA_CONTROL + SA_REG_ERROR_CAUSE) != 0u)
            return BENCH_ERR_MXU_STATUS;
        mmio_write(BENCH_SA_CONTROL + SA_REG_STATUS, SA_STATUS_DONE);
        return 0u;
    }
    return BENCH_ERR_MXU_STATUS;
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
    if (benchmark != BENCH_EXPECTED_ID) return BENCH_ERR_BENCHMARK;
    if (implementation != BENCH_IMPL_MXU) return BENCH_ERR_IMPL;
    if (mode != BENCH_MODE_KERNEL && mode != BENCH_MODE_E2E)
        return BENCH_ERR_MODE;
    if (m == 0u || m > BENCH_MXU_ROWS || n == 0u
        || n > BENCH_MXU_COLUMNS || k == 0u || k > BENCH_MATRIX_MAX_K)
        return BENCH_ERR_SHAPE;
#if BENCH_EXPECTED_ID == BENCH_ID_GEMV_MXU
    if (m != 1u) return BENCH_ERR_SHAPE;
#endif
    if (in_bytes != m * k + k * n || out_bytes != output_elements * 4u)
        return BENCH_ERR_OUT_BYTES;
    if (mmio_read(BENCH_SA_CONTROL + SA_REG_ID) == 0u)
        return BENCH_ERR_MXU_ID;
    if (mmio_read(BENCH_SA_CONTROL + SA_REG_GEOMETRY)
            != ((BENCH_MXU_ROWS << 16) | BENCH_MXU_COLUMNS)
        || (mmio_read(BENCH_SA_CONTROL + SA_REG_CAPABILITY)
            & SA_CAP_INT8_INT32) == 0u)
        return BENCH_ERR_MXU_CAP;
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
    const uint32_t programmed_k
        = (faults & BENCH_FAULT_CORRUPT_ARITH) != 0u ? k - 1u : k;
    const uint32_t b = sram_src + m * k;
    uint32_t failed = matrix_multiply(sram_src, b, sram_dst, m, n,
                                      programmed_k, k);
    if (failed != 0u) return failed;
    if (mode == BENCH_MODE_E2E
        && (faults & BENCH_FAULT_SKIP_DMA_OUT) == 0u) {
        mark(BENCH_STAGE_DMA_OUT);
        failed = dma_copy(sram_dst, host_dst, out_bytes);
        if (failed != 0u) return failed;
    }
    if ((faults & BENCH_FAULT_SKIP_END_MARK) == 0u)
        mmio_write(SIM_MEASURE_END, 1u);
    mark(BENCH_STAGE_VERIFY);
    mmio_write(SIM_VLMAX, 0u);
    mmio_write(SIM_VL_FIRST, 0u);
    mmio_write(SIM_VL_LAST, 0u);
    mmio_write(SIM_VECTOR_ITERS, 0u);
    mmio_write(SIM_GUEST_ELEMENTS, output_elements);
    mmio_write(SIM_GUEST_CHECKSUM, checksum(sram_dst, output_elements));
    mark(BENCH_STAGE_DONE);
    return SIM_EXIT_PASS;
}
