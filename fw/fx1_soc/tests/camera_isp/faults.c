/*
 * CSI-2 M6a: camera -> ISP faults (fx1_soc --camera, RAW12 zero-copy). One
 * image per fault (CAM_FAULT):
 *   1 csi_err      CSI-2 side 1 at an unmapped address: its frames end with an
 *                  error and never reach the ISP; the frames of side 0 do, exact
 *   2 backpressure the application holds all four NV12 outputs: the manager
 *                  stops feeding the ISP, the held CSI-2 sides make the receiver
 *                  drop frames (counted); after the release the two held frames
 *                  come out intact, then the stream resumes after the drops
 *   3 odma_err     NV12 output slot 2 at an unmapped address: ODMA error, the
 *                  application repairs the slot from the error hook, the manager
 *                  recovers, that frame is lost (counted), the next ones exact
 *   4 timeout      the sensor stops streaming: dequeue returns
 *                  FX1_CAMERA_ETIMEOUT within its budget; stop still works
 *   5 idma_hold    the ISP's input DMA paused (DMA_CTRL.IDMA_EN = 0, test-only
 *                  poke): frames handed over zero-copy stay unread, so their
 *                  CSI-2 sides must stay out of the receiver's hands (both
 *                  held, the receiver drops); IDMA resumed, the two held
 *                  frames come out intact, then the stream after the drops.
 *                  Catches a side released before the ISP has read it, which
 *                  at 64x16 is otherwise invisible (the IDMA reads in µs)
 *   6 idma_err     review M6a F1: an IDMA read error on frame 2's zero-copy
 *                  input (its IDMA_BUF_ADDR made unmapped while the IDMA is
 *                  paused), then 7 ms with only the ISRs running while the
 *                  sensor keeps streaming; the error hook repairs the address,
 *                  recovery re-reads the same input. The side must stay held
 *                  meanwhile: the retried output is capture 2 with the pixels
 *                  of sensor frame 2, the stream resumes after the drops
 *   7 sensor_stop_timeout  review M6a F2: the I2C host disabled, so the
 *                  standby never reaches the sensor: stop returns
 *                  FX1_CAMERA_ESENSOR within its deadline, state 3, CSI-2 RX
 *                  and ISP stopped, configure refused, the sensor still
 *                  streaming; I2C back: stop again -> OK, mode_select 0, the
 *                  sensor's frame count stops; configure and start again,
 *                  exact consecutive frames
 *   8 sensor_stop_nack     the same with the sensor address wrong (NACK)
 *  10 sensor_stop_budget   re-review M6a R1: stop budgets shorter than the
 *                  sensor's I2C timeout (2000 µs per transfer). I2C host
 *                  disabled: stop(100 µs) from running -> ESENSOR, state 3;
 *                  stop(100 µs) again in state 3 -> ESENSOR; each within
 *                  100 µs + FX1_CAMERA_STOP_SLACK_US. I2C back: stop(1000 µs)
 *                  -> OK within its bound (write and read-back share the
 *                  deadline), mode_select 0; configure and start again, exact.
 *                  Read-back cut: one standby write timed in state 0 (W µs);
 *                  stop(W + 30 µs) while running -> the write gets through, the
 *                  read-back has no time left: ESENSOR, state 3, within its
 *                  bound, although the sensor is in standby; stop(1000 µs)
 *                  -> OK; configure and start again, exact
 *  11 start_fail   M6b review F1: the I2C host lost between configure and
 *                  start: start -> FX1_CAMERA_EDEV, the manager stays
 *                  configured (state 1) with the CSI-2 DMA started; I2C back:
 *                  stop -> OK, state 0, both sides back, the sensor does not
 *                  stream; configure and start again, exact frames
 *   9 stop_held    stop while the application holds all four outputs and two
 *                  captures wait for the ISP: stop completes within its
 *                  deadline, the waiting captures are cancelled (counted), the
 *                  held NV12 stay readable and exact, releasing them after the
 *                  stop is OK (a second release is not); configure and start
 *                  again, exact consecutive frames
 * Every NV12 that comes out equals the ISP model's for its frame number.
 */
#include "cam_isp.h"
#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_csi2_hal_mmio.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp_hal_mmio.h"

