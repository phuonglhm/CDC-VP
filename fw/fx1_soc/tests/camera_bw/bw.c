/*
 * Bandwidth benchmark (plan CSI_ISP_BW2): sensor -> detailed D-PHY -> CSI-2 RX
 * -> AXI4-Stream -> ISP -> ODMA -> DDR, direct path, through the camera
 * manager, on fx1_soc --camera with --bw-report (or --bw-marker for the
 * control run). Bare metal: no scheduler, no UART output inside the window.
 *
 * The sensor runs from its own timing (line_length x frame_length pixel
 * clocks at 200 MHz); the firmware never triggers a frame. Inside the window
 * it only dequeues and releases each output (no CRC, no print) and keeps a
 * small record per frame; after STOP it prints the records and checks that no
 * frame was dropped or failed and that the outputs came one per sensor frame
 * (consecutive FRAME_IDs: the consumer kept up). NV12 correctness is checked
 * on the host from the VP's NV12 dump (check_bw_report.py, golden CRCs).
 *
 * Window marker: SIM_CTRL BW_MARK (FX1_SIM_CTRL_BW_MARK_OFF). Without
 * --bw-marker / --bw-report that write is a bus error: the image is for those runs.
 *
 * Build-time parameters:
 *   CAM_BITS   12 (4 lanes) or 10 (2 lanes)
 *   BW_STEADY  0: batch -- START(batch, BW_N) before the camera starts, the
 *              first BW_N captures are the set, STOP after them;
 *              1: steady -- BW_WARMUP frames, START(steady), BW_N frames, STOP
 *   BW_N, BW_WARMUP, BW_LLP (pixel clocks per line), BW_FLL (lines per frame)
 */
#include "cam_isp.h"
#include "fx1/fx1_memory_map.h"
#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_csi2_hal_mmio.h"
#include "fx1_isp_hal_mmio.h"

#ifndef CAM_BITS
#error CAM_BITS required
#endif
#ifndef BW_STEADY
#define BW_STEADY 0
#endif
#ifndef BW_N
#define BW_N 4u
#endif
#ifndef BW_WARMUP
#define BW_WARMUP 0u
#endif
#ifndef BW_LLP
#define BW_LLP 2000u
#endif
#ifndef BW_FLL
#define BW_FLL 3333u
#endif
#define BW_PERIOD_US ((BW_LLP * BW_FLL) / 200u) /* pixel clock 200 MHz */
#define BW_WAIT_US (4u * BW_PERIOD_US)
#define BW_TOTAL (BW_WARMUP + BW_N)

typedef struct {
    uint32_t capture;    /* low 32 bits of the CSI capture sequence */
    uint16_t frame_id;   /* CSI FRAME_ID */
    uint32_t isp_frame;  /* ISP frame ID */
    uint32_t tag;        /* ingress TAG */
    uint64_t t_dequeue;  /* mtime when the frame was dequeued */
} bw_rec;

static bw_rec recs[BW_TOTAL];

static void mark(uint32_t value) { fx1_write32(FX1_SIM_CTRL_BASE + FX1_SIM_CTRL_BW_MARK_OFF, value); }

static void take(uint32_t k)
{
    fx1_camera_frame f;
    const int rc = fx1_camera_dequeue(&g_cam, &f, BW_WAIT_US);
    if (rc != FX1_CAMERA_OK) {
        fx1_log_hex("bw: dequeue rc", (uint32_t)rc);
        fx1_log_hex("bw: frame", k);
        fx1_fail(0x400 + k);
    }
    recs[k].t_dequeue = fx1_mtime();
    recs[k].capture = f.capture;
    recs[k].frame_id = f.csi_frame_id;
    recs[k].isp_frame = f.isp_frame;
    recs[k].tag = f.isp_seq;
    if (fx1_camera_release(&g_cam, &f) != FX1_CAMERA_OK) fx1_fail(0x500 + k);
}

