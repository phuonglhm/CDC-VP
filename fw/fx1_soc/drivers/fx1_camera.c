#include "fx1_camera.h"

#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_fw.h"
#include "fx1_isp/fx1_isp_csr.h"

#define CSI_BUF_DONE_OK (1u << 16)
#define CSI_BUF_DONE_ERR (1u << 17)
#define CSI_FRAME_DROPPED (1u << 14)
#define CSI_AXI_WRITE_ERROR (1u << 18)
#define ISP_FATAL (FX1_ISP_DMA_ERR_ERR_ALIGN_OR_GEOMETRY_BIT | FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT | \
                   FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT)

enum { SIDE_HW = 0, SIDE_WAIT = 1, SIDE_ISP = 2 };

static uint64_t ticks(uint32_t us) { return (uint64_t)us * (FX1_MTIME_HZ / 1000000u); }

static uint32_t isp_rd(fx1_camera* cam, uint32_t off) { return cam->isp.hal.read32(cam->isp.hal.ctx, off); }
static void isp_wr(fx1_camera* cam, uint32_t off, uint32_t v) { cam->isp.hal.write32(cam->isp.hal.ctx, off, v); }
static void isp_wr64(fx1_camera* cam, uint32_t lo, uint64_t v)
{
    isp_wr(cam, lo, (uint32_t)v);
    isp_wr(cam, lo + 4u, (uint32_t)(v >> 32));
}

int fx1_camera_probe(fx1_camera* cam, const fx1_csi2_hal* csi_hal, const fx1_isp_hal* isp_hal, int tpg)
{
    if (!cam || !csi_hal || !isp_hal) return FX1_CAMERA_EINVAL;
    for (size_t i = 0; i < sizeof *cam; ++i) ((volatile uint8_t*)cam)[i] = 0;
    if (fx1_csi2_probe(&cam->csi, csi_hal) != FX1_CSI2_OK) return FX1_CAMERA_EDEV;
    if (fx1_isp_init(&cam->isp, isp_hal) != FX1_ISP_OK) return FX1_CAMERA_EDEV;
    if (!tpg) {
        if (fx1_i2c_open(&cam->bus, FX1_I2C0_BASE, 0, FX1_IRQ_I2C0, FX1_PLIC_CONTEXT_HART_M(0)) != FX1_I2C_OK)
            return FX1_CAMERA_EDEV;
        if (fx1_ccs_probe(&cam->sensor, &cam->bus, FX1_CCS_ADDR, 2000) != FX1_CCS_OK) return FX1_CAMERA_EDEV;
    }
    cam->state = 0;
    return FX1_CAMERA_OK;
}