#ifndef CAM_FAULT
#error CAM_FAULT required
#endif
#define UNMAPPED 0x60000000u

__attribute__((unused)) static uint32_t drops(void) { return fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_FRAME_DROP_COUNT); }
static uint32_t us_since(uint64_t t) { return (uint32_t)((fx1_mtime() - t) / (FX1_MTIME_HZ / 1000000u)); }

#define STOP_US (2u * CAM_ISP_WAIT_US)

__attribute__((unused)) static void wait_us(uint32_t us)
{
    const uint64_t t = fx1_mtime();
    while (us_since(t) < us) {
    }
}

__attribute__((unused)) static uint8_t sensor_frames(void)
{
    uint8_t n = 0;
    fx1_check(fx1_ccs_frame_count(&g_cam.sensor, &n) == FX1_CCS_OK, 0x1F0, "faults: sensor frame_count read");
    return n;
}

/* Configure and start again after a stop; two exact consecutive frames. */
__attribute__((unused)) static void restart_exact(const fx1_camera_cfg* cfg, uint32_t code)
{
    fx1_check(fx1_camera_configure(&g_cam, cfg) == FX1_CAMERA_OK, code, "faults: configured again after the stop");
    fx1_check(fx1_camera_start(&g_cam) == FX1_CAMERA_OK, code + 1u, "faults: started again");
    const uint32_t a = cam_isp_take(12, code + 0x10u), b = cam_isp_take(12, code + 0x20u);
    fx1_log_hex("  after restart, frame", a);
    fx1_check(b == a + 1u, code + 2u, "faults: exact consecutive frames after the restart");
}

static unsigned g_bad_input;
static uint32_t g_bad_side;

__attribute__((unused)) static void repair_input(void* ctx, uint32_t dma_err)
{
    (void)ctx;
    fx1_log_hex("  ISP DMA_ERR", dma_err);
    fx1_write32(FX1_ISP_CSR_BASE + FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET + 8u * g_bad_input, g_bad_side);
    fx1_write32(FX1_ISP_CSR_BASE + FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET + 8u * g_bad_input + 4u, 0);
}

__attribute__((unused)) static void repair_slot2(void* ctx, uint32_t dma_err)
{
    (void)ctx;
    fx1_log_hex("  ISP DMA_ERR", dma_err);
    fx1_camera_set_output(&g_cam, 2, CAM_ISP_Y(2), CAM_ISP_UV(2));
}

__attribute__((unused)) static void bring_up(fx1_camera_cfg* cfg)
{
    fx1_global_irq_disable();
    fx1_check(fx1_camera_probe(&g_cam, fx1_csi2_hal_mmio(FX1_MIPI_CSR_BASE), fx1_isp_hal_mmio(), 0) == FX1_CAMERA_OK,
              0x10, "faults: probe");
    fx1_check(fx1_camera_configure(&g_cam, cfg) == FX1_CAMERA_OK, 0x11, "faults: configured");
    cam_isp_irqs();
    fx1_check(fx1_camera_start(&g_cam) == FX1_CAMERA_OK, 0x12, "faults: started");
}

