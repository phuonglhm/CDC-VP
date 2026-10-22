/*
 * CSI-2 M6a: sensor -> CSI-2 RX -> DDR -> ISP -> NV12 on fx1_soc --camera,
 * through the camera manager. Built twice:
 *   camera_isp_raw12  RAW12 4 lanes, zero-copy (the ISP reads the CSI-2 side)
 *   camera_isp_raw10  RAW10 2 lanes, staged sample << 2 for the ISP
 * Eight consecutive frames: each NV12 equals the stand-alone ISP model's for
 * that sensor frame number (1..8, none skipped), from the ISP driver's output
 * geometry; CSI-2 and ISP interrupts through the PLIC; then stop: every CSI-2
 * side back to the hardware, ISP stopped, counters consistent.
 */
#include "cam_isp.h"
#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_csi2_hal_mmio.h"
#include "fx1_isp_hal_mmio.h"

#ifndef CAM_BITS
#error CAM_BITS required
#endif
#define FRAMES 8u

int main(void)
{
    fx1_global_irq_disable();
    fx1_check(fx1_camera_probe(&g_cam, fx1_csi2_hal_mmio(FX1_MIPI_CSR_BASE), fx1_isp_hal_mmio(), 0) == FX1_CAMERA_OK,
              0x10, "camera isp: probe (CSI-2 RX, ISP, sensor)");
    const fx1_camera_cfg cfg = cam_isp_cfg(CAM_BITS);
    fx1_check(fx1_camera_configure(&g_cam, &cfg) == FX1_CAMERA_OK, 0x11, "camera isp: configured");
    cam_isp_irqs();
    fx1_check(fx1_camera_start(&g_cam) == FX1_CAMERA_OK, 0x12, "camera isp: started");
    fx1_log_hex("camera isp: bits", CAM_BITS);
    for (uint32_t k = 1; k <= FRAMES; ++k) {
        const uint32_t n = cam_isp_take(CAM_BITS, 0x100 + (k << 4));
        if (n != k) fx1_log_hex("  frame number", n);
        fx1_check(n == k, 0x200 + k, "camera isp: frames in order, none lost");
    }
    fx1_log("camera isp: 8 frames, NV12 = ISP model golden");
    /* No drop while the consumer kept up (after this, stop leaves frames unconsumed). */
    const uint32_t drops = fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_FRAME_DROP_COUNT);
    fx1_check(fx1_camera_stop(&g_cam, CAM_ISP_WAIT_US * 2u) == FX1_CAMERA_OK, 0x300, "camera isp: stopped");
    const fx1_camera_stats* s = &g_cam.stats;
    fx1_log_hex("  drops while streaming", drops);
    fx1_log_hex("  csi err", s->csi_err);
    fx1_log_hex("  isp errors", s->isp_errors);
    fx1_log_hex("  isp lost", s->isp_lost);
    fx1_log_hex("  csi ok", s->csi_ok);
    fx1_log_hex("  isp queued", s->isp_queued);
    fx1_log_hex("  isp done", s->isp_done);
    fx1_log_hex("  csi irqs", s->csi_irqs);
    fx1_log_hex("  isp irqs", s->isp_irqs);
    fx1_check(drops == 0 && s->csi_err == 0 && s->isp_errors == 0 && s->isp_lost == 0, 0x301,
              "camera isp: no drop, no error");
    fx1_check(s->isp_done >= FRAMES && s->isp_queued >= s->isp_done, 0x302, "camera isp: frames went through the ISP");
    fx1_check(g_cam.side_state[0] == 0 && g_cam.side_state[1] == 0, 0x304, "camera isp: both CSI-2 sides back");
    fx1_check(s->csi_irqs >= FRAMES && s->isp_irqs >= FRAMES, 0x303, "camera isp: interrupts through the PLIC");
    return 0;
}
