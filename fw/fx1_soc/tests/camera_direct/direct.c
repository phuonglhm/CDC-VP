/*
 * Direct CSI-2 -> ISP on fx1_soc --camera (plan DIRECT_CSI_ISP D2a; contract
 * components/FX1_Components/mipi-csi2/docs/DIRECT_CSI_ISP_CONTRACT.md).
 *
 * The sensor (CcsSensor through the detailed D-PHY) or the CSI-2 RX test
 * pattern generator streams RAW16 over the CSI's AXI4-Stream master into the
 * ISP's stream ingress (ISP_INGRESS at 0x1105_0000, SOURCE = direct); the CSI
 * RAW DMA stays disabled and the ISP IDMA off (IDMA_EN = 0): no RAW buffer, no
 * DDR bridge. Bare metal, polling, the C drivers directly (the camera
 * manager's direct backend is D3).
 *
 * For every frame: the ISP_INGRESS context entry (outcome OK, 64-bit capture
 * sequence, FRAME_ID, FS timestamp, lines, samples, ISP frame ID) against the
 * ODMA buffer's BUF_TAG / BUF_TS / TUNE_CH0_BUFn_FRAME, and the NV12 CRC
 * against the stand-alone ISP model's golden (camera_isp_golden.h: the direct
 * RAW12 path is the memory path's zero-copy input; RAW10 native is bit-exact
 * with the memory path's << 2 staging, D1a).
 *
 *   CAM_BITS=12|10   sensor 64x32 (RAW12 4 lanes, RAW10 2 lanes)
 *   CAM_TPG=1        CSI TPG 640x480 RAW12 instead of the sensor
 *   CAM_CASE=0       nominal: CAM_FRAMES frames, every one published
 *   CAM_CASE=1       held outputs: one output buffer, consumed late: frames
 *                    without an output credit are dropped at their SOF
 *                    (NO_CREDIT, TREADY 1), the published ones are exact
 *   CAM_CASE=2       ISP paused (ISP_EN 0) inside a frame for 5 frame
 *                    periods: the ingress back-pressures the CSI (TREADY 0)
 *                    until its Line Buffer overflows and it drops captures
 *                    (CSI_DROPPED, or NO_STREAM if dropped before their SOF);
 *                    the drop's Line Buffer flush may also cut an earlier
 *                    capture the CSI had ended OK: INVALID (ABORT) at the
 *                    ingress; none of them is published; the later frames
 *                    are exact
 *
 * The host checks the traffic of the run (fx1_soc --traffic-report):
 * MIPI_DMA writes 0, ISP_IDMA reads 0, ISP_ODMA writes > 0.
 */
#include <stdint.h>

#include "direct_common.h"

#ifndef CAM_CASE
#define CAM_CASE 0
#endif
#ifndef CAM_FRAMES
#define CAM_FRAMES 6u
#endif

#define isp_rd direct_isp_rd
#define isp_wr direct_isp_wr
#define ticks direct_ticks
#define wait_ctx direct_wait_ctx
#define find_done direct_find_done
#define arm direct_arm
#define tpg_frame direct_tpg_frame
#define check_frame direct_check_frame