int main(void)
{
    fx1_camera_cfg cfg = cam_isp_cfg(12);
#if CAM_FAULT == 1
    cfg.csi_side[1] = UNMAPPED;
    bring_up(&cfg);
    uint32_t last = 0;
    for (uint32_t k = 0; k < 4; ++k) {
        fx1_camera_frame f;
        fx1_check(fx1_camera_dequeue(&g_cam, &f, CAM_ISP_WAIT_US) == FX1_CAMERA_OK, 0x100 + k, "csi_err: frame");
        const uint32_t n = cam_isp_frame_of(12, cam_isp_crc(&f, CAM_ISP_W, CAM_ISP_W));
        fx1_log_hex("  frame", n);
        fx1_check(n > last && f.csi_side == 0 && f.isp_in == CAM_ISP_SIDE(0), 0x110 + k,
                  "csi_err: exact frame, from the good side only");
        last = n;
        fx1_camera_release(&g_cam, &f);
    }
    const fx1_camera_stats* s = &g_cam.stats;
    fx1_log_hex("  csi err", s->csi_err);
    fx1_check(s->csi_err >= 3u && s->isp_queued == s->csi_ok && s->isp_errors == 0, 0x120,
              "csi_err: error sides released, never queued to the ISP");
    fx1_log("csi_err: frames with a CSI-2 error stay out of the ISP");
#elif CAM_FAULT == 2
    bring_up(&cfg);
    fx1_camera_frame held[4];
    for (uint32_t k = 0; k < 4; ++k) { /* consumed, not released */
        fx1_check(fx1_camera_dequeue(&g_cam, &held[k], CAM_ISP_WAIT_US) == FX1_CAMERA_OK, 0x100 + k, "backpressure: frame");
        fx1_check(cam_isp_frame_of(12, cam_isp_crc(&held[k], CAM_ISP_W, CAM_ISP_W)) == k + 1u, 0x110 + k,
                  "backpressure: frames 1-4 exact");
    }
    const uint64_t t = fx1_mtime();
    while (us_since(t) < 5u * CAM_ISP_FRAME_US) fx1_camera_poll(&g_cam); /* nothing can go to the ISP */
    const uint32_t d0 = drops();
    fx1_log_hex("  drops while held", d0);
    fx1_check(d0 >= 2u && g_cam.stats.held_for_isp > 0, 0x120, "backpressure: CSI-2 drops counted");
    for (uint32_t k = 0; k < 4; ++k) fx1_camera_release(&g_cam, &held[k]);
    fx1_check(cam_isp_take(12, 0x130) == 5u, 0x131, "backpressure: held frame 5 intact");
    fx1_check(cam_isp_take(12, 0x140) == 6u, 0x141, "backpressure: held frame 6 intact");
    const uint32_t d1 = drops();
    const uint32_t n = cam_isp_take(12, 0x150);
    fx1_log_hex("  next frame", n);
    fx1_check(n == 7u + d1, 0x151, "backpressure: stream resumes right after the dropped frames");
    fx1_log("backpressure: held outputs stall the ISP feed, the receiver drops, nothing corrupt");
#elif CAM_FAULT == 3
    cfg.y[2] = UNMAPPED;
    cfg.uv[2] = UNMAPPED + 0x8000u;
    cfg.isp_error_hook = repair_slot2;
    bring_up(&cfg);
    uint32_t seen[6], k = 0;
    while (k < 6) {
        seen[k] = cam_isp_take(12, 0x100 + (k << 4));
        fx1_log_hex("  frame", seen[k]);
        ++k;
    }
    const fx1_camera_stats* s = &g_cam.stats;
    fx1_log_hex("  isp errors", s->isp_errors);
    fx1_log_hex("  isp lost", s->isp_lost);
    fx1_check(s->isp_errors >= 1u && s->isp_lost == 1u, 0x120, "odma_err: one ISP error, one frame lost");
    fx1_check(seen[0] == 1u && seen[1] == 2u && seen[2] == 4u && seen[5] == 7u, 0x121,
              "odma_err: frame 3 lost, the others exact and in order");
    fx1_log("odma_err: ODMA error repaired and recovered, one frame lost");
#elif CAM_FAULT == 4
    bring_up(&cfg);
    fx1_check(cam_isp_take(12, 0x100) == 1u && cam_isp_take(12, 0x110) == 2u, 0x101, "timeout: frames 1-2");
    fx1_check(fx1_ccs_stream(&g_cam.sensor, 0) == FX1_CCS_OK, 0x120, "timeout: sensor stops");
    fx1_camera_frame f;
    int rc;
    while ((rc = fx1_camera_dequeue(&g_cam, &f, 2u * CAM_ISP_FRAME_US)) == FX1_CAMERA_OK) /* the frame in progress */
        fx1_camera_release(&g_cam, &f);
    const uint64_t t = fx1_mtime();
    rc = fx1_camera_dequeue(&g_cam, &f, 2u * CAM_ISP_FRAME_US);
    const uint32_t el = us_since(t);
    fx1_log_hex("  dequeue after the stop: us", el);
    fx1_check(rc == FX1_CAMERA_ETIMEOUT && el >= 2u * CAM_ISP_FRAME_US && el < 2u * CAM_ISP_FRAME_US + 200u, 0x121,
              "timeout: ETIMEOUT within its budget");
    fx1_check(g_cam.stats.timeouts >= 2u, 0x122, "timeout: counted");
#elif CAM_FAULT == 5
    bring_up(&cfg);
    fx1_check(cam_isp_take(12, 0x100) == 1u, 0x101, "idma_hold: frame 1");
    const uint32_t ctrl = fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_DMA_CTRL_OFFSET);
    fx1_write32(FX1_ISP_CSR_BASE + FX1_ISP_DMA_CTRL_OFFSET, ctrl & ~FX1_ISP_DMA_CTRL_IDMA_EN_MASK);
    const uint64_t t = fx1_mtime();
    while (us_since(t) < 5u * CAM_ISP_FRAME_US) fx1_camera_poll(&g_cam); /* frames 2, 3 handed over, unread */
    const uint32_t d0 = drops();
    fx1_log_hex("  drops while the IDMA is paused", d0);
    fx1_log_hex("  isp queued", g_cam.stats.isp_queued);
    fx1_check(g_cam.side_state[0] == 2 && g_cam.side_state[1] == 2, 0x120,
              "idma_hold: both zero-copy sides held until the ISP reads them");
    fx1_check(d0 >= 2u && g_cam.stats.isp_queued == 3u, 0x121, "idma_hold: the receiver drops, nothing overwritten");
    fx1_write32(FX1_ISP_CSR_BASE + FX1_ISP_DMA_CTRL_OFFSET, ctrl);
    fx1_check(cam_isp_take(12, 0x130) == 2u, 0x131, "idma_hold: held frame 2 intact");
    fx1_check(cam_isp_take(12, 0x140) == 3u, 0x141, "idma_hold: held frame 3 intact");
    const uint32_t d1 = drops();
    const uint32_t n = cam_isp_take(12, 0x150);
    fx1_log_hex("  next frame", n);
    fx1_check(n == 4u + d1, 0x151, "idma_hold: stream resumes right after the dropped frames");
    fx1_log("idma_hold: zero-copy sides stay with the ISP until its IDMA has read them");
