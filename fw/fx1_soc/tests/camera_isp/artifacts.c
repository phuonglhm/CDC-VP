/*
 * CSI-2 M6a: camera -> ISP artifacts for the host (fx1_soc --camera, built for
 * RAW12 and RAW10). CAM_ART_FRAMES consecutive frames; for each, the firmware
 * keeps in DDR (CAM_ART_BASE, read back with fx1_soc --dump):
 *   - the CSI-2 RAW16 side as the manager saw it before the ISP (capture hook),
 *   - the ISP input it was given (RAW10: the staging buffer; RAW12 zero-copy:
 *     none, the side itself was the input),
 *   - the NV12 output, and a record linking them (capture, CSI-2 FRAME_ID and
 *     side, ISP sequence, input / output slots and addresses, the frame number
 *     the firmware identified from the golden CRC).
 * check_artifacts.py checks on the host: RAW = sensor pattern, staging =
 * RAW << 2, NV12 = golden CRC and = fx1_isp_run_raw on the dumped input (SHA-256),
 * and the record links.
 *
 * Layout (all little-endian words), base CAM_ART_BASE:
 *   0x0000 header: magic 'CAMA', version, bits, width, height, frames,
 *          csi_side[0], csi_side[1], record offset, record size, frame offset,
 *          frame stride, raw offset, input offset, y offset, uv offset
 *   0x0040 + k * 0x40  record k (cam_art_rec)
 *   0x1000 + k * 0x2000  frame k: RAW16 +0x0, ISP input +0x800, Y +0x1000, UV +0x1400
 */
#include "cam_isp.h"
#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_csi2_hal_mmio.h"
#include "fx1_isp_hal_mmio.h"

#ifndef CAM_BITS
#error CAM_BITS required
#endif
#define CAM_ART_BASE (FX1_FW_RAW_BUF_BASE + 0x400000u)
#define CAM_ART_RING (CAM_ART_BASE + 0x100000u) /* RAW copies by capture number, 8 deep */
#define CAM_ART_FRAMES 6u
#define REC_OFF 0x40u
#define REC_SIZE 0x40u
#define FRAME_OFF 0x1000u
#define RAW_BYTES (CAM_ISP_W * CAM_ISP_H * 2u)
/* Per frame: RAW, ISP input (RAW10 staging), Y, UV, derived from the sensor
 * size (the fixed 64x16 offsets overlapped at 64x32, V107-11 platform CI). */
#define RAW_OFF 0x0u
#define IN_OFF (RAW_OFF + RAW_BYTES)
#define Y_OFF (IN_OFF + RAW_BYTES)
#define UV_OFF (Y_OFF + CAM_ISP_W * CAM_ISP_H)
#define FRAME_STRIDE ((UV_OFF + CAM_ISP_W * CAM_ISP_H / 2u + 0xFFFu) & ~0xFFFu)
#define CAM_ART_LEN 0x20000u /* dumped by check_artifacts.py (ART_LEN) */
_Static_assert(FRAME_OFF + CAM_ART_FRAMES * FRAME_STRIDE <= CAM_ART_LEN, "artifact frames within the dumped area");
_Static_assert(REC_OFF + CAM_ART_FRAMES * REC_SIZE <= FRAME_OFF, "records below the frames");
_Static_assert(CAM_ART_LEN <= CAM_ART_RING - CAM_ART_BASE, "dumped area below the RAW ring");

typedef struct {
    uint32_t capture, csi_frame_id, csi_side, isp_seq;
    uint32_t isp_input, isp_output, zero_copy, isp_in;
    uint32_t y, uv, number, nv12_crc;
    uint32_t raw_capture, width, height, reserved;
} cam_art_rec;

static uint32_t ring_capture[8];

static void copy_words(uint32_t dst, uint32_t src, uint32_t bytes)
{
    volatile uint32_t* d = (volatile uint32_t*)(uintptr_t)dst;
    const volatile uint32_t* s = (const volatile uint32_t*)(uintptr_t)src;
    for (uint32_t i = 0; i < bytes / 4u; ++i) d[i] = s[i];
}

static void keep_raw(void* ctx, uint32_t raw16, uint32_t capture, uint16_t csi_frame_id)
{
    (void)ctx;
    (void)csi_frame_id;
    copy_words(CAM_ART_RING + (capture & 7u) * RAW_BYTES, raw16, RAW_BYTES);
    ring_capture[capture & 7u] = capture;
}

