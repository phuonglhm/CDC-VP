/*
 * CSI-2 M5b: firmware-driven camera recovery (fx1_soc --camera), RAW10 4
 * lanes, packed:
 *   A  no free buffer: frames 1 and 2 held, frame 3 dropped (FRAME_DROPPED,
 *      FRAME_DROP_COUNT 1), the release of frame 1's side lets frame 4 in
 *   B  sensor software_reset inside a frame (timed from the previous frame's
 *      end: the next frame starts CAM_FRAME_US - 160 us later and the I2C
 *      write takes ~95 us): that frame is still delivered whole and exact, the
 *      sensor stops; mode set again, stream on: frame numbers restart at 1
 *   C  DMA stopped while the sensor streams, then programmed and started
 *      again: capture resumes with exact frames
 */
#include "camera.h"
#include "fx1_csi2/fx1_csi2_regs.h"

static uint32_t take_valid(uint32_t code)
{
    const uint32_t ev = cam_wait(CAM_BIT(CAM_IRQ_BUF_DONE_OK) | CAM_BIT(CAM_IRQ_BUF_DONE_ERR), CAM_WAIT_US);
    fx1_check((ev & CAM_BIT(CAM_IRQ_BUF_DONE_OK)) && !(ev & CAM_BIT(CAM_IRQ_BUF_DONE_ERR)), code, "recover: BUF_DONE_OK");
    return ev;
}

int main(void)
{
    cam_bringup(10, 4, 0, CAM_SIDE0, CAM_SIDE1);
    fx1_check(fx1_ccs_stream(&g_cam.sensor, 1) == FX1_CCS_OK, 0x100, "recover: stream on");

    /* A */
    take_valid(0x110);
    const int s1 = cam_side_in(FX1_CSI2_BUF_SW_OWNED_VALID);
    fx1_check(s1 >= 0 && cam_frame_number(s1) == 1u, 0x111, "A: frame 1 held");
    take_valid(0x112);
    fx1_check(cam_side_in(FX1_CSI2_BUF_HW_OWNED) < 0, 0x113, "A: frame 2 held, no side free");
    const uint32_t ev = cam_wait(CAM_BIT(CAM_IRQ_FRAME_DROPPED), CAM_WAIT_US);
    fx1_check(ev & CAM_BIT(CAM_IRQ_FRAME_DROPPED), 0x114, "A: frame 3 dropped");
    fx1_check(fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_FRAME_DROP_COUNT) == 1u, 0x115, "A: FRAME_DROP_COUNT 1");
    fx1_check(cam_check_frame(s1, 1), 0x116, "A: frame 1 exact");
    cam_release(s1);
    const int s2 = 1 - s1;
    fx1_check(cam_check_frame(s2, 2), 0x117, "A: frame 2 exact");
    take_valid(0x120); /* frame 4, into the side just released */
    fx1_csi2_buf_info bi1;
    fx1_csi2_get_buffer(&g_cam.dev, (unsigned)s1, &bi1);
    fx1_check(bi1.state == FX1_CSI2_BUF_SW_OWNED_VALID && cam_frame_number(s1) == 4u && cam_check_frame(s1, 4), 0x121,
              "A: frame 4 exact in the released side");
    cam_release(s1);
    cam_release(s2);
    cam_expect_frame(5, 0x130);
    fx1_log("A: frame 3 dropped while both sides were held; capture resumed at frame 4");

    /* B: frame 6 starts about CAM_FRAME_US - 160 us after frame 5's end. */
    const uint64_t at = g_cam_event_mtime + (uint64_t)(CAM_FRAME_US - 140u) * (FX1_MTIME_HZ / 1000000u);
    while (fx1_mtime() < at) {
    }
    fx1_check(fx1_ccs_soft_reset(&g_cam.sensor) == FX1_CCS_OK, 0x140, "B: sensor software_reset");
    const uint32_t tail = cam_stream_off_and_drain(0x150); /* frame 6, which the reset hit */
    fx1_check(tail == 1u, 0x151, "B: the frame the reset hit was delivered whole");
    fx1_check(fx1_ccs_set_mode(&g_cam.sensor, &g_cam.mode) == FX1_CCS_OK, 0x160, "B: mode set again");
    fx1_check(fx1_ccs_stream(&g_cam.sensor, 1) == FX1_CCS_OK, 0x161, "B: stream on");
    cam_expect_frame(1, 0x170);
    cam_expect_frame(2, 0x180);
    fx1_log_hex("B: frames completed after the reset", tail);
    fx1_log("B: sensor reset mid-stream; frame numbers restart at 1");

    /* C */
    fx1_check(fx1_csi2_dma_stop(&g_cam.dev, CAM_WAIT_US) == FX1_CSI2_OK, 0x190, "C: DMA stopped while streaming");
    (void)cam_wait(0, 0); /* events of a capture finalised by the stop */
    for (int side = 0; side < 2; ++side) {
        fx1_csi2_buf_info bi;
        fx1_csi2_get_buffer(&g_cam.dev, (unsigned)side, &bi);
        if (bi.state == FX1_CSI2_BUF_SW_OWNED_VALID)
            fx1_check(cam_check_frame(side, cam_frame_number(side)), 0x191, "C: frame finished by the stop exact");
        if (bi.state != FX1_CSI2_BUF_HW_OWNED) cam_release(side);
    }
    cam_delay_us(2u * CAM_FRAME_US); /* two frame periods without the DMA */
    fx1_check(!(cam_wait(0, 0) & (CAM_BIT(CAM_IRQ_BUF_DONE_OK) | CAM_BIT(CAM_IRQ_BUF_DONE_ERR))), 0x192,
              "C: no capture while the DMA is stopped");
    const uint32_t before = fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_FRAME_COUNT);
    cam_dma_restart(CAM_SIDE0, CAM_SIDE1);
    take_valid(0x1A0);
    const int s = cam_side_in(FX1_CSI2_BUF_SW_OWNED_VALID);
    const uint32_t n = s >= 0 ? cam_frame_number(s) : 0;
    fx1_log_hex("C: first frame after the restart", n);
    fx1_check(s >= 0 && n >= 4u && cam_check_frame(s, n), 0x1A1, "C: exact frame after the DMA restart");
    cam_release(s);
    cam_expect_frame(n + 1u, 0x1B0);
    fx1_check(fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_FRAME_COUNT) == before + 2u, 0x1B2,
              "C: FRAME_COUNT counts the frames delivered after the restart");
    (void)cam_stream_off_and_drain(0x1C0);
    fx1_check(fx1_csi2_dma_stop(&g_cam.dev, CAM_WAIT_US) == FX1_CSI2_OK, 0x1D0, "C: DMA stopped");
    fx1_log("C: DMA stop and restart while streaming; capture resumed");
    return 0;
}