#elif CAM_FAULT == 6
    cfg.isp_error_hook = repair_input;
    bring_up(&cfg);
    fx1_check(cam_isp_take(12, 0x100) == 1u, 0x101, "idma_err: frame 1");
    const uint32_t ctrl = fx1_read32(FX1_ISP_CSR_BASE + FX1_ISP_DMA_CTRL_OFFSET);
    fx1_write32(FX1_ISP_CSR_BASE + FX1_ISP_DMA_CTRL_OFFSET, ctrl & ~FX1_ISP_DMA_CTRL_IDMA_EN_MASK);
    uint64_t t = fx1_mtime();
    while (g_cam.stats.isp_queued < 2u && us_since(t) < CAM_ISP_WAIT_US) fx1_camera_poll(&g_cam);
    fx1_check(g_cam.stats.isp_queued == 2u, 0x110, "idma_err: frame 2 handed over, unread");
    const unsigned s = g_cam.side_state[0] == 2 ? 0u : 1u;
    fx1_check(g_cam.side_state[s] == 2, 0x111, "idma_err: frame 2's side with the ISP");
    g_bad_side = CAM_ISP_SIDE(s);
    g_bad_input = g_cam.side_input[s];
    fx1_write32(FX1_ISP_CSR_BASE + FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET + 8u * g_bad_input, UNMAPPED);
    fx1_write32(FX1_ISP_CSR_BASE + FX1_ISP_DMA_CTRL_OFFSET, ctrl); /* the IDMA arms on the bad address */
    wait_us(7000u); /* only the ISRs run; the sensor keeps streaming */
    fx1_check((g_cam.isp_fatal & FX1_ISP_DMA_ERR_ERR_AXI_RD_BIT) != 0, 0x120, "idma_err: read error collected by the ISR");
    fx1_check(g_cam.side_state[s] == 2, 0x121, "idma_err: side held while the read error is pending");
    fx1_camera_frame f;
    fx1_check(fx1_camera_dequeue(&g_cam, &f, CAM_ISP_WAIT_US) == FX1_CAMERA_OK, 0x130, "idma_err: retried frame");
    const uint32_t n = cam_isp_frame_of(12, cam_isp_crc(&f, CAM_ISP_W, CAM_ISP_W));
    fx1_log_hex("  retried capture", f.capture);
    fx1_log_hex("  retried output = sensor frame", n);
    fx1_check(f.capture == 2u && n == 2u && f.csi_side == s && f.isp_in == g_bad_side, 0x131,
              "idma_err: the retry read the original frame 2");
    fx1_camera_release(&g_cam, &f);
    fx1_check(cam_isp_take(12, 0x140) == 3u, 0x141, "idma_err: frame 3 (the other side) intact");
    const uint32_t d1 = drops();
    const uint32_t m = cam_isp_take(12, 0x150);
    fx1_log_hex("  drops", d1);
    fx1_log_hex("  next frame", m);
    fx1_check(d1 >= 1u && m == 4u + d1, 0x151, "idma_err: stream resumes right after the dropped frames");
    fx1_check(g_cam.stats.isp_errors == 1u && g_cam.stats.isp_lost == 0, 0x152, "idma_err: one error, nothing lost");
    fx1_log("idma_err: the zero-copy side stays held until the retried read succeeds");
