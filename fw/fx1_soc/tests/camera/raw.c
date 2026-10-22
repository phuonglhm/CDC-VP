/*
 * CSI-2 M5b: camera capture smoke (fx1_soc --camera). Built twice:
 *   camera_raw10  CAM_BITS 10, CAM_LANES 4, packed RAW10 in DDR
 *   camera_raw12  CAM_BITS 12, CAM_LANES 2, RAW16 right-aligned in DDR
 * Sensor programmed over I2C0 (64x16), three frames captured through the
 * detailed D-PHY and the CSI-2 RX DMA, each interrupt taken at PLIC source 7,
 * each frame checked pixel by pixel against the sensor pattern (frame numbers
 * 1, 2, 3) and released; stream off at a frame boundary, DMA stopped,
 * counters consistent at the receiver and the sensor.
 */
#include "camera.h"
#include "fx1_csi2/fx1_csi2_regs.h"

#ifndef CAM_BITS
#error CAM_BITS required
#endif

int main(void)
{
    cam_bringup(CAM_BITS, CAM_LANES, CAM_RAW16, CAM_SIDE0, CAM_SIDE1);
    fx1_log_hex("camera: bits << 8 | lanes", CAM_BITS << 8 | CAM_LANES);
    fx1_check(fx1_ccs_stream(&g_cam.sensor, 1) == FX1_CCS_OK, 0x100, "camera: stream on");
    for (uint32_t f = 1; f <= 3; ++f) cam_expect_frame(f, 0x110 + (f << 4));
    fx1_log_hex("camera: frame handling (check, refill, release), us", g_cam_handle_us);
    fx1_log("camera: frames 1-3 exact, released from the interrupt");
    const uint32_t extra = cam_stream_off_and_drain(0x150);
    fx1_check(fx1_csi2_dma_stop(&g_cam.dev, CAM_WAIT_US) == FX1_CSI2_OK, 0x160, "camera: DMA stopped");

    fx1_csi2_status st;
    fx1_check(fx1_csi2_get_status(&g_cam.dev, &st) == FX1_CSI2_OK, 0x170, "camera: status");
    uint8_t sensor_frames = 0;
    fx1_check(fx1_ccs_frame_count(&g_cam.sensor, &sensor_frames) == FX1_CCS_OK, 0x171, "camera: sensor frame_count");
    fx1_log_hex("camera: FRAME_COUNT", st.frame_count);
    fx1_log_hex("camera: frames after stream off", extra);
    fx1_log_hex("camera: sensor frame_count", sensor_frames);
    fx1_check(st.frame_count == 3u + extra && sensor_frames == st.frame_count, 0x172,
              "camera: every frame sent was received");
    fx1_check(st.frame_drop_count == 0 && st.crc_error_count == 0 && st.ecc_error_count == 0, 0x173,
              "camera: no drop, no CRC or ECC error");
    fx1_check(!(st.irq_status & CAM_BIT(CAM_IRQ_RESOLUTION_MISMATCH)), 0x174, "camera: resolution as expected");
    fx1_check(g_cam_irqs >= 3u, 0x175, "camera: interrupts through the PLIC");
    fx1_log("camera: stream off, DMA stopped, counters consistent");
    return 0;
}
