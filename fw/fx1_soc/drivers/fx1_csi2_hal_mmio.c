#include "fx1_csi2_hal_mmio.h"

#include "fx1_fw.h"

static uintptr_t g_base;

static uint32_t mmio_read32(void* ctx, uint32_t off)
{
    (void)ctx;
    return fx1_read32(g_base + off);
}

static void mmio_write32(void* ctx, uint32_t off, uint32_t value)
{
    (void)ctx;
    fx1_write32(g_base + off, value);
}

static void mmio_delay_us(void* ctx, uint32_t us)
{
    (void)ctx;
    const uint64_t start = fx1_mtime();
    const uint64_t wait = (uint64_t)us * (FX1_MTIME_HZ / 1000000u);
    while (fx1_mtime() - start < wait) {
    }
}

const fx1_csi2_hal* fx1_csi2_hal_mmio(uintptr_t base)
{
    static fx1_csi2_hal hal = {mmio_read32, mmio_write32, mmio_delay_us, 0};
    g_base = base;
    return &hal;
}
