#include "camera.h"

#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_csi2_hal_mmio.h"

cam_t g_cam;
volatile uint32_t g_cam_events;
volatile uint32_t g_cam_irqs;
uint32_t g_cam_handle_us;
uint64_t g_cam_event_mtime;

static const uint32_t kCtx = FX1_PLIC_CONTEXT_HART_M(0);

static void csr_wr(uint32_t off, uint32_t v) { fx1_write32(FX1_MIPI_CSR_BASE + off, v); }

void cam_delay_us(uint32_t us)
{
    const uint64_t start = fx1_mtime();
    while (fx1_mtime() - start < (uint64_t)us * (FX1_MTIME_HZ / 1000000u)) {
    }
}

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    (void)mtval;
    if (mcause != (FX1_MCAUSE_INTERRUPT | FX1_IRQ_M_EXT)) {
        fx1_log_hex("unexpected trap, mcause", mcause);
        fx1_log_hex("  mepc", mepc);
        fx1_fail(0xE00);
    }
    const uint32_t id = fx1_plic_claim(kCtx);
    if (id == FX1_IRQ_MIPI) {
        g_cam_events |= fx1_csi2_irq_handle(&g_cam.dev);
        ++g_cam_irqs;
    }
    if (id) fx1_plic_complete(kCtx, id);
    return mepc;
}

static void fill(uint32_t base)
{
    if (base < FX1_DDR_BASE || (uint64_t)base + CAM_SIDE_SIZE > (uint64_t)FX1_DDR_BASE + FX1_DDR_SIZE)
        return; /* a side outside DDR (decerr test): nothing the CPU may touch */
    for (uint32_t i = 0; i < CAM_SIDE_SIZE; i += 4u) fx1_write32(base + i, 0xA5A5A5A5u);
    fx1_dma_prepare((const volatile void*)(uintptr_t)base, CAM_SIDE_SIZE, FX1_DMA_FROM_DEVICE);
}

static void program_dma(uint32_t base0, uint32_t base1)
{
    g_cam.base[0] = base0;
    g_cam.base[1] = base1;
    fx1_csi2_dma_cfg c = {{base0, base1}, CAM_SIDE_SIZE, (uint16_t)g_cam.bpl, 0u, 255u, 0u, 0u, 0u,
                          (uint8_t)g_cam.raw16, 1u};
    fx1_check(fx1_csi2_dma_config(&g_cam.dev, &c) == FX1_CSI2_OK, 0x0D1, "camera: DMA configured");
    fx1_check(fx1_csi2_dma_start(&g_cam.dev) == FX1_CSI2_OK, 0x0D2, "camera: DMA started");
}

void cam_bringup(uint32_t bits, uint32_t lanes, int raw16, uint32_t base0, uint32_t base1)
{
    fx1_global_irq_disable();
    g_cam.mode = (fx1_ccs_mode){CAM_W, CAM_H, CAM_LINE_LENGTH, CAM_FRAME_LENGTH, (uint8_t)bits, (uint8_t)lanes};
    g_cam.raw16 = raw16;
    g_cam.bpl = raw16 ? 2u * CAM_W : (bits == 10 ? CAM_W * 5u / 4u : CAM_W * 3u / 2u);
    /* Sensor over I2C0, polled (the trap handler below owns the PLIC context). */
    fx1_check(fx1_i2c_open(&g_cam.bus, FX1_I2C0_BASE, 0, FX1_IRQ_I2C0, kCtx) == FX1_I2C_OK, 0x0A0, "I2C0 open");
    fx1_check(fx1_ccs_probe(&g_cam.sensor, &g_cam.bus, FX1_CCS_ADDR, 2000) == FX1_CCS_OK, 0x0A1,
              "camera: sensor model_id 0xF1C5");
    fx1_check(fx1_ccs_set_mode(&g_cam.sensor, &g_cam.mode) == FX1_CCS_OK, 0x0A2, "camera: sensor mode set");
    /* CSI-2 RX */
    fx1_check(fx1_csi2_probe(&g_cam.dev, fx1_csi2_hal_mmio(FX1_MIPI_CSR_BASE)) == FX1_CSI2_OK, 0x0B0,
              "camera: CSI-2 RX probed");
    fx1_check(fx1_csi2_phy_start(&g_cam.dev, lanes, 2000) == FX1_CSI2_OK, 0x0B1, "camera: D-PHY started");
    fx1_check(fx1_csi2_stream_config(&g_cam.dev, 0, 0) == FX1_CSI2_OK, 0x0B2, "camera: AXIS path off");
    csr_wr(FX1_CSI2_EXPECTED_RESOLUTION, (CAM_H << 16) | CAM_W); /* C36: before stream-on */
    fill(base0);
    fill(base1);
    program_dma(base0, base1);
    fx1_check(fx1_csi2_irq_enable(&g_cam.dev, CAM_BIT(CAM_IRQ_BUF_DONE_OK) | CAM_BIT(CAM_IRQ_BUF_DONE_ERR) |
                                                  CAM_BIT(CAM_IRQ_FRAME_DROPPED) |
                                                  CAM_BIT(CAM_IRQ_AXI_WRITE_ERROR)) == FX1_CSI2_OK,
              0x0B3, "camera: interrupts enabled");
    g_cam_events = 0;
    fx1_set_trap_handler(handler);
    fx1_plic_set_priority(FX1_IRQ_MIPI, 3);
    fx1_plic_enable(kCtx, FX1_IRQ_MIPI, 1);
    fx1_irq_enable(FX1_MIE_MEIE);
    fx1_global_irq_enable();
}

