/*
 * CSI-2 M6a: large frames through the camera manager, from the CSI-2 RX test
 * pattern generator (no sensor, no D-PHY): 3840x2160 RAW12 colour bars
 * (spec_decisions C35/D11), RAW16 sides of 16 MiB, zero-copy into the ISP,
 * NV12 3840x2160. Two frames, one per CSI-2 side. The firmware checks the
 * geometry and metadata and spot-checks the RAW (every bar, every Bayer
 * phase, first and last line); it records each frame's output slot at
 * CAM_TPG_REC, and check_tpg4k.py compares the dumped NV12 against the golden
 * of the stand-alone ISP model (cam_isp_crc_tpg4k). A CRC of 12 MB on the
 * simulated CPU would cost more host time than the whole frame.
 */
#include "cam_isp.h"
#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_csi2_hal_mmio.h"
#include "fx1_isp_hal_mmio.h"

#define W CAM_ISP_TPG_W
#define H CAM_ISP_TPG_H
#define FRAMES 2u
#define SIDE_SIZE 0x1000000u
#define TPG_SIDE(i) (FX1_FW_RAW_BUF_BASE + (i) * SIDE_SIZE)
#define TPG_Y(i) (FX1_FW_YUV_BUF_BASE + (i) * 0x1400000u)
#define TPG_UV(i) (TPG_Y(i) + 0xC00000u)
#define CAM_TPG_REC (FX1_FW_SHARED_BASE + 0x800000u) /* magic, frames, then per frame: output, y, uv, capture */
#define TPG_WAIT_US 400000u

/* C35: band = min(7, x / (W' >> 3)), W' = W & ~3; RGGB; RAW12 0xFFF / 0. */
static uint32_t bar(uint32_t x, uint32_t y)
{
    static const uint8_t R[8] = {1, 1, 0, 0, 1, 1, 0, 0}, G[8] = {1, 1, 1, 1, 0, 0, 0, 0}, B[8] = {1, 0, 1, 0, 1, 0, 1, 0};
    uint32_t b = x / ((W & ~3u) >> 3);
    if (b > 7u) b = 7u;
    const uint8_t on = (!(y & 1u) && !(x & 1u)) ? R[b] : ((y & 1u) && (x & 1u)) ? B[b] : G[b];
    return on ? 0x0FFFu : 0u;
}

static void spot_check_raw(uint32_t side, uint32_t code)
{
    static const uint32_t rows[4] = {0u, 1u, H - 2u, H - 1u};
    uint32_t bad = 0;
    for (uint32_t r = 0; r < 4u; ++r)
        for (uint32_t b = 0; b < 8u; ++b)
            for (uint32_t dx = 0; dx < 2u; ++dx) {
                const uint32_t x = b * ((W & ~3u) >> 3) + 240u + dx, y = rows[r];
                const uint16_t v = *(const volatile uint16_t*)(uintptr_t)(side + 2u * (y * W + x));
                bad += v != bar(x, y);
            }
    if (bad) fx1_log_hex("  RAW samples off", bad);
    fx1_check(bad == 0, code, "tpg4k: RAW colour bars (every bar and Bayer phase)");
}

int main(void)
{
    fx1_global_irq_disable();
    fx1_check(fx1_camera_probe(&g_cam, fx1_csi2_hal_mmio(FX1_MIPI_CSR_BASE), fx1_isp_hal_mmio(), 1) == FX1_CAMERA_OK,
              0x10, "tpg4k: probe (CSI-2 RX, ISP, no sensor)");
    fx1_camera_cfg c;
    for (unsigned i = 0; i < sizeof c; ++i) ((uint8_t*)&c)[i] = 0;
    c.width = W;
    c.height = H;
    c.bits = 12;
    c.tpg = 1;
    c.csi_side[0] = TPG_SIDE(0);
    c.csi_side[1] = TPG_SIDE(1);
    c.csi_side_size = SIDE_SIZE;
    for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i) {
        c.y[i] = TPG_Y(i);
        c.uv[i] = TPG_UV(i);
    }
    c.y_stride = c.uv_stride = W;
    c.profile = cam_isp_profile_tpg4k;
    c.profile_len = sizeof cam_isp_profile_tpg4k / sizeof cam_isp_profile_tpg4k[0];
    fx1_check(fx1_camera_configure(&g_cam, &c) == FX1_CAMERA_OK, 0x11, "tpg4k: configured 3840x2160 RAW12");
    fx1_write32(CAM_TPG_REC, 0);
    fx1_write32(CAM_TPG_REC + 4u, 0);
    cam_isp_irqs();
    fx1_check(fx1_camera_start(&g_cam) == FX1_CAMERA_OK, 0x12, "tpg4k: started");
    for (uint32_t k = 0; k < FRAMES; ++k) {
        const uint64_t t0 = fx1_mtime();
        fx1_check(fx1_camera_tpg_frame(&g_cam) == FX1_CAMERA_OK, 0x100 + (k << 4), "tpg4k: TPG frame");
        fx1_camera_frame f;
        const int rc = fx1_camera_dequeue(&g_cam, &f, TPG_WAIT_US);
        if (rc) fx1_log_hex("  dequeue rc", (uint32_t)rc);
        fx1_check(rc == FX1_CAMERA_OK, 0x101 + (k << 4), "tpg4k: frame through CSI-2 and the ISP");
        fx1_log_hex("  frame us", (uint32_t)((fx1_mtime() - t0) / (FX1_MTIME_HZ / 1000000u)));
        fx1_check(f.width == W && f.height == H && f.capture == k + 1u && f.csi_side == k && f.isp_seq == k &&
                      f.isp_output == k && f.y == TPG_Y(k) && f.uv == TPG_UV(k),
                  0x102 + (k << 4), "tpg4k: geometry and metadata");
        fx1_check(f.zero_copy && f.isp_in == TPG_SIDE(k), 0x103 + (k << 4), "tpg4k: zero-copy from the CSI-2 side");
        spot_check_raw(f.isp_in, 0x104 + (k << 4));
        fx1_write32(CAM_TPG_REC + 8u + 16u * k, f.isp_output);
        fx1_write32(CAM_TPG_REC + 12u + 16u * k, f.y);
        fx1_write32(CAM_TPG_REC + 16u + 16u * k, f.uv);
        fx1_write32(CAM_TPG_REC + 20u + 16u * k, f.capture);
        fx1_check(fx1_camera_release(&g_cam, &f) == FX1_CAMERA_OK, 0x105 + (k << 4), "tpg4k: released");
    }
    const fx1_camera_stats* s = &g_cam.stats;
    fx1_check(s->csi_ok == FRAMES && s->csi_err == 0 && s->isp_done == FRAMES && s->isp_errors == 0 && s->isp_lost == 0 &&
                  fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_FRAME_DROP_COUNT) == 0,
              0x200, "tpg4k: no error, no drop");
    fx1_check(fx1_camera_stop(&g_cam, TPG_WAIT_US) == FX1_CAMERA_OK, 0x201, "tpg4k: stop");
    fx1_write32(CAM_TPG_REC, 0x4B475054u); /* 'TPGK' */
    fx1_write32(CAM_TPG_REC + 4u, FRAMES);
    fx1_log("tpg4k: two 3840x2160 frames through the camera manager, NV12 left for the host");
    return 0;
}