int fx1_camera_configure(fx1_camera* cam, const fx1_camera_cfg* c)
{
    if (cam && cam->state != 0) return FX1_CAMERA_ESTATE;
    if (!cam || !c || (c->bits != 10 && c->bits != 12) || !c->width || !c->height || (c->width & 7u) ||
        (c->height & 1u) || c->csi_side_size < 2u * c->width * c->height || !c->profile)
        return FX1_CAMERA_EINVAL;
    cam->cfg = *c;
    if (!c->tpg) {
        const fx1_ccs_mode m = {(uint16_t)c->width, (uint16_t)c->height, c->line_length, c->frame_length,
                                (uint8_t)c->bits, (uint8_t)c->lanes};
        if (fx1_ccs_set_mode(&cam->sensor, &m) != FX1_CCS_OK) return FX1_CAMERA_EDEV;
        if (fx1_csi2_phy_start(&cam->csi, c->lanes, 2000) != FX1_CSI2_OK) return FX1_CAMERA_EDEV;
    }
    if (fx1_csi2_stream_config(&cam->csi, 0, 0) != FX1_CSI2_OK) return FX1_CAMERA_EDEV;
    fx1_write32(FX1_MIPI_CSR_BASE + FX1_CSI2_EXPECTED_RESOLUTION, (c->height << 16) | c->width);
    const fx1_csi2_dma_cfg d = {{c->csi_side[0], c->csi_side[1]}, c->csi_side_size, (uint16_t)(2u * c->width), 0u,
                                255u, 0u, 0u, 0u, 1u /* RAW16 */, 1u /* release with error */};
    if (fx1_csi2_dma_config(&cam->csi, &d) != FX1_CSI2_OK) return FX1_CAMERA_EDEV;
    if (fx1_csi2_irq_enable(&cam->csi, CSI_BUF_DONE_OK | CSI_BUF_DONE_ERR | CSI_FRAME_DROPPED | CSI_AXI_WRITE_ERROR) !=
        FX1_CSI2_OK)
        return FX1_CAMERA_EDEV;
    /* ISP: RGGB (no phase crop), profile, buffers. RAW12 inputs are the CSI-2
     * sides (rewritten per frame); RAW10 inputs are the staging buffers. */
    if (fx1_isp_set_geometry(&cam->isp, c->width, c->height, 0 /* RGGB */) != FX1_ISP_OK) return FX1_CAMERA_EDEV;
    if (fx1_isp_apply(&cam->isp, c->profile, c->profile_len) != FX1_ISP_OK) return FX1_CAMERA_EDEV;
    for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i) {
        cam->bufs.in[i] = c->bits == 12 ? c->csi_side[i & 1u] : c->staging[i];
        cam->bufs.y[i] = c->y[i];
        cam->bufs.uv[i] = c->uv[i];
        cam->out_owned[i] = 0;
        fx1_dma_prepare((const volatile void*)(uintptr_t)c->y[i], c->y_stride * c->height, FX1_DMA_FROM_DEVICE);
        fx1_dma_prepare((const volatile void*)(uintptr_t)c->uv[i], c->uv_stride * c->height / 2u, FX1_DMA_FROM_DEVICE);
    }
    cam->bufs.in_stride = 2u * c->width;
    cam->bufs.y_stride = c->y_stride;
    cam->bufs.uv_stride = c->uv_stride;
    cam->bufs.max_burst_beats = 16;
    if (fx1_isp_start(&cam->isp, &cam->bufs) != FX1_ISP_OK) return FX1_CAMERA_EDEV;
    cam->side_state[0] = cam->side_state[1] = SIDE_HW;
    cam->ready_head = cam->ready_count = 0;
    cam->isp_stop_pending = 0;
    cam->state = 1;
    return FX1_CAMERA_OK;
}

int fx1_camera_start(fx1_camera* cam)
{
    if (!cam || cam->state != 1 || cam->isp_stop_pending) return FX1_CAMERA_ESTATE;
    if (fx1_csi2_dma_start(&cam->csi) != FX1_CSI2_OK) return FX1_CAMERA_EDEV;
    if (!cam->cfg.tpg && fx1_ccs_stream(&cam->sensor, 1) != FX1_CCS_OK) return FX1_CAMERA_EDEV;
    cam->state = 2;
    return FX1_CAMERA_OK;
}

int fx1_camera_tpg_frame(fx1_camera* cam)
{
    if (!cam || cam->state != 2 || !cam->cfg.tpg) return FX1_CAMERA_ESTATE;
    fx1_write32(FX1_MIPI_CSR_BASE + FX1_CSI2_TPG_RESOLUTION, (cam->cfg.height << 16) | cam->cfg.width);
    const uint32_t fmt = cam->cfg.bits == 10 ? FX1_CSI2_TPG_CONTROL_TPG_PIXEL_FORMAT_MASK : 0u;
    fx1_write32(FX1_MIPI_CSR_BASE + FX1_CSI2_TPG_CONTROL, FX1_CSI2_TPG_CONTROL_TPG_ENABLE_MASK | fmt);
    fx1_write32(FX1_MIPI_CSR_BASE + FX1_CSI2_TPG_CONTROL, fmt); /* the frame in progress completes */
    return FX1_CAMERA_OK;
}

void fx1_camera_csi_isr(fx1_camera* cam)
{
    cam->csi_events |= fx1_csi2_irq_handle(&cam->csi);
    ++cam->stats.csi_irqs;
}

static void release_side(fx1_camera* cam, unsigned s)
{
    fx1_csi2_buffer_release(&cam->csi, s);
    cam->side_state[s] = SIDE_HW;
}