void cam_dma_restart(uint32_t base0, uint32_t base1)
{
    fill(base0);
    fill(base1);
    program_dma(base0, base1);
}

uint32_t cam_wait(uint32_t mask, uint32_t timeout_us)
{
    const uint64_t start = fx1_mtime();
    const uint64_t budget = (uint64_t)timeout_us * (FX1_MTIME_HZ / 1000000u);
    while (!(g_cam_events & mask) && fx1_mtime() - start < budget) {
    }
    const uint32_t saved = fx1_irq_save();
    const uint32_t ev = g_cam_events;
    g_cam_events = 0;
    fx1_irq_restore(saved);
    return ev;
}

int cam_side_in(fx1_csi2_buf_state state)
{
    for (int side = 0; side < 2; ++side) {
        fx1_csi2_buf_info bi;
        if (fx1_csi2_get_buffer(&g_cam.dev, (unsigned)side, &bi) == FX1_CSI2_OK && bi.state == state) return side;
    }
    return -1;
}

static uint16_t pixel(int side, uint32_t x, uint32_t y)
{
    const volatile uint8_t* line = (const volatile uint8_t*)(uintptr_t)(g_cam.base[side] + y * g_cam.bpl);
    if (g_cam.raw16) return (uint16_t)(line[2u * x] | line[2u * x + 1u] << 8);
    if (g_cam.mode.bits == 10) { /* CSI-2 RAW10: 4 MSB bytes, then the 2-bit LSBs of the group */
        const uint32_t g = x / 4u * 5u, k = x % 4u;
        return (uint16_t)((uint32_t)line[g + k] << 2 | (line[g + 4u] >> (2u * k) & 3u));
    }
    const uint32_t g = x / 2u * 3u, k = x % 2u; /* RAW12: 2 MSB bytes, then the 4-bit LSBs */
    return (uint16_t)((uint32_t)line[g + k] << 4 | (line[g + 2u] >> (4u * k) & 15u));
}

uint32_t cam_frame_number(int side)
{
    fx1_dma_complete((const volatile void*)(uintptr_t)g_cam.base[side], CAM_SIDE_SIZE, FX1_DMA_FROM_DEVICE);
    const uint16_t p = pixel(side, 0, 0);
    for (uint32_t f = 1; f < (1u << g_cam.mode.bits); ++f)
        if (fx1_ccs_pattern(f, 0, 0, g_cam.mode.bits) == p) return f;
    return 0;
}

/* Expected bytes of line y of frame f in the DMA layout. */
static void expected_line(uint32_t f, uint32_t y, uint8_t* out)
{
    const uint32_t bits = g_cam.mode.bits;
    if (g_cam.raw16) {
        for (uint32_t x = 0; x < CAM_W; ++x) {
            const uint16_t p = fx1_ccs_pattern(f, x, y, bits);
            out[2u * x] = (uint8_t)p;
            out[2u * x + 1u] = (uint8_t)(p >> 8);
        }
    } else if (bits == 10) {
        for (uint32_t x = 0; x < CAM_W; x += 4u) {
            uint8_t* g = out + x / 4u * 5u;
            g[4] = 0;
            for (uint32_t k = 0; k < 4u; ++k) {
                const uint16_t p = fx1_ccs_pattern(f, x + k, y, bits);
                g[k] = (uint8_t)(p >> 2);
                g[4] |= (uint8_t)((p & 3u) << (2u * k));
            }
        }
    } else {
        for (uint32_t x = 0; x < CAM_W; x += 2u) {
            uint8_t* g = out + x / 2u * 3u;
            const uint16_t p0 = fx1_ccs_pattern(f, x, y, bits), p1 = fx1_ccs_pattern(f, x + 1u, y, bits);
            g[0] = (uint8_t)(p0 >> 4);
            g[1] = (uint8_t)(p1 >> 4);
            g[2] = (uint8_t)((p0 & 15u) | (p1 & 15u) << 4);
        }
    }
}