#elif CAM_FAULT == 7 || CAM_FAULT == 8
    bring_up(&cfg);
    fx1_check(cam_isp_take(12, 0x100) == 1u && cam_isp_take(12, 0x110) == 2u, 0x101, "sensor_stop: frames 1-2");
#if CAM_FAULT == 7
    fx1_write32(FX1_I2C0_BASE + 0x10u, 0); /* I2C CTRL.enablehost = 0: the standby never reaches the sensor */
#else
    g_cam.sensor.addr7 = FX1_CCS_ADDR + 1u; /* no device there: NACK */
#endif
    uint64_t t = fx1_mtime();
    int rc = fx1_camera_stop(&g_cam, STOP_US);
    uint32_t el = us_since(t);
    fx1_log_hex("  stop rc", (uint32_t)rc);
    fx1_log_hex("  stop us", el);
    fx1_check(rc == FX1_CAMERA_ESENSOR && g_cam.state == 3 && el <= STOP_US + FX1_CAMERA_STOP_SLACK_US, 0x120,
              "sensor_stop: ESENSOR within the deadline, state 3");
    /* A sensor that does not answer fails at its own I2C timeout, not at the
     * stop's (larger) budget; the frame in flight may add one period. */
    fx1_check(el <= g_cam.sensor.timeout_us + CAM_ISP_FRAME_US + FX1_CAMERA_STOP_SLACK_US, 0x123,
              "sensor_stop: each transfer capped at the sensor's timeout");
    fx1_check(g_cam.side_state[0] == 0 && g_cam.side_state[1] == 0 && g_cam.stats.sensor_errors == 1u, 0x121,
              "sensor_stop: CSI-2 RX and ISP stopped anyway");
    fx1_check(fx1_camera_configure(&g_cam, &cfg) == FX1_CAMERA_ESTATE, 0x122, "sensor_stop: configure refused in state 3");
#if CAM_FAULT == 7
    fx1_write32(FX1_I2C0_BASE + 0x10u, 1);
#else
    g_cam.sensor.addr7 = FX1_CCS_ADDR;
#endif
    const uint8_t c0 = sensor_frames();
    wait_us(2u * CAM_ISP_FRAME_US);
    const uint8_t c1 = sensor_frames();
    fx1_check((uint8_t)(c1 - c0) >= 1u, 0x130, "sensor_stop: the sensor was still streaming");
    t = fx1_mtime();
    rc = fx1_camera_stop(&g_cam, STOP_US);
    fx1_log_hex("  stop again rc", (uint32_t)rc);
    int on = 1;
    fx1_check(rc == FX1_CAMERA_OK && g_cam.state == 0 && fx1_ccs_stream_state(&g_cam.sensor, &on) == FX1_CCS_OK && !on,
              0x131, "sensor_stop: stop again -> OK, sensor in standby");
    wait_us(2u * CAM_ISP_FRAME_US); /* the frame in progress ends */
    const uint8_t c2 = sensor_frames();
    wait_us(2u * CAM_ISP_FRAME_US);
    fx1_check(sensor_frames() == c2, 0x132, "sensor_stop: the sensor stopped");
    restart_exact(&cfg, 0x140);
    fx1_log("sensor_stop: a lost standby is reported, retried, and the camera restarts");