/* Zero-copy sides whose ISP input slot the IDMA has read go back to the
 * CSI-2 DMA. Called with interrupts masked (ISR, or poll in a critical
 * section): the CSI-2 DMA gets the side as soon as possible, whatever the
 * application is doing, so a slow consumer does not hold both sides.
 * A clear VALID bit alone is not a successful read: an AXI error starts the
 * ISP's automatic recovery (MAS v1.07 §9.9.8), which clears every VALID bit
 * and aborts the frames reading them (review M6a F1). Those frames are lost
 * (fx1_isp_next_lost) and their sides may go back too, but only once
 * fx1_isp_recover_ex has settled the recovery: VALID is read first, then
 * DMA_ERR, so an error raised in between is seen; while a read error is
 * pending (in the hardware or in the ISR's snapshot) no side is released. */
static void release_read_sides(fx1_camera* cam)
{
    const uint32_t valid = isp_rd(cam, FX1_ISP_IDMA_BUF_VALID_OFFSET);
    if ((isp_rd(cam, FX1_ISP_DMA_ERR_OFFSET) | cam->isp_fatal) & FX1_ISP_DMA_ERR_ERR_AXI_RD_BIT) return;
    for (unsigned s = 0; s < 2; ++s)
        if (cam->side_state[s] == SIDE_ISP && !(valid & (1u << cam->side_input[s]))) release_side(cam, s);
}

void fx1_camera_isp_isr(fx1_camera* cam)
{
    fx1_isp_events ev;
    fx1_isp_irq(&cam->isp, &ev); /* acknowledges at the ISP */
    if (ev.dma_err & ISP_FATAL) cam->isp_fatal |= ev.dma_err & ISP_FATAL;
    ++cam->stats.isp_irqs;
    release_read_sides(cam);
}

/* 14-bit FRAME_ID order: a before b. */
static int older(uint16_t a, uint16_t b) { return ((uint16_t)(b - a) & 0x3FFFu) < 0x2000u && a != b; }

/* Hands the oldest waiting side to the ISP if the ISP can take it now. */
static int hand_over(fx1_camera* cam)
{
    int s = -1;
    for (unsigned k = 0; k < 2; ++k)
        if (cam->side_state[k] == SIDE_WAIT &&
            (s < 0 || older(cam->side_frame_id[k], cam->side_frame_id[(unsigned)s])))
            s = (int)k;
    if (s < 0) return 0;
    const uint32_t saved = fx1_irq_save();
    const int ready = fx1_isp_input_ready(&cam->isp) && !cam->out_owned[cam->isp.out_next];
    const unsigned input = fx1_isp_next_input(&cam->isp);
    fx1_irq_restore(saved);
    if (!ready) {
        ++cam->stats.held_for_isp;
        return 0;
    }
    const unsigned side = (unsigned)s;
    const uint32_t src = cam->cfg.csi_side[side];
    const uint32_t words = cam->cfg.width * cam->cfg.height / 2u;
    fx1_camera_frame f = {cam->side_capture[side], side, cam->side_frame_id[side], 0, input, 0, 0, 0, 0, 0,
                          cam->cfg.width, cam->cfg.height};
    fx1_dma_complete((const volatile void*)(uintptr_t)src, words * 4u, FX1_DMA_FROM_DEVICE);
    if (cam->cfg.capture_hook) cam->cfg.capture_hook(cam->cfg.hook_ctx, src, f.capture, f.csi_frame_id);
    if (cam->cfg.bits == 12) { /* zero-copy: the ISP reads the side */
        f.zero_copy = 1;
        f.isp_in = src;
        isp_wr64(cam, FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET + 8u * input, src);
    } else { /* RAW10 -> ISP container bits [11:2]: each 16-bit sample << 2 */
        const volatile uint32_t* in = (const volatile uint32_t*)(uintptr_t)src;
        volatile uint32_t* out = (volatile uint32_t*)(uintptr_t)cam->cfg.staging[input];
        for (uint32_t i = 0; i < words; ++i) out[i] = in[i] << 2; /* samples <= 0x3FF: no carry between halves */
        f.isp_in = cam->cfg.staging[input];
        fx1_dma_prepare((const volatile void*)(uintptr_t)f.isp_in, words * 4u, FX1_DMA_TO_DEVICE);
        release_side(cam, side); /* copied: the CSI-2 DMA may use it again */
    }
    fx1_mb();
    uint32_t seq = 0;
    const uint32_t saved2 = fx1_irq_save();
    const int rc = fx1_isp_queue_frame_meta(&cam->isp, f.capture, f.csi_frame_id, &seq);
    if (rc == FX1_ISP_OK) {
        f.isp_seq = seq;
        cam->rec[seq % 16u] = f;
        ++cam->stats.isp_queued;
        if (f.zero_copy) { /* from here the ISR may release it */
            cam->side_input[side] = input;
            cam->side_state[side] = SIDE_ISP;
        }
    } else {
        /* EAGAIN: a recovery restarted the rotation after the input slot was
         * chosen (MAS v1.07 §9.9.8); the capture is dropped and counted. */
        if (rc == FX1_ISP_EAGAIN) ++cam->stats.isp_lost;
        if (f.zero_copy) release_side(cam, side);
    }
    fx1_irq_restore(saved2);
    return rc == FX1_ISP_OK;
}

