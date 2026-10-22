#include "fx1_isp_hal_mmio.h"

#include "fx1_fw.h"

static uint32_t mmio_read32(void* ctx, uint32_t offset)
{
    (void)ctx;
    return fx1_read32(FX1_ISP_CSR_BASE + offset);
}

static void mmio_write32(void* ctx, uint32_t offset, uint32_t value)
{
    (void)ctx;
    fx1_write32(FX1_ISP_CSR_BASE + offset, value);
}

static void mmio_delay_cycles(void* ctx, uint32_t cycles)
{
    (void)ctx;
    /* ISP core clock 2 ns (fx1_isp_params::core_period); mtime tick 50 ns. */
    const uint64_t ticks = ((uint64_t)cycles * 2u + 49u) / 50u + 1u;
    const uint64_t start = fx1_mtime();
    while (fx1_mtime() - start < ticks) {
    }
}

const fx1_isp_hal* fx1_isp_hal_mmio(void)
{
    static const fx1_isp_hal hal = {mmio_read32, mmio_write32, mmio_delay_cycles, 0};
    return &hal;
}