static void put(uint32_t off, uint32_t v) { fx1_write32(CAM_ART_BASE + off, v); }

int main(void)
{
    fx1_global_irq_disable();
    fx1_check(fx1_camera_probe(&g_cam, fx1_csi2_hal_mmio(FX1_MIPI_CSR_BASE), fx1_isp_hal_mmio(), 0) == FX1_CAMERA_OK,
              0x10, "artifacts: probe");
    fx1_camera_cfg cfg = cam_isp_cfg(CAM_BITS);
    cfg.capture_hook = keep_raw;
    /* Twice the frame period: at 64x32 the per-frame copies (RAW ring in the
     * hook, RAW / staged input / Y / UV here, RAW10 staging in the manager)
     * outlast one 3 ms frame and the sensor drops one (V107-11 platform CI). */
    cfg.frame_length = 2u * CAM_ISP_FRAME_LENGTH;
    fx1_check(fx1_camera_configure(&g_cam, &cfg) == FX1_CAMERA_OK, 0x11, "artifacts: configured");
    const uint32_t hdr[16] = {0x414D4143u, 1u, CAM_BITS, CAM_ISP_W, CAM_ISP_H, CAM_ART_FRAMES,
                              cfg.csi_side[0], cfg.csi_side[1], REC_OFF, REC_SIZE, FRAME_OFF, FRAME_STRIDE,
                              RAW_OFF, IN_OFF, Y_OFF, UV_OFF};
    for (uint32_t i = 0; i < 16u; ++i) put(4u * i, hdr[i]);
    put(4u * 5u, 0); /* frames: written at the end, so a partial run has none */
    cam_isp_irqs();
    fx1_check(fx1_camera_start(&g_cam) == FX1_CAMERA_OK, 0x12, "artifacts: started");
    for (uint32_t k = 0; k < CAM_ART_FRAMES; ++k) {
        fx1_camera_frame f;
        fx1_check(fx1_camera_dequeue(&g_cam, &f, CAM_ISP_WAIT_US) == FX1_CAMERA_OK, 0x100 + (k << 4), "artifacts: frame");
        const uint32_t fb = FRAME_OFF + k * FRAME_STRIDE;
        fx1_check(ring_capture[f.capture & 7u] == f.capture, 0x101 + (k << 4), "artifacts: RAW kept for this capture");
        copy_words(CAM_ART_BASE + fb + RAW_OFF, CAM_ART_RING + (f.capture & 7u) * RAW_BYTES, RAW_BYTES);
        if (!f.zero_copy) copy_words(CAM_ART_BASE + fb + IN_OFF, f.isp_in, RAW_BYTES);
        copy_words(CAM_ART_BASE + fb + Y_OFF, f.y, CAM_ISP_W * CAM_ISP_H);
        copy_words(CAM_ART_BASE + fb + UV_OFF, f.uv, CAM_ISP_W * CAM_ISP_H / 2u);
        const uint32_t crc = cam_isp_crc(&f, CAM_ISP_W, CAM_ISP_W);
        const cam_art_rec r = {f.capture, f.csi_frame_id, f.csi_side, f.isp_seq, f.isp_input, f.isp_output,
                               (uint32_t)f.zero_copy, f.isp_in, f.y, f.uv, cam_isp_frame_of(CAM_BITS, crc), crc,
                               ring_capture[f.capture & 7u], f.width, f.height, 0};
        const uint32_t* w = (const uint32_t*)&r;
        for (uint32_t i = 0; i < REC_SIZE / 4u; ++i) put(REC_OFF + k * REC_SIZE + 4u * i, w[i]);
        fx1_log_hex("  frame", r.number);
        fx1_check(r.number == k + 1u, 0x102 + (k << 4), "artifacts: frames 1.. in order, NV12 = golden");
        fx1_check(fx1_camera_release(&g_cam, &f) == FX1_CAMERA_OK, 0x103 + (k << 4), "artifacts: released");
    }
    fx1_check(fx1_camera_stop(&g_cam, CAM_ISP_WAIT_US * 2u) == FX1_CAMERA_OK, 0x200, "artifacts: stop");
    fx1_mb();
    put(4u * 5u, CAM_ART_FRAMES);
    fx1_log("artifacts: RAW, ISP input, NV12 and records in DDR for the host");
    return 0;
}