unsigned fx1_camera_poll(fx1_camera* cam)
{
    if (!cam || (cam->state != 1 && cam->state != 2) || cam->isp_stop_pending) return 0; /* stopping */
    uint32_t saved = fx1_irq_save();
    const uint32_t ev = cam->csi_events;
    const uint32_t fatal = cam->isp_fatal;
    cam->csi_events = 0;
    cam->isp_fatal = 0;
    /* ISP DMA errors: the ISP recovered by itself (every frame in flight
     * aborted, rotations at 0); the driver settles, losses are counted below. */
    if (fatal) {
        if (cam->cfg.isp_error_hook) cam->cfg.isp_error_hook(cam->cfg.hook_ctx, fatal);
        fx1_isp_recovery r;
        fx1_isp_recover_ex(&cam->isp, fatal, &r);
        ++cam->stats.isp_errors;
    }
    uint32_t lost_seq;
    unsigned lost_input;
    while (fx1_isp_next_lost(&cam->isp, &lost_seq, &lost_input) == FX1_ISP_OK) ++cam->stats.isp_lost;
    release_read_sides(cam);
    fx1_irq_restore(saved);
    (void)ev;
    /* Sides the CSI-2 DMA completed (side_state is shared with the ISR). */
    for (unsigned s = 0; s < 2; ++s) {
        saved = fx1_irq_save();
        const int hw = cam->side_state[s] == SIDE_HW;
        fx1_irq_restore(saved);
        if (!hw) continue;
        fx1_csi2_buf_info bi;
        if (fx1_csi2_get_buffer(&cam->csi, s, &bi) != FX1_CSI2_OK) continue;
        if (bi.state == FX1_CSI2_BUF_SW_OWNED_ERROR) { /* never to the ISP */
            ++cam->stats.csi_err;
            release_side(cam, s);
        } else if (bi.state == FX1_CSI2_BUF_SW_OWNED_VALID) {
            ++cam->stats.csi_ok;
            cam->side_state[s] = SIDE_WAIT;
            cam->side_capture[s] = ++cam->capture_seq;
            cam->side_frame_id[s] = bi.frame_id;
        }
    }
    while (hand_over(cam)) {
    }
    /* Completed outputs, in queue order. */
    for (;;) {
        unsigned index = 0;
        uint32_t seq = 0;
        saved = fx1_irq_save();
        const int got = cam->ready_count < FX1_ISP_NUM_BUFFERS ? fx1_isp_next_done_ex(&cam->isp, &index, &seq)
                                                                : FX1_ISP_ENODATA;
        fx1_irq_restore(saved);
        if (got != FX1_ISP_OK) break;
        fx1_camera_frame f = cam->rec[seq % 16u];
        f.isp_output = index;
        f.y = cam->bufs.y[index];
        f.uv = cam->bufs.uv[index];
        f.width = cam->isp.out_width;
        f.height = cam->isp.out_height;
        cam->out_owned[index] = f.capture;
        cam->ready[(cam->ready_head + cam->ready_count) % FX1_ISP_NUM_BUFFERS] = f;
        ++cam->ready_count;
        ++cam->stats.isp_done;
    }
    return cam->ready_count;
}