int main(void)
{
    fx1_global_irq_disable();
    fx1_check(fx1_camera_probe(&g_cam, fx1_csi2_hal_mmio(FX1_MIPI_CSR_BASE), fx1_isp_hal_mmio(), 0) == FX1_CAMERA_OK,
              0x10, "bw: probe (CSI-2 RX, ISP, sensor)");
    fx1_camera_cfg cfg = cam_isp_cfg(CAM_BITS);
    cfg.path = FX1_CAMERA_PATH_DIRECT;
    cfg.line_length = BW_LLP;
    cfg.frame_length = BW_FLL;
    fx1_check(fx1_camera_configure(&g_cam, &cfg) == FX1_CAMERA_OK, 0x11, "bw: configured (direct)");
    cam_isp_irqs();
    fx1_log_hex("bw: bits", CAM_BITS);
    fx1_log_hex("bw: steady", BW_STEADY);
    fx1_log_hex("bw: frames in the set / window", BW_N);
    fx1_log_hex("bw: warm-up frames", BW_WARMUP);
    fx1_log_hex("bw: line_length_pck", BW_LLP);
    fx1_log_hex("bw: frame_length_lines", BW_FLL);
#if !BW_STEADY
    mark(FX1_SIM_CTRL_BW_START | FX1_SIM_CTRL_BW_MODE_BATCH | (BW_N << FX1_SIM_CTRL_BW_COUNT_SHIFT));
#endif
    fx1_check(fx1_camera_start(&g_cam) == FX1_CAMERA_OK, 0x12, "bw: started");
    for (uint32_t k = 0; k < BW_TOTAL; ++k) {
#if BW_STEADY
        if (k == BW_WARMUP) mark(FX1_SIM_CTRL_BW_START | FX1_SIM_CTRL_BW_MODE_STEADY);
#endif
        take(k);
    }
    mark(FX1_SIM_CTRL_BW_STOP);
    /* Outside the window from here: reads, stop, prints and checks. */
    const uint32_t drops = fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_FRAME_DROP_COUNT);
    fx1_check(fx1_camera_stop(&g_cam, 2u * BW_WAIT_US) == FX1_CAMERA_OK, 0x300, "bw: stopped");
    uint32_t gaps = 0;
    for (uint32_t k = 0; k < BW_TOTAL; ++k) {
        fx1_puts("bw frame ");
        fx1_putdec(k);
        fx1_puts(" capture ");
        fx1_putdec(recs[k].capture);
        fx1_puts(" frame_id ");
        fx1_putdec(recs[k].frame_id);
        fx1_puts(" isp_frame ");
        fx1_putdec(recs[k].isp_frame);
        fx1_puts(" tag ");
        fx1_putdec(recs[k].tag);
        fx1_puts(" mtime ");
        fx1_putdec((uint32_t)recs[k].t_dequeue);
        fx1_puts("\n");
        if (k && recs[k].frame_id != (uint16_t)((recs[k - 1].frame_id + 1u) & 0x3FFFu)) ++gaps;
    }
    const fx1_camera_stats* s = &g_cam.stats;
    uint32_t failed = 0;
    for (unsigned i = 0; i < FX1_CAMERA_OUTCOMES; ++i) failed += s->direct_failed[i];
    fx1_log_hex("bw: csi drops while streaming", drops);
    fx1_log_hex("bw: direct ok", s->direct_ok);
    fx1_log_hex("bw: direct failed", failed);
    fx1_log_hex("bw: frame id gaps", gaps);
    fx1_check(drops == 0 && failed == 0 && s->isp_errors == 0, 0x301, "bw: no drop, no failed capture");
    fx1_check(s->direct_ok >= BW_TOTAL, 0x302, "bw: every frame through the ISP");
    fx1_check(gaps == 0, 0x303, "bw: one output per sensor frame (consumer kept up)");
    fx1_log("bw: PASS");
    return 0;
}