#elif CAM_FAULT == 10
    bring_up(&cfg);
    fx1_check(cam_isp_take(12, 0x100) == 1u, 0x101, "stop_budget: frame 1");
    fx1_write32(FX1_I2C0_BASE + 0x10u, 0); /* I2C CTRL.enablehost = 0 */
    static const uint32_t budget[3] = {100u, 100u, 1000u};
    static const int want_state[3] = {3, 3, 0};
    for (uint32_t k = 0; k < 3u; ++k) {
        if (k == 2u) fx1_write32(FX1_I2C0_BASE + 0x10u, 1); /* I2C back */
        const uint64_t t = fx1_mtime();
        const int rc = fx1_camera_stop(&g_cam, budget[k]);
        const uint32_t el = us_since(t);
        fx1_log_hex("  stop budget us", budget[k]);
        fx1_log_hex("  stop rc", (uint32_t)rc);
        fx1_log_hex("  stop elapsed us", el);
        fx1_check(rc == (k < 2u ? FX1_CAMERA_ESENSOR : FX1_CAMERA_OK) && g_cam.state == want_state[k], 0x110 + k,
                  "stop_budget: result and state");
        fx1_check(el <= budget[k] + FX1_CAMERA_STOP_SLACK_US, 0x118 + k, "stop_budget: within budget + slack");
        fx1_check(g_cam.side_state[0] == 0 && g_cam.side_state[1] == 0, 0x11C, "stop_budget: sides with the CSI-2 DMA");
    }
    int on = 1;
    fx1_check(fx1_ccs_stream_state(&g_cam.sensor, &on) == FX1_CCS_OK && !on, 0x120, "stop_budget: sensor in standby");
    /* Read-back cut: time one standby write (harmless, the sensor is in
     * standby), then a stop budget that fits the write but not the read-back. */
    const uint64_t tw = fx1_mtime();
    fx1_check(fx1_ccs_stream(&g_cam.sensor, 0) == FX1_CCS_OK, 0x121, "stop_budget: standby write timed");
    const uint32_t w = us_since(tw);
    fx1_log_hex("  standby write us", w);
    wait_us(2u * CAM_ISP_FRAME_US);
    restart_exact(&cfg, 0x140);
    uint64_t t = fx1_mtime();
    int rc = fx1_camera_stop(&g_cam, w + 30u);
    uint32_t el = us_since(t);
    fx1_log_hex("  read-back cut: stop rc", (uint32_t)rc);
    fx1_log_hex("  read-back cut: stop elapsed us", el);
    fx1_check(rc == FX1_CAMERA_ESENSOR && g_cam.state == 3 && el <= w + 30u + FX1_CAMERA_STOP_SLACK_US, 0x150,
              "stop_budget: no time left for the read-back -> ESENSOR within the bound");
    on = 1;
    fx1_check(fx1_ccs_stream_state(&g_cam.sensor, &on) == FX1_CCS_OK && !on, 0x151,
              "stop_budget: the write itself reached the sensor");
    rc = fx1_camera_stop(&g_cam, 1000u);
    fx1_check(rc == FX1_CAMERA_OK && g_cam.state == 0, 0x152, "stop_budget: stop again from state 3 -> OK");
    wait_us(2u * CAM_ISP_FRAME_US);
    restart_exact(&cfg, 0x160);
    fx1_log("stop_budget: the stop deadline bounds every sensor transfer, also in state 3");
