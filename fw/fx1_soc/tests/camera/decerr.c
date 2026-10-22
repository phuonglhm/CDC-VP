/*
 * CSI-2 M5b: AXI DECERR on a camera frame (fx1_soc --camera), firmware only:
 * side 1 is programmed at an address no bus target decodes. Frame 1 goes to
 * side 0 and is exact; frame 2 to side 1 fails at its first write
 * (BUF_DONE_ERR, AXI_WRITE_ERROR, DECERR at the side's base, no valid line).
 * Recovery: DMA stopped, programmed with a valid side 1, started; capture
 * resumes with exact frames.
 */
#include "camera.h"
#include "fx1_csi2/fx1_csi2_regs.h"

#define UNMAPPED 0x60000000u /* no target (bus_fault test) */

int main(void)
{
    cam_bringup(10, 4, 0, CAM_SIDE0, UNMAPPED);
    fx1_check(fx1_ccs_stream(&g_cam.sensor, 1) == FX1_CCS_OK, 0x100, "decerr: stream on");
    uint32_t ev = cam_wait(CAM_BIT(CAM_IRQ_BUF_DONE_OK) | CAM_BIT(CAM_IRQ_BUF_DONE_ERR), CAM_WAIT_US);
    fx1_check((ev & CAM_BIT(CAM_IRQ_BUF_DONE_OK)) && !(ev & CAM_BIT(CAM_IRQ_BUF_DONE_ERR)), 0x110, "decerr: frame 1 valid");
    fx1_csi2_buf_info b0;
    fx1_csi2_get_buffer(&g_cam.dev, 0, &b0);
    fx1_check(b0.state == FX1_CSI2_BUF_SW_OWNED_VALID && cam_frame_number(0) == 1u && cam_check_frame(0, 1), 0x111,
              "decerr: frame 1 exact in side 0");
    ev = cam_wait(CAM_BIT(CAM_IRQ_BUF_DONE_OK) | CAM_BIT(CAM_IRQ_BUF_DONE_ERR), CAM_WAIT_US);
    fx1_log_hex("decerr: frame 2 events", ev);
    fx1_check((ev & CAM_BIT(CAM_IRQ_BUF_DONE_ERR)) && (ev & CAM_BIT(CAM_IRQ_AXI_WRITE_ERROR)), 0x120,
              "decerr: frame 2 BUF_DONE_ERR and AXI_WRITE_ERROR");
    fx1_csi2_buf_info b1;
    fx1_csi2_get_buffer(&g_cam.dev, 1, &b1);
    fx1_check(b1.state == FX1_CSI2_BUF_SW_OWNED_ERROR && b1.last_valid_line == 0xFFFFu, 0x121,
              "decerr: side 1 with error, no line complete");
    fx1_csi2_dma_fault f;
    fx1_check(fx1_csi2_get_dma_fault(&g_cam.dev, &f) == FX1_CSI2_OK, 0x122, "decerr: fault info");
    fx1_log_hex("decerr: fault address", f.addr);
    fx1_check(f.resp == 3u && f.decerr_count == 1u && f.slverr_count == 0u && f.addr == UNMAPPED, 0x123,
              "decerr: DECERR at the side's base");
    /* Recovery: stop, keep side 0's frame 1 checked, give both back, fix side 1. */
    fx1_check(fx1_csi2_dma_stop(&g_cam.dev, CAM_WAIT_US) == FX1_CSI2_OK, 0x130, "decerr: DMA stopped");
    (void)cam_wait(0, 0);
    for (int side = 0; side < 2; ++side) {
        fx1_csi2_buf_info bi;
        fx1_csi2_get_buffer(&g_cam.dev, (unsigned)side, &bi);
        if (bi.state != FX1_CSI2_BUF_HW_OWNED) {
            fx1_check(fx1_csi2_buffer_release(&g_cam.dev, (unsigned)side) == FX1_CSI2_OK, 0x131, "decerr: released");
        }
    }
    cam_dma_restart(CAM_SIDE0, CAM_SIDE1);
    ev = cam_wait(CAM_BIT(CAM_IRQ_BUF_DONE_OK) | CAM_BIT(CAM_IRQ_BUF_DONE_ERR), CAM_WAIT_US);
    fx1_check((ev & CAM_BIT(CAM_IRQ_BUF_DONE_OK)) && !(ev & CAM_BIT(CAM_IRQ_BUF_DONE_ERR)), 0x140, "decerr: recovered");
    const int s = cam_side_in(FX1_CSI2_BUF_SW_OWNED_VALID);
    const uint32_t n = s >= 0 ? cam_frame_number(s) : 0;
    fx1_log_hex("decerr: first frame after the restart", n);
    fx1_check(s >= 0 && n >= 3u && cam_check_frame(s, n), 0x141, "decerr: exact frame after the restart");
    cam_release(s);
    cam_expect_frame(n + 1u, 0x150);
    (void)cam_stream_off_and_drain(0x160);
    fx1_check(fx1_csi2_dma_stop(&g_cam.dev, CAM_WAIT_US) == FX1_CSI2_OK, 0x170, "decerr: DMA stopped");
    fx1_log("decerr: DECERR contained to its frame; capture recovered after reprogramming");
    return 0;
}
