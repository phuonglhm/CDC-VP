#include "cam_isp.h"

fx1_camera g_cam;

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    (void)mtval;
    if (mcause != (FX1_MCAUSE_INTERRUPT | FX1_IRQ_M_EXT)) {
        fx1_log_hex("unexpected trap, mcause", mcause);
        fx1_log_hex("  mepc", mepc);
        fx1_fail(0xE00);
    }
    const uint32_t ctx = FX1_PLIC_CONTEXT_HART_M(0);
    const uint32_t id = fx1_plic_claim(ctx);
    if (id == FX1_IRQ_MIPI) fx1_camera_csi_isr(&g_cam);
    else if (id == FX1_IRQ_ISP) fx1_camera_isp_isr(&g_cam);
    if (id) fx1_plic_complete(ctx, id);
    return mepc;
}

void cam_isp_irqs(void)
{
    const uint32_t ctx = FX1_PLIC_CONTEXT_HART_M(0);
    fx1_set_trap_handler(handler);
    fx1_plic_set_priority(FX1_IRQ_MIPI, 3);
    fx1_plic_set_priority(FX1_IRQ_ISP, 2);
    fx1_plic_enable(ctx, FX1_IRQ_MIPI, 1);
    fx1_plic_enable(ctx, FX1_IRQ_ISP, 1);
    fx1_irq_enable(FX1_MIE_MEIE);
    fx1_global_irq_enable();
}

fx1_camera_cfg cam_isp_cfg(uint32_t bits)
{
    fx1_camera_cfg c;
    for (unsigned i = 0; i < sizeof c; ++i) ((uint8_t*)&c)[i] = 0;
    c.width = CAM_ISP_W;
    c.height = CAM_ISP_H;
    c.bits = bits;
    c.lanes = bits == 12 ? 4u : 2u;
    c.line_length = CAM_ISP_LINE_LENGTH;
    c.frame_length = CAM_ISP_FRAME_LENGTH;
    c.csi_side[0] = CAM_ISP_SIDE(0);
    c.csi_side[1] = CAM_ISP_SIDE(1);
    c.csi_side_size = 0x1000u;
    for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i) {
        c.staging[i] = CAM_ISP_STAGING(i);
        c.y[i] = CAM_ISP_Y(i);
        c.uv[i] = CAM_ISP_UV(i);
    }
    c.y_stride = c.uv_stride = CAM_ISP_W;
    c.profile = bits == 12 ? cam_isp_profile_raw12 : cam_isp_profile_raw10;
    c.profile_len = bits == 12 ? sizeof cam_isp_profile_raw12 / sizeof cam_isp_profile_raw12[0]
                               : sizeof cam_isp_profile_raw10 / sizeof cam_isp_profile_raw10[0];
    return c;
}

/* Table CRC-32: the check of a 64x16 NV12 must stay well inside a frame
 * period, or the consumer itself would make the CSI-2 RX drop frames. */
static uint32_t crc_table[256];

static uint32_t crc_update(uint32_t crc, uint32_t word, unsigned bytes)
{
    if (!crc_table[1])
        for (uint32_t i = 0; i < 256u; ++i) {
            uint32_t c = i;
            for (unsigned k = 0; k < 8; ++k) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
            crc_table[i] = c;
        }
    for (unsigned b = 0; b < bytes; ++b) crc = (crc >> 8) ^ crc_table[(crc ^ (word >> (8u * b))) & 0xFFu];
    return crc;
}

uint32_t cam_isp_crc(const fx1_camera_frame* f, uint32_t y_stride, uint32_t uv_stride)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t y = 0; y < f->height; ++y) {
        const volatile uint32_t* row = (const volatile uint32_t*)(uintptr_t)(f->y + y * y_stride);
        for (uint32_t i = 0; i < f->width / 4u; ++i) crc = crc_update(crc, row[i], 4);
    }
    for (uint32_t y = 0; y < f->height / 2u; ++y) {
        const volatile uint32_t* row = (const volatile uint32_t*)(uintptr_t)(f->uv + y * uv_stride);
        for (uint32_t i = 0; i < f->width / 4u; ++i) crc = crc_update(crc, row[i], 4);
    }
    return ~crc;
}

uint32_t cam_isp_frame_of(uint32_t bits, uint32_t crc)
{
    const uint32_t* t = bits == 12 ? cam_isp_crc_raw12 : cam_isp_crc_raw10;
    for (uint32_t f = 0; f < CAM_ISP_FRAMES; ++f)
        if (t[f] == crc) return f + 1u;
    return 0;
}

uint32_t cam_isp_take(uint32_t bits, uint32_t code)
{
    fx1_camera_frame f;
    const int rc = fx1_camera_dequeue(&g_cam, &f, CAM_ISP_WAIT_US);
    if (rc != FX1_CAMERA_OK) fx1_log_hex("  dequeue rc", (uint32_t)rc);
    fx1_check(rc == FX1_CAMERA_OK, code, "camera isp: frame dequeued");
    fx1_check(f.width == CAM_ISP_W && f.height == CAM_ISP_H, code | 1u, "camera isp: NV12 geometry from the ISP driver");
    const uint32_t crc = cam_isp_crc(&f, CAM_ISP_W, CAM_ISP_W);
    const uint32_t n = cam_isp_frame_of(bits, crc);
    if (!n) {
        fx1_log_hex("  capture", f.capture);
        fx1_log_hex("  NV12 CRC", crc);
    }
    fx1_check(n != 0, code | 2u, "camera isp: NV12 equals the ISP model's for a sensor frame");
    if (bits == 12)
        fx1_check(f.zero_copy && f.isp_in == CAM_ISP_SIDE(f.csi_side), code | 3u, "camera isp: RAW12 zero-copy");
    else
        fx1_check(!f.zero_copy && f.isp_in == CAM_ISP_STAGING(f.isp_input), code | 3u, "camera isp: RAW10 staging");
    fx1_check(fx1_camera_release(&g_cam, &f) == FX1_CAMERA_OK, code | 4u, "camera isp: output released");
    return n;
}