int fx1_camera_dequeue(fx1_camera* cam, fx1_camera_frame* frame, uint32_t timeout_us)
{
    if (!cam || !frame || (cam->state != 1 && cam->state != 2)) return FX1_CAMERA_ESTATE;
    const uint64_t start = fx1_mtime();
    while (!fx1_camera_poll(cam)) {
        if (fx1_mtime() - start > ticks(timeout_us)) {
            ++cam->stats.timeouts;
            return FX1_CAMERA_ETIMEOUT;
        }
    }
    *frame = cam->ready[cam->ready_head];
    cam->ready_head = (cam->ready_head + 1u) % FX1_ISP_NUM_BUFFERS;
    --cam->ready_count;
    fx1_dma_complete((const volatile void*)(uintptr_t)frame->y, cam->cfg.y_stride * frame->height,
                     FX1_DMA_FROM_DEVICE);
    fx1_dma_complete((const volatile void*)(uintptr_t)frame->uv, cam->cfg.uv_stride * frame->height / 2u,
                     FX1_DMA_FROM_DEVICE);
    return FX1_CAMERA_OK;
}

int fx1_camera_release(fx1_camera* cam, const fx1_camera_frame* frame)
{
    if (!cam || !frame || frame->isp_output >= FX1_ISP_NUM_BUFFERS || !frame->capture ||
        cam->out_owned[frame->isp_output] != frame->capture)
        return FX1_CAMERA_EINVAL;
    fx1_dma_prepare((const volatile void*)(uintptr_t)frame->y, cam->cfg.y_stride * frame->height, FX1_DMA_FROM_DEVICE);
    fx1_dma_prepare((const volatile void*)(uintptr_t)frame->uv, cam->cfg.uv_stride * frame->height / 2u,
                    FX1_DMA_FROM_DEVICE);
    cam->out_owned[frame->isp_output] = 0;
    return FX1_CAMERA_OK;
}

void fx1_camera_set_output(fx1_camera* cam, unsigned slot, uint32_t y, uint32_t uv)
{
    if (!cam || slot >= FX1_ISP_NUM_BUFFERS) return;
    cam->bufs.y[slot] = y;
    cam->bufs.uv[slot] = uv;
    isp_wr64(cam, FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u * slot, y);
    isp_wr64(cam, FX1_ISP_ODMA_UV_ADDR_L0_OFFSET + 16u * slot, uv);
}

static uint32_t left_us(uint64_t deadline)
{
    const uint64_t now = fx1_mtime();
    return now >= deadline ? 0u : (uint32_t)((deadline - now) / (FX1_MTIME_HZ / 1000000u));
}

/* Budget of the next sensor transfer: what is left of the deadline, at most
 * the sensor's own timeout (a sensor that does not answer fails as early as
 * in any other call); 0: no time left, the transfer is not started. */
static uint32_t transfer_us(const fx1_camera* cam, uint64_t deadline)
{
    const uint32_t left = left_us(deadline);
    return left < cam->sensor.timeout_us ? left : cam->sensor.timeout_us;
}

/* mode_select = 0, read back: the sensor stops at the end of its frame in
 * progress (P12). An I2C error, a read-back of 1 or a deadline that leaves no
 * time for the next transfer is not a standby. Each transfer gets
 * transfer_us() through a copy of the sensor context (its default budget
 * stays for the other calls; re-review M6a R1). */
static int sensor_standby(fx1_camera* cam, uint64_t deadline)
{
    fx1_ccs s = cam->sensor;
    int on = 1;
    s.timeout_us = transfer_us(cam, deadline);
    if (s.timeout_us && fx1_ccs_stream(&s, 0) == FX1_CCS_OK) {
        s.timeout_us = transfer_us(cam, deadline);
        if (s.timeout_us && fx1_ccs_stream_state(&s, &on) == FX1_CCS_OK && !on) return FX1_CAMERA_OK;
    }
    ++cam->stats.sensor_errors;
    return FX1_CAMERA_ESENSOR;
}