int cam_check_frame(int side, uint32_t f)
{
    fx1_dma_complete((const volatile void*)(uintptr_t)g_cam.base[side], CAM_SIDE_SIZE, FX1_DMA_FROM_DEVICE);
    /* Word compares: bytes per line are a multiple of 8 (DMA stride rule). */
    static uint32_t want[2u * CAM_W / 4u];
    for (uint32_t y = 0; y < CAM_H; ++y) {
        expected_line(f, y, (uint8_t*)want);
        const volatile uint32_t* line = (const volatile uint32_t*)(uintptr_t)(g_cam.base[side] + y * g_cam.bpl);
        for (uint32_t i = 0; i < g_cam.bpl / 4u; ++i)
            if (line[i] != want[i]) {
                fx1_log_hex("  mismatch at (line << 16 | byte)", y << 16 | i * 4u);
                fx1_log_hex("  got", line[i]);
                fx1_log_hex("  want", want[i]);
                return 0;
            }
    }
    const volatile uint32_t* after = (const volatile uint32_t*)(uintptr_t)(g_cam.base[side] + CAM_H * g_cam.bpl);
    for (uint32_t i = 0; i < 16u; ++i)
        if (after[i] != 0xA5A5A5A5u) {
            fx1_log_hex("  written past the frame at offset", CAM_H * g_cam.bpl + i * 4u);
            return 0;
        }
    return 1;
}

void cam_release(int side)
{
    fill(g_cam.base[side]);
    fx1_check(fx1_csi2_buffer_release(&g_cam.dev, (unsigned)side) == FX1_CSI2_OK, 0x0C0, "camera: buffer released");
}

void cam_expect_frame(uint32_t f, uint32_t code)
{
    const uint32_t ev = cam_wait(CAM_BIT(CAM_IRQ_BUF_DONE_OK) | CAM_BIT(CAM_IRQ_BUF_DONE_ERR), CAM_WAIT_US);
    if (!(ev & CAM_BIT(CAM_IRQ_BUF_DONE_OK)) || (ev & CAM_BIT(CAM_IRQ_BUF_DONE_ERR))) {
        fx1_log_hex("  events", ev);
        fx1_check(0, code, "camera: BUF_DONE_OK expected");
    }
    const uint64_t t0 = fx1_mtime();
    g_cam_event_mtime = t0;
    const int side = cam_side_in(FX1_CSI2_BUF_SW_OWNED_VALID);
    fx1_check(side >= 0, code | 1u, "camera: a side handed to software, valid");
    const uint32_t n = cam_frame_number(side);
    if (n != f) fx1_log_hex("  frame number", n);
    fx1_check(n == f, code | 2u, "camera: frame number");
    fx1_check(cam_check_frame(side, f), code | 3u, "camera: every pixel equals the sensor pattern");
    cam_release(side);
    g_cam_handle_us = (uint32_t)((fx1_mtime() - t0) / (FX1_MTIME_HZ / 1000000u));
}

uint32_t cam_stream_off_and_drain(uint32_t code)
{
    fx1_check(fx1_ccs_stream(&g_cam.sensor, 0) == FX1_CCS_OK, code, "camera: stream off");
    uint32_t handled = 0;
    /* The frame in progress, if any, still completes: up to one more frame. */
    for (;;) {
        const uint32_t ev = cam_wait(CAM_BIT(CAM_IRQ_BUF_DONE_OK) | CAM_BIT(CAM_IRQ_BUF_DONE_ERR), CAM_WAIT_US);
        if (!(ev & (CAM_BIT(CAM_IRQ_BUF_DONE_OK) | CAM_BIT(CAM_IRQ_BUF_DONE_ERR)))) break;
        fx1_check(!(ev & CAM_BIT(CAM_IRQ_BUF_DONE_ERR)), code | 1u, "camera: last frame valid");
        const int side = cam_side_in(FX1_CSI2_BUF_SW_OWNED_VALID);
        fx1_check(side >= 0 && cam_check_frame(side, cam_frame_number(side)), code | 2u, "camera: last frame exact");
        cam_release(side);
        fx1_check(++handled <= 1u, code | 3u, "camera: at most one frame after stream off");
    }
    return handled;
}
