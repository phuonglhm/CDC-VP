#include "isp_common.h"

#define IN_SLOT  0x2000u
#define OUT_SLOT 0x2000u /* Y at +0, UV at +0x1000 */

static uint32_t hal_read32(void* ctx, uint32_t offset)
{
    (void)ctx;
    return fx1_read32(FX1_ISP_CSR_BASE + offset);
}

static void hal_write32(void* ctx, uint32_t offset, uint32_t value)
{
    (void)ctx;
    fx1_write32(FX1_ISP_CSR_BASE + offset, value);
}

static void hal_delay_cycles(void* ctx, uint32_t cycles)
{
    (void)ctx;
    /* ISP core clock: 2 ns (fx1_isp_params::core_period); mtime tick: 50 ns. */
    const uint64_t ticks = ((uint64_t)cycles * 2u + 49u) / 50u + 1u;
    const uint64_t start = fx1_mtime();
    while (fx1_mtime() - start < ticks) {
    }
}

const fx1_isp_hal isp_hal = {hal_read32, hal_write32, hal_delay_cycles, 0};

void isp_buffers(fx1_isp_buffers* bufs)
{
    for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i) {
        bufs->in[i] = FX1_FW_RAW_BUF_BASE + i * IN_SLOT;
        bufs->y[i] = FX1_FW_YUV_BUF_BASE + i * OUT_SLOT;
        bufs->uv[i] = bufs->y[i] + 0x1000u;
    }
    bufs->in_stride = ISP_IN_STRIDE;
    bufs->y_stride = bufs->uv_stride = ISP_OUT_STRIDE;
    bufs->max_burst_beats = 16;
}

uint32_t isp_raw_sample(uint32_t frame, uint32_t x, uint32_t y)
{
    uint32_t v = 400u + 160u * frame;
    v += (x * 37u + y * 91u + frame * 53u) % 512u;
    v += ((x ^ y) & 7u) * 24u;
    if ((x + 2u * frame) % 16u < 3u) v += 900u;
    if ((x * 7u + y * 13u + frame) % 211u == 0u) v = 4095u;
    return v > 4095u ? 4095u : v;
}

void isp_fill_input(uint64_t base, uint32_t frame)
{
    for (uint32_t y = 0; y < ISP_FX_HEIGHT; ++y) {
        volatile uint16_t* line = (volatile uint16_t*)(uintptr_t)(base + y * ISP_IN_STRIDE);
        for (uint32_t x = 0; x < ISP_FX_WIDTH; ++x) line[x] = (uint16_t)isp_raw_sample(frame, x, y);
    }
    fx1_dma_prepare((const volatile void*)(uintptr_t)base, ISP_IN_STRIDE * ISP_FX_HEIGHT, FX1_DMA_TO_DEVICE);
}

void isp_output_to_device(uint64_t y, uint64_t uv)
{
    fx1_dma_prepare((const volatile void*)(uintptr_t)y, ISP_OUT_STRIDE * ISP_FX_HEIGHT, FX1_DMA_FROM_DEVICE);
    fx1_dma_prepare((const volatile void*)(uintptr_t)uv, ISP_OUT_STRIDE * ISP_FX_HEIGHT / 2, FX1_DMA_FROM_DEVICE);
}

void isp_outputs_to_device(const fx1_isp_buffers* bufs)
{
    for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i) isp_output_to_device(bufs->y[i], bufs->uv[i]);
}

void isp_output_to_cpu(uint64_t y, uint64_t uv)
{
    fx1_dma_complete((const volatile void*)(uintptr_t)y, ISP_OUT_STRIDE * ISP_FX_HEIGHT, FX1_DMA_FROM_DEVICE);
    fx1_dma_complete((const volatile void*)(uintptr_t)uv, ISP_OUT_STRIDE * ISP_FX_HEIGHT / 2, FX1_DMA_FROM_DEVICE);
}

static uint32_t crc32_update(uint32_t crc, const volatile uint8_t* p, uint32_t n)
{
    static uint32_t table[256];
    if (!table[1]) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    }
    for (uint32_t i = 0; i < n; ++i) crc = table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
    return crc;
}

uint32_t isp_nv12_crc(uint64_t y, uint64_t uv)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t row = 0; row < ISP_FX_HEIGHT; ++row)
        crc = crc32_update(crc, (const volatile uint8_t*)(uintptr_t)(y + row * ISP_OUT_STRIDE), ISP_FX_WIDTH);
    for (uint32_t row = 0; row < ISP_FX_HEIGHT / 2; ++row)
        crc = crc32_update(crc, (const volatile uint8_t*)(uintptr_t)(uv + row * ISP_OUT_STRIDE), ISP_FX_WIDTH);
    return crc ^ 0xFFFFFFFFu;
}
