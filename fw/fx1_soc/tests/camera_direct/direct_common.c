/* Direct CSI-2 -> ISP firmware tests: shared code (direct_common.h). */
#include "direct_common.h"

fx1_isp_ingress g_ing;

static uint32_t ing_rd(void* ctx, uint32_t off)
{
    (void)ctx;
    return fx1_read32(FX1_ISP_INGRESS_BASE + off);
}
static void ing_wr(void* ctx, uint32_t off, uint32_t v)
{
    (void)ctx;
    fx1_write32(FX1_ISP_INGRESS_BASE + off, v);
}
static void ing_delay(void* ctx, uint32_t cycles)
{
    (void)ctx;
    (void)cycles;
}

uint32_t direct_isp_rd(uint32_t off) { return fx1_read32(FX1_ISP_CSR_BASE + off); }
void direct_isp_wr(uint32_t off, uint32_t v) { fx1_write32(FX1_ISP_CSR_BASE + off, v); }
uint64_t direct_ticks(uint32_t us) { return (uint64_t)us * (FX1_MTIME_HZ / 1000000u); }
void direct_spin_us(uint32_t us)
{
    const uint64_t end = fx1_mtime() + direct_ticks(us);
    while (fx1_mtime() < end) {
    }
}

int direct_wait_ctx(uint32_t us)
{
    const uint64_t end = fx1_mtime() + direct_ticks(us);
    while (fx1_mtime() < end)
        if (fx1_isp_ingress_ctx_ready(&g_ing)) return 1;
    return 0;
}

int direct_find_done(uint32_t tag)
{
    const uint32_t done = direct_isp_rd(FX1_ISP_ODMA_BUF_DONE_OFFSET);
    for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i)
        if ((done & (1u << i)) && direct_isp_rd(FX1_ISP_ODMA_BUF_TAG0_OFFSET + 4u * i) == tag) return (int)i;
    return -1;
}

void direct_arm(unsigned buf)
{
    direct_isp_wr(FX1_ISP_ODMA_BUF_DONE_OFFSET, 1u << buf); /* W1C: software has consumed it */
    direct_isp_wr(FX1_ISP_ODMA_BUF_FREE_OFFSET, 1u << buf); /* W1S: the ODMA may take it */
}

#if CAM_TPG
void direct_tpg_frame(void)
{
    fx1_write32(FX1_MIPI_CSR_BASE + FX1_CSI2_TPG_RESOLUTION, (H << 16) | W);
    fx1_write32(FX1_MIPI_CSR_BASE + FX1_CSI2_TPG_CONTROL, FX1_CSI2_TPG_CONTROL_TPG_ENABLE_MASK);
    fx1_write32(FX1_MIPI_CSR_BASE + FX1_CSI2_TPG_CONTROL, 0); /* the frame in progress completes */
}
#endif

uint32_t direct_check_frame(const fx1_isp_ingress_ctx* c, unsigned buf, uint32_t code)
{
    fx1_check(c->outcome == FX1_ISP_INGRESS_OUTCOME_OK, code, "direct: outcome OK");
    fx1_check(c->lines == H && c->samples == W * H, code | 1u, "direct: H lines of W samples");
    fx1_check(direct_isp_rd(FX1_ISP_ODMA_BUF_TAG0_OFFSET + 4u * buf) == c->tag, code | 3u,
              "direct: BUF_TAG = context TAG");
    fx1_check(direct_isp_rd(FX1_ISP_ODMA_BUF_TS0_OFFSET + 4u * buf) == c->frame_id, code | 4u,
              "direct: BUF_TS = FRAME_ID");
    fx1_check(c->isp_frame != 0 && direct_isp_rd(FX1_ISP_TUNE_CH0_BUF0_FRAME_OFFSET + 4u * buf) == c->isp_frame,
              code | 5u, "direct: ISP frame ID = BUFn_FRAME");
    fx1_camera_frame f;
    f.y = Y_BUF(buf);
    f.uv = UV_BUF(buf);
    f.width = W;
    f.height = H;
    fx1_dma_complete((const volatile void*)(uintptr_t)f.y, W * H, FX1_DMA_FROM_DEVICE);
    fx1_dma_complete((const volatile void*)(uintptr_t)f.uv, W * H / 2u, FX1_DMA_FROM_DEVICE);
    const uint32_t crc = cam_isp_crc(&f, W, W);
#if CAM_TPG
    const uint32_t n = crc == cam_isp_crc_tpg640[0] ? 1u : 0u;
#else
    const uint32_t n = cam_isp_frame_of(CAM_BITS, crc);
#endif
    if (!n) fx1_log_hex("  NV12 CRC", crc);
    fx1_check(n != 0, code | 6u, "direct: NV12 equals the ISP model's");
    return n;
}