int fx1_camera_stop(fx1_camera* cam, uint32_t timeout_us)
{
    if (!cam) return FX1_CAMERA_EINVAL;
    const uint64_t deadline = fx1_mtime() + ticks(timeout_us); /* one deadline for every step (review M6a F2, R1) */
    if (cam->state == 3) { /* CSI-2 RX and ISP already stopped: retry the standby only */
        if (sensor_standby(cam, deadline) != FX1_CAMERA_OK) return FX1_CAMERA_ESENSOR;
        cam->state = 0;
        return FX1_CAMERA_OK;
    }
    /* State 1: configured (ISP started), possibly a start that failed after
     * the CSI-2 DMA start, or with the stream-on write lost: cleaned up like a
     * running stream (M6b review F1). */
    if (cam->state != 1 && cam->state != 2) return FX1_CAMERA_ESTATE;
    int sensor = cam->stop_sensor_rc;
    if (!cam->isp_stop_pending) { /* a retry skips the steps a previous stop completed */
        sensor = cam->cfg.tpg ? FX1_CAMERA_OK : sensor_standby(cam, deadline);
        /* No new claim; the capture in flight is finalised (the sensor ends its
         * frame even when it did not get the standby), else aborted: the abort
         * gets at least FX1_CAMERA_STOP_ABORT_US, the safe drain is not skipped. */
        if (fx1_csi2_dma_stop(&cam->csi, left_us(deadline)) != FX1_CSI2_OK) {
            uint32_t abort_us = left_us(deadline);
            if (abort_us < FX1_CAMERA_STOP_ABORT_US) abort_us = FX1_CAMERA_STOP_ABORT_US;
            if (fx1_csi2_dma_abort(&cam->csi, abort_us) != FX1_CSI2_OK) {
                ++cam->stats.timeouts;
                return FX1_CAMERA_ETIMEOUT; /* state unchanged (1 or 2): stop may be called again */
            }
        }
    }
    /* Cancel what is left in the pipeline. The CSI-2 DMA no longer writes, so
     * the ISP is stopped (frames in flight discarded) before the sides it may
     * still read go back; masked against the ISP ISR, which returns sides. */
    const uint32_t saved = fx1_irq_save();
    const int dropped = fx1_isp_stop(&cam->isp);
    if (dropped == FX1_ISP_EBUSY) {
        /* The ISP's reset drains the transactions it had accepted, possibly
         * reading a zero-copy side (MAS v1.07 §9.9.8): nothing goes back and
         * nothing is cleared until it has finished (review V107-7..10 R10). */
        cam->isp_stop_pending = 1;
        cam->stop_sensor_rc = sensor;
        fx1_irq_restore(saved);
        return FX1_CAMERA_EBUSY;
    }
    cam->isp_stop_pending = 0;
    uint32_t cancelled = (dropped > 0 ? (uint32_t)dropped : 0u) + cam->ready_count;
    for (unsigned s = 0; s < 2; ++s) {
        fx1_csi2_buf_info bi;
        const int got = fx1_csi2_get_buffer(&cam->csi, s, &bi) == FX1_CSI2_OK;
        if (cam->side_state[s] == SIDE_WAIT ||
            (cam->side_state[s] == SIDE_HW && got && bi.state == FX1_CSI2_BUF_SW_OWNED_VALID))
            ++cancelled; /* captured, never given to the ISP */
        if (got && bi.state != FX1_CSI2_BUF_HW_OWNED) fx1_csi2_buffer_release(&cam->csi, s);
        cam->side_state[s] = SIDE_HW;
    }
    cam->csi_events = 0;
    cam->isp_fatal = 0;
    fx1_irq_restore(saved);
    cam->stats.stop_cancelled += cancelled;
    cam->ready_head = cam->ready_count = 0; /* outputs the application holds stay owned until released */
    cam->state = sensor == FX1_CAMERA_OK ? 0 : 3;
    if (sensor != FX1_CAMERA_OK) return sensor;
    return dropped < 0 ? FX1_CAMERA_EDEV : FX1_CAMERA_OK;
}