int main(void)
{
    fx1_log("direct CSI-2 -> ISP");
    direct_setup(CAM_CASE == 1 ? 1u : FX1_ISP_NUM_BUFFERS);
    direct_start();

    unsigned buf = 0, published = 0, dropped = 0;
    uint32_t last_n = 0, last_fid = 0;
    uint64_t last_seq = 0, last_ts = 0;
    for (unsigned k = 0; published < CAM_FRAMES && k < 4u * CAM_FRAMES; ++k) {
#if CAM_CASE == 2
        if (k == 2) {  /* pause the ISP inside the next frame: back-pressure until the CSI drops it */
            const uint64_t t = fx1_mtime() + ticks(CAM_ISP_FRAME_US / 2u);
            while (!(fx1_read32(FX1_ISP_INGRESS_BASE + FX1_ISP_INGRESS_STATUS_OFFSET) &
                     FX1_ISP_INGRESS_STATUS_IN_FRAME_MASK) && fx1_mtime() < t + ticks(CAM_ISP_FRAME_US)) {
            }
            isp_wr(FX1_ISP_COMMON_CTRL_OFFSET, 0);
            const uint64_t hold = fx1_mtime() + ticks(5u * CAM_ISP_FRAME_US);
            while (fx1_mtime() < hold) {
            }
            isp_wr(FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
        }
#endif
        fx1_check(wait_ctx(4u * CAM_ISP_FRAME_US + (CAM_TPG ? 400000u : 0u)), 0x20, "direct: a context entry READY");
        fx1_isp_ingress_ctx c;
        fx1_check(fx1_isp_ingress_ctx_pop(&g_ing, &c) == FX1_ISP_OK, 0x21, "direct: context popped");
        fx1_check(c.seq > last_seq, 0x22, "direct: capture sequence increases");
        fx1_check(!last_seq || c.ts_ns > last_ts, 0x23, "direct: FS timestamp increases");
        last_seq = c.seq;
        last_ts = c.ts_ns;
        if (c.outcome != FX1_ISP_INGRESS_OUTCOME_OK) {
            /* Not published: its output buffer, if it took one, is released unread. */
            fx1_log_hex("  not OK: outcome | cause << 8", c.outcome | (c.cause << 8));
            fx1_log_hex("  lines", c.lines);
            /* Under back-pressure the CSI drops a capture (CSI_DROPPED); its Line
             * Buffer flush can also cut the tail of an earlier capture that the
             * CSI had already ended OK: that one is INVALID (ABORT / MISSING_LINES)
             * at the ingress, never published. */
            const int expected = c.outcome == FX1_ISP_INGRESS_OUTCOME_CSI_DROPPED ||
                                 (c.outcome == FX1_ISP_INGRESS_OUTCOME_INVALID &&
                                  (c.cause == FX1_ISP_INGRESS_CAUSE_ABORT || c.cause == FX1_ISP_INGRESS_CAUSE_MISSING_LINES));
            fx1_check(CAM_CASE == 2 && expected && c.lines < H, 0x24,
                      "direct: only frames cut by the back-pressure drop fail (CSI_DROPPED / INVALID)");
            ++dropped;
            const int out = find_done(c.tag);  /* an output it completed: released unread */
            if (out >= 0) arm((unsigned)out);
#if CAM_TPG
            tpg_frame();
#endif
            continue;
        }
        const int out = find_done(c.tag);
        fx1_check(out >= 0 && (CAM_CASE != 0 || (unsigned)out == buf), 0x2A,
                  "direct: ODMA DONE with BUF_TAG = context TAG (nominal: in rotation order)");
        buf = (unsigned)out;
        const uint32_t n = check_frame(&c, buf, 0x30);
#if !CAM_TPG
        fx1_check(n > last_n, 0x28, "direct: sensor frames in order");
        if (CAM_CASE == 0 && last_n)
            fx1_check(n == last_n + 1u && c.frame_id == ((last_fid + 1u) & 0x3FFFu), 0x29,
                      "direct: consecutive sensor frames and FRAME_IDs");
#endif
        last_n = n;
        last_fid = c.frame_id;
        (void)last_fid;
        (void)last_n;
        ++published;
#if CAM_CASE == 1
        /* Consumed late: two frame periods with the only buffer held; then the
         * next buffer of the rotation is the only one armed. */
        const uint64_t hold = fx1_mtime() + ticks(2u * CAM_ISP_FRAME_US);
        while (fx1_mtime() < hold) {
        }
        isp_wr(FX1_ISP_ODMA_BUF_DONE_OFFSET, 1u << buf);
        buf = (buf + 1u) % FX1_ISP_NUM_BUFFERS;
        arm(buf);
#else
        arm(buf);
        buf = (buf + 1u) % FX1_ISP_NUM_BUFFERS;
#endif
#if CAM_TPG
        if (published < CAM_FRAMES) tpg_frame();
#endif
    }
    fx1_check(published == CAM_FRAMES, 0x40, "direct: frames published");
    fx1_log_hex("  failed (not published)", dropped);

#if !CAM_TPG
    fx1_check(fx1_ccs_stream(&g_cam.sensor, 0) == FX1_CCS_OK, 0x41, "direct: sensor stopped");
#endif
    /* Drain: no frame in the ingress or the ISP; then back to memory. */
    fx1_isp_ingress_counters k;
    direct_drain(&k, 8u * CAM_ISP_FRAME_US);
    fx1_check(k.captures_untracked == 0 && k.drop_no_context == 0, 0x43, "direct: every capture tracked");
    if (CAM_CASE == 1) fx1_check(k.drop_no_credit > 0, 0x44, "direct: frames dropped without output credit");
    if (CAM_CASE == 2)
        fx1_check(dropped + k.drop_no_stream >= 1 && k.stall_cycles > 0, 0x45, "direct: back-pressure, capture dropped");
    if (CAM_CASE != 2) fx1_check(k.frames_invalid == 0, 0x42, "direct: no invalid frame");
    if (CAM_CASE == 0) fx1_check(k.drop_no_credit == 0 && dropped == 0, 0x46, "direct: nothing dropped");
    fx1_check(fx1_isp_ingress_set_source(&g_ing, FX1_ISP_INGRESS_SOURCE_MEMORY) == FX1_ISP_OK, 0x47,
              "direct: SOURCE back to memory");
    fx1_log("direct CSI-2 -> ISP: PASS");
    return 0;
}