void direct_setup(unsigned armed)
{
    fx1_check(fx1_camera_probe(&g_cam, fx1_csi2_hal_mmio(FX1_MIPI_CSR_BASE), fx1_isp_hal_mmio(), CAM_TPG) ==
                  FX1_CAMERA_OK,
              0x10, "direct: CSI-2, ISP, sensor probed");
    const fx1_isp_hal ing_hal = {ing_rd, ing_wr, ing_delay, 0};
    fx1_check(fx1_isp_ingress_init(&g_ing, &ing_hal) == FX1_ISP_OK, 0x11, "direct: ISP_INGRESS present");
    fx1_check(fx1_isp_ingress_bound(&g_ing) == 1, 0x12, "direct: CSI stream bound");

    /* CSI-2: sensor mode and D-PHY (sensor), RAW16 stream enabled, RAW DMA off. */
#if !CAM_TPG
    const fx1_camera_cfg cfg = cam_isp_cfg(CAM_BITS);
    const fx1_ccs_mode m = {(uint16_t)W, (uint16_t)H, cfg.line_length, CAM_FRAME_LENGTH, (uint8_t)CAM_BITS,
                            (uint8_t)cfg.lanes};
    fx1_check(fx1_ccs_set_mode(&g_cam.sensor, &m) == FX1_CCS_OK, 0x13, "direct: sensor mode");
    fx1_check(fx1_csi2_phy_start(&g_cam.csi, cfg.lanes, 2000) == FX1_CSI2_OK, 0x14, "direct: D-PHY started");
#endif
    fx1_check(fx1_csi2_stream_config(&g_cam.csi, 0, 0) == FX1_CSI2_OK, 0x15, "direct: stream off");
    fx1_check(fx1_csi2_stream_config(&g_cam.csi, 1, 1) == FX1_CSI2_OK, 0x16, "direct: stream RAW16 on");
    fx1_write32(FX1_MIPI_CSR_BASE + FX1_CSI2_EXPECTED_RESOLUTION, (H << 16) | W);

    /* ISP: geometry, native depth before the profile (COMMON_INPUT_FORMAT is
     * refused in profile mode), profile, ODMA only. */
    fx1_check(fx1_isp_set_geometry(&g_cam.isp, W, H, 0 /* RGGB */) == FX1_ISP_OK, 0x17, "direct: geometry");
    fx1_check(fx1_isp_set_input_format(&g_cam.isp, CAM_BITS == 10 ? 1u : 0u) == FX1_ISP_OK, 0x18, "direct: depth");
#if CAM_TPG
    fx1_check(fx1_isp_apply(&g_cam.isp, cam_isp_profile_tpg640,
                            sizeof cam_isp_profile_tpg640 / sizeof cam_isp_profile_tpg640[0]) == FX1_ISP_OK,
              0x19, "direct: profile");
#else
    fx1_check(fx1_isp_apply(&g_cam.isp, cfg.profile, cfg.profile_len) == FX1_ISP_OK, 0x19, "direct: profile");
#endif
    direct_isp_wr(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
    for (unsigned i = 0; i < 64u; ++i) (void)direct_isp_rd(FX1_ISP_COMMON_STATUS_OFFSET); /* Resizer derivation */
    fx1_check(direct_isp_rd(FX1_ISP_RESIZER_OUT_W_OFFSET) == W && direct_isp_rd(FX1_ISP_RESIZER_OUT_H_OFFSET) == H,
              0x1A, "direct: output geometry");
    direct_isp_wr(FX1_ISP_ODMA_Y_STRIDE_OFFSET, W);
    direct_isp_wr(FX1_ISP_ODMA_UV_STRIDE_OFFSET, W);
    for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i) {
        direct_isp_wr(FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u * i, Y_BUF(i));
        direct_isp_wr(FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u * i + 4u, 0);
        direct_isp_wr(FX1_ISP_ODMA_UV_ADDR_L0_OFFSET + 16u * i, UV_BUF(i));
        direct_isp_wr(FX1_ISP_ODMA_UV_ADDR_L0_OFFSET + 16u * i + 4u, 0);
        fx1_dma_prepare((const volatile void*)(uintptr_t)Y_BUF(i), W * H, FX1_DMA_FROM_DEVICE);
        fx1_dma_prepare((const volatile void*)(uintptr_t)UV_BUF(i), W * H / 2u, FX1_DMA_FROM_DEVICE);
    }
    direct_isp_wr(FX1_ISP_DMA_CTRL_OFFSET,
                  (15u << FX1_ISP_DMA_CTRL_MAX_BURST_M1_SHIFT) | FX1_ISP_DMA_CTRL_ODMA_EN_MASK);
    fx1_check(fx1_isp_ingress_set_source(&g_ing, FX1_ISP_INGRESS_SOURCE_DIRECT) == FX1_ISP_OK, 0x1B,
              "direct: SOURCE = direct");
    for (unsigned i = 0; i < armed; ++i) direct_arm(i);
}

void direct_start(void)
{
#if CAM_TPG
    direct_tpg_frame();
#else
    fx1_check(fx1_ccs_stream(&g_cam.sensor, 1) == FX1_CCS_OK, 0x1C, "direct: sensor streaming");
#endif
}

void direct_drain(fx1_isp_ingress_counters* k, uint32_t us)
{
    const uint64_t end = fx1_mtime() + direct_ticks(us);
    while (fx1_mtime() < end) {
        fx1_isp_ingress_ctx c;
        while (fx1_isp_ingress_ctx_pop(&g_ing, &c) == FX1_ISP_OK) {
        }
        if ((fx1_isp_ingress_status(&g_ing) & FX1_ISP_INGRESS_STATUS_IDLE_MASK) &&
            !(direct_isp_rd(FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK))
            break;
    }
    fx1_isp_ingress_read_counters(&g_ing, k);
    fx1_log_hex("  frames admitted", k->frames_admitted);
    fx1_log_hex("  dropped no credit", k->drop_no_credit);
    fx1_log_hex("  dropped no stream", k->drop_no_stream);
    fx1_log_hex("  frames invalid", k->frames_invalid);
    fx1_log_hex("  stall cycles", k->stall_cycles);
}

void direct_log_failed(const fx1_isp_ingress_ctx* c)
{
    fx1_log_hex("  not OK: outcome | cause << 8", c->outcome | (c->cause << 8));
    fx1_log_hex("  seq", (uint32_t)c->seq);
    fx1_log_hex("  lines", c->lines);
}