#elif CAM_FAULT == 11
    fx1_global_irq_disable();
    fx1_check(fx1_camera_probe(&g_cam, fx1_csi2_hal_mmio(FX1_MIPI_CSR_BASE), fx1_isp_hal_mmio(), 0) == FX1_CAMERA_OK,
              0x10, "start_fail: probe");
    fx1_check(fx1_camera_configure(&g_cam, &cfg) == FX1_CAMERA_OK, 0x11, "start_fail: configured");
    cam_isp_irqs();
    fx1_write32(FX1_I2C0_BASE + 0x10u, 0); /* I2C CTRL.enablehost = 0: the stream-on write is lost */
    int rc = fx1_camera_start(&g_cam);
    fx1_log_hex("  start rc", (uint32_t)rc);
    fx1_check(rc == FX1_CAMERA_EDEV && g_cam.state == 1, 0x100, "start_fail: EDEV, still configured");
    fx1_write32(FX1_I2C0_BASE + 0x10u, 1);
    const uint64_t t = fx1_mtime();
    rc = fx1_camera_stop(&g_cam, STOP_US);
    fx1_log_hex("  stop rc", (uint32_t)rc);
    fx1_log_hex("  stop us", us_since(t));
    fx1_check(rc == FX1_CAMERA_OK && g_cam.state == 0 && g_cam.side_state[0] == 0 && g_cam.side_state[1] == 0, 0x110,
              "start_fail: stop from the configured state -> idle");
    int on = 1;
    fx1_check(fx1_ccs_stream_state(&g_cam.sensor, &on) == FX1_CCS_OK && !on, 0x111, "start_fail: sensor in standby");
    const uint8_t c0 = sensor_frames();
    wait_us(2u * CAM_ISP_FRAME_US);
    fx1_check(sensor_frames() == c0, 0x112, "start_fail: the sensor does not stream");
    restart_exact(&cfg, 0x140);
    fx1_log("start_fail: a start that failed half-way is cleaned up by stop");
#elif CAM_FAULT == 9
    bring_up(&cfg);
    fx1_camera_frame held[4];
    for (uint32_t k = 0; k < 4; ++k) {
        fx1_check(fx1_camera_dequeue(&g_cam, &held[k], CAM_ISP_WAIT_US) == FX1_CAMERA_OK, 0x100 + k, "stop_held: frame");
        fx1_check(cam_isp_frame_of(12, cam_isp_crc(&held[k], CAM_ISP_W, CAM_ISP_W)) == k + 1u, 0x110 + k,
                  "stop_held: frames 1-4");
    }
    const uint64_t t0 = fx1_mtime();
    while (us_since(t0) < 3u * CAM_ISP_FRAME_US) fx1_camera_poll(&g_cam); /* both sides wait for the ISP */
    fx1_check(g_cam.side_state[0] == 1 && g_cam.side_state[1] == 1, 0x120, "stop_held: two captures waiting");
    const uint64_t t = fx1_mtime();
    const int rc = fx1_camera_stop(&g_cam, STOP_US);
    const uint32_t el = us_since(t);
    fx1_log_hex("  stop us", el);
    fx1_log_hex("  cancelled", g_cam.stats.stop_cancelled);
    fx1_check(rc == FX1_CAMERA_OK && g_cam.state == 0 && el <= STOP_US, 0x121, "stop_held: stop completes");
    fx1_check(g_cam.stats.stop_cancelled >= 2u && g_cam.side_state[0] == 0 && g_cam.side_state[1] == 0, 0x122,
              "stop_held: waiting captures cancelled, sides back");
    for (uint32_t k = 0; k < 4; ++k) {
        fx1_check(cam_isp_frame_of(12, cam_isp_crc(&held[k], CAM_ISP_W, CAM_ISP_W)) == k + 1u, 0x130 + k,
                  "stop_held: held NV12 still readable and exact");
        fx1_check(fx1_camera_release(&g_cam, &held[k]) == FX1_CAMERA_OK, 0x138 + k, "stop_held: release after stop");
    }
    fx1_check(fx1_camera_release(&g_cam, &held[0]) == FX1_CAMERA_EINVAL, 0x13F, "stop_held: second release refused");
    restart_exact(&cfg, 0x140);
    fx1_log("stop_held: stop cancels the pipeline, held outputs stay with the application");
#else
#error unknown CAM_FAULT
#endif
    const uint64_t t1 = fx1_mtime();
    fx1_check(fx1_camera_stop(&g_cam, CAM_ISP_WAIT_US * 2u) == FX1_CAMERA_OK, 0x200, "faults: stop");
    fx1_log_hex("  stop us", us_since(t1));
    fx1_check(g_cam.side_state[0] == 0 && g_cam.side_state[1] == 0, 0x201, "faults: both CSI-2 sides back");
    return 0;
}
