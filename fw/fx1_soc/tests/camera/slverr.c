/*
 * CSI-2 M5b: AXI SLVERR on a camera frame (fx1_soc --camera --ddr3-ecc off
 * --ddr3-fault T:write-result:1). The DDR3 controller answers the next write
 * after T with SLVERR, whatever the master: T lies inside frame 2 and the CPU
 * only reads while frame 2 is captured (polling, no stores; the interrupt of
 * frame 1 came before T), so the failing write is the CSI-2 DMA's. ECC off:
 * with ECC the fault also locks the granule (DDR3 fatal ECC hold).
 *
 * Timing: stream on at main + CAM_SLVERR_ON_US; frame k starts about
 * main + CAM_SLVERR_ON_US + ~100 us + (k - 1) * CAM_FRAME_US (I2C write at
 * 400 kHz, then the sensor). The test's T (fw/fx1_soc/CMakeLists.txt) is
 * chosen in the middle of frame 2's 160 us of lines; the log prints the times
 * it relies on. Calibration (ECC off, no fault): CPUs released at 1005.08 us,
 * main 16.6 us later, frame 2 done at main + 6273 us, so its lines run from
 * about main + 6113 us: T = 6200 us after the release is about line 7.
 *
 * Checks: frame 1 exact; frame 2 BUF_DONE_ERR + AXI_WRITE_ERROR, side
 * SW_OWNED_ERROR with a last valid line inside the frame, fault response
 * SLVERR at an address of that side, SLVERR count 1; after the release the
 * next frame is exact.
 */
#include "camera.h"
#include "fx1_csi2/fx1_csi2_regs.h"

#define CAM_SLVERR_ON_US 3000u

static uint32_t now_us(uint64_t t0) { return (uint32_t)((fx1_mtime() - t0) / (FX1_MTIME_HZ / 1000000u)); }

int main(void)
{
    const uint64_t t0 = fx1_mtime();
    fx1_log_hex("slverr: main at mtime (low word)", (uint32_t)t0);
    cam_bringup(10, 4, 0, CAM_SIDE0, CAM_SIDE1);
    while (now_us(t0) < CAM_SLVERR_ON_US) {
    }
    fx1_check(fx1_ccs_stream(&g_cam.sensor, 1) == FX1_CCS_OK, 0x100, "slverr: stream on");
    uint32_t ev = cam_wait(CAM_BIT(CAM_IRQ_BUF_DONE_OK) | CAM_BIT(CAM_IRQ_BUF_DONE_ERR), CAM_WAIT_US);
    fx1_log_hex("slverr: frame 1 done at main + us", now_us(t0));
    fx1_check((ev & CAM_BIT(CAM_IRQ_BUF_DONE_OK)) && !(ev & CAM_BIT(CAM_IRQ_BUF_DONE_ERR)), 0x110, "slverr: frame 1 valid");
    /* Frame 2: reads only until its end. */
    ev = cam_wait(CAM_BIT(CAM_IRQ_BUF_DONE_OK) | CAM_BIT(CAM_IRQ_BUF_DONE_ERR), CAM_WAIT_US);
    fx1_log_hex("slverr: frame 2 done at main + us", now_us(t0));
    fx1_log_hex("slverr: frame 2 events", ev);
    fx1_check((ev & CAM_BIT(CAM_IRQ_BUF_DONE_ERR)) && (ev & CAM_BIT(CAM_IRQ_AXI_WRITE_ERROR)) &&
                  !(ev & CAM_BIT(CAM_IRQ_BUF_DONE_OK)),
              0x120, "slverr: frame 2 ended with BUF_DONE_ERR and AXI_WRITE_ERROR");
    const int bad = cam_side_in(FX1_CSI2_BUF_SW_OWNED_ERROR);
    const int good = cam_side_in(FX1_CSI2_BUF_SW_OWNED_VALID);
    fx1_check(bad >= 0 && good >= 0 && bad != good, 0x121, "slverr: one side valid, one side with error");
    fx1_csi2_buf_info bi;
    fx1_csi2_get_buffer(&g_cam.dev, (unsigned)bad, &bi);
    fx1_log_hex("slverr: last valid line", bi.last_valid_line);
    fx1_check(bi.last_valid_line != 0xFFFFu && bi.last_valid_line < CAM_H - 1u, 0x122,
              "slverr: lines before the failing write kept, the rest not");
    fx1_csi2_dma_fault f;
    fx1_check(fx1_csi2_get_dma_fault(&g_cam.dev, &f) == FX1_CSI2_OK, 0x123, "slverr: fault info");
    fx1_log_hex("slverr: fault address", f.addr);
    fx1_check(f.resp == 2u && f.slverr_count == 1u && f.decerr_count == 0u, 0x124, "slverr: one SLVERR");
    fx1_check(f.addr >= g_cam.base[bad] && f.addr < g_cam.base[bad] + CAM_H * g_cam.bpl, 0x125,
              "slverr: fault address inside the failing side's frame");
    fx1_check(cam_frame_number(good) == 1u && cam_check_frame(good, 1), 0x126, "slverr: frame 1 exact");
    cam_release(good);
    cam_release(bad);
    /* Frame 3 if the release came before its start, else 3 is dropped and 4 is next. */
    ev = cam_wait(CAM_BIT(CAM_IRQ_BUF_DONE_OK) | CAM_BIT(CAM_IRQ_BUF_DONE_ERR), CAM_WAIT_US);
    fx1_check((ev & CAM_BIT(CAM_IRQ_BUF_DONE_OK)) && !(ev & CAM_BIT(CAM_IRQ_BUF_DONE_ERR)), 0x130, "slverr: recovered");
    const int s = cam_side_in(FX1_CSI2_BUF_SW_OWNED_VALID);
    const uint32_t n = s >= 0 ? cam_frame_number(s) : 0;
    const uint32_t drops = fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_FRAME_DROP_COUNT);
    fx1_log_hex("slverr: next frame", n);
    fx1_check(s >= 0 && n >= 3u && n == 3u + drops && cam_check_frame(s, n), 0x131, "slverr: next frame exact");
    cam_release(s);
    (void)cam_stream_off_and_drain(0x140);
    fx1_check(fx1_csi2_dma_stop(&g_cam.dev, CAM_WAIT_US) == FX1_CSI2_OK, 0x150, "slverr: DMA stopped");
    fx1_log("slverr: SLVERR on frame 2 reported and contained; capture recovered");
    return 0;
}
