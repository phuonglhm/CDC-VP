/*
 * ISP behaviour of the CSR/MAS revision of 2026-10-07 on the FX1 platform
 * (milestone U6 of the ISP CSR/MAS update), through the reference C99 driver.
 * Every expected value comes from the pinned Python reference (isp_fixture.h).
 *
 *   A  AWB result ownership (MAS §6.22.4) and frame metadata (MAS Table 160).
 *      The stats profile, one frame at a time, 8 frames. Each frame is queued
 *      with fx1_isp_queue_frame_meta (tag, capture time = FRAME_COUNTER) and
 *      collected with fx1_isp_next_done_meta: the tag and timestamp come back
 *      with the buffer, SOF/EOF follow the capture, and the NV12 CRC is
 *      basic's. After given frames the AWB script (isp_fx_awb_script) runs:
 *      report, acquire, release, clear report. Software holding both result
 *      banks makes the next frames drop their AWB measurement (DROP_COUNT,
 *      OVERRUN); released banks are reused. Status, identity and global sums
 *      of every step must equal the reference.
 *   B  An AWB lease across fx1_isp_dma_reset right after two frames are
 *      queued back to back, so at least the second is aborted in progress
 *      (MAS Table 183: the DMA soft reset keeps statistics and AWB
 *      ownership): the leased result reads the same afterwards, nothing
 *      completes after the call, and the next frame is correct.
 *   C  Resizer from the first frame and mode switches (MAS §6.20.4, §9.2):
 *      648x368 RGGB, scaled to 640x360 from the first frame after
 *      fx1_isp_start, then pass-through, then 640x360 again, strides sized
 *      for the largest output. CRCs from the reference. RESIZER_CTRL is an
 *      IDLE-class word in v1.07 (MAS §9.9.3: rejected with TUNE_ERROR.MODE
 *      while the input DMA runs), so each switch stops the stream, writes the
 *      mode and starts again (fx1_isp_start waits for the published set).
 *      The rz profile has no temporal block, so the stop's soft reset does
 *      not change the reference CRCs.
 */
#include "isp_common.h"

static fx1_isp_dev dev;
static volatile uint32_t notified, dma_errors, last_dma_err, isr_done_reports;

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    (void)mtval;
    if (mcause != (FX1_MCAUSE_INTERRUPT | FX1_IRQ_M_EXT)) {
        fx1_log_hex("unexpected mcause", mcause);
        fx1_fail(0x0E0);
    }
    const uint32_t id = fx1_plic_claim(0);
    if (id == FX1_IRQ_ISP) {
        fx1_isp_events ev;
        fx1_isp_irq(&dev, &ev);
        isr_done_reports += ev.frames_done;
        for (unsigned k = 0; k < ev.frames_done && k < dev.in_flight; ++k)
            notified |= 1u << (dev.queue[k].seq & 31u);
        if (ev.dma_err & ISP_DMA_ERR_FATAL) {
            ++dma_errors;
            last_dma_err = ev.dma_err;
        }
    }
    if (id) fx1_plic_complete(0, id);
    return mepc;
}

static void expect_eq(uint32_t got, uint32_t want, uint32_t code, const char* what)
{
    if (got == want) return;
    fx1_log_hex(what, got);
    fx1_log_hex("  expected", want);
    fx1_fail(code);
}

static void wait_us(uint32_t us)
{
    const uint64_t start = fx1_mtime();
    while (fx1_mtime() - start < (uint64_t)us * (FX1_MTIME_HZ / 1000000u)) {
    }
}

/* ---- one frame at a time, with metadata ------------------------------------ */

typedef struct {
    unsigned index;
    fx1_isp_frame_meta meta;
} completed;

static uint32_t queue_tagged(uint32_t tag, uint32_t* ts, uint32_t code)
{
    uint32_t seq = 0;
    *ts = fx1_isp_frame_counter(&dev); /* capture time in the IP time base */
    fx1_check(fx1_isp_queue_frame_meta(&dev, tag, *ts, &seq) == FX1_ISP_OK, code, "frame queued with metadata");
    return seq;
}

static completed wait_done(uint32_t seq, uint32_t tag, uint32_t ts, uint32_t code)
{
    const uint64_t start = fx1_mtime();
    while (!(notified & (1u << (seq & 31u)))) {
        fx1_check(fx1_mtime() - start < FX1_MTIME_HZ / 2u, code, "completion within 500 ms");
        if (dma_errors) {
            fx1_log_hex("ISP DMA_ERR", last_dma_err);
            fx1_fail(code + 1);
        }
    }
    notified &= ~(1u << (seq & 31u));
    completed c;
    uint32_t done_seq = 0;
    const uint32_t saved = fx1_irq_save();
    const int got = fx1_isp_next_done_meta(&dev, &c.index, &done_seq, &c.meta);
    const uint32_t now = fx1_isp_frame_counter(&dev);
    fx1_irq_restore(saved);
    fx1_check(got == FX1_ISP_OK && done_seq == seq, code + 2, "the signalled frame is the one consumed");
    expect_eq(c.meta.tag, tag, code + 3, "ODMA_BUF_TAG");
    expect_eq(c.meta.ts, ts, code + 4, "ODMA_BUF_TS");
    /* Unsigned 32-bit spans (modulo 2^32): capture < SOF < EOF < now. */
    fx1_check(c.meta.sof_ts - ts > 0u && c.meta.sof_ts - ts < 0x80000000u, code + 5, "SOF_TS after the capture");
    fx1_check(c.meta.eof_ts - c.meta.sof_ts > 0u && c.meta.eof_ts - c.meta.sof_ts < 0x80000000u, code + 6,
              "EOF_TS after SOF_TS");
    fx1_check(now - c.meta.eof_ts > 0u && now - c.meta.eof_ts < 0x80000000u, code + 7, "published after EOF_TS");
    return c;
}

/* ---- A: AWB ownership script ------------------------------------------------ */

static void awb_step(const isp_fx_awb_step* s, uint32_t code)
{
    fx1_isp_awb_status st;
    fx1_isp_awb_lease lease;
    fx1_isp_awb_global g;
    switch (s->op) {
    case 0:
        fx1_check(fx1_isp_awb_report(&dev, &st, 0) == FX1_ISP_OK, code, "AWB report");
        expect_eq(st.ready_count, s->ready, code + 1, "AWB READY count");
        expect_eq((uint32_t)st.leased, s->leased, code + 2, "AWB leased");
        expect_eq((uint32_t)st.overrun, s->overrun, code + 3, "AWB OVERRUN");
        expect_eq(st.drop_count, s->drops, code + 4, "AWB_DROP_COUNT");
        break;
    case 1:
        fx1_check(fx1_isp_awb_acquire(&dev, &lease) == FX1_ISP_OK, code + 5, "AWB acquire: a result");
        fx1_check(fx1_isp_awb_read_global(&dev, &g) == FX1_ISP_OK, code + 6, "AWB global read under the lease");
        expect_eq(lease.frame_id, s->fid, code + 7, "AWB_FRAME_ID of the oldest READY result");
        expect_eq(lease.context_id, s->ctx, code + 8, "AWB_RESULT_CONTEXT_ID");
        for (unsigned c = 0; c < 3; ++c) {
            expect_eq((uint32_t)g.sum[c], (uint32_t)s->sum[c], code + 9, "AWB global sum (low)");
            expect_eq((uint32_t)(g.sum[c] >> 32), (uint32_t)(s->sum[c] >> 32), code + 9, "AWB global sum (high)");
        }
        expect_eq(g.count, s->count, code + 10, "AWB_GLOBAL_COUNT");
        break;
    case 2:
        fx1_check(fx1_isp_awb_acquire(&dev, &lease) == FX1_ISP_ENODATA, code + 11, "AWB acquire: nothing READY");
        break;
    case 3:
        fx1_check(fx1_isp_awb_release(&dev) == FX1_ISP_OK, code + 12, "AWB release");
        break;
    default:
        fx1_check(fx1_isp_awb_report(&dev, &st, 1) == FX1_ISP_OK, code + 13, "AWB clear report");
        break;
    }
}

static void phase_a(const fx1_isp_buffers* bufs)
{
    unsigned step = 0;
    for (uint32_t k = 0; k < ISP_FX_FRAMES; ++k) {
        uint32_t saved = fx1_irq_save();
        fx1_check(fx1_isp_input_ready(&dev), 0x10, "A: input ready");
        isp_fill_input(bufs->in[fx1_isp_next_input(&dev)], k);
        uint32_t ts;
        const uint32_t tag = 0xA0000000u | (k << 8) | 0x5Au;
        const uint32_t seq = queue_tagged(tag, &ts, 0x11);
        fx1_irq_restore(saved);
        const completed c = wait_done(seq, tag, ts, 0x12);
        isp_output_to_cpu(bufs->y[c.index], bufs->uv[c.index]);
        expect_eq(isp_nv12_crc(bufs->y[c.index], bufs->uv[c.index]), isp_fx_crc_basic[k], 0x20 + k, "A: NV12 CRC");
        isp_output_to_device(bufs->y[c.index], bufs->uv[c.index]);
        for (; step < ISP_FX_AWB_STEPS && isp_fx_awb_script[step].frame == k; ++step) {
            saved = fx1_irq_save();
            awb_step(&isp_fx_awb_script[step], 0x100 + 0x10u * step);
            fx1_irq_restore(saved);
        }
    }
    fx1_check(step == ISP_FX_AWB_STEPS, 0x30, "A: every AWB step ran");
    fx1_log("A: metadata with every buffer; AWB lease, drops and bank reuse match the reference");
}

/* ---- B: AWB lease across a DMA soft reset ------------------------------------ */

static void phase_b(const fx1_isp_buffers* bufs)
{
    /* One more frame: its AWB result is the one held across the reset. */
    uint32_t saved = fx1_irq_save();
    isp_fill_input(bufs->in[fx1_isp_next_input(&dev)], 0);
    uint32_t ts;
    uint32_t seq = queue_tagged(0xB0000001u, &ts, 0x40);
    fx1_irq_restore(saved);
    completed c = wait_done(seq, 0xB0000001u, ts, 0x41);
    isp_output_to_cpu(bufs->y[c.index], bufs->uv[c.index]);
    expect_eq(isp_nv12_crc(bufs->y[c.index], bufs->uv[c.index]), isp_fx_crc_basic[0], 0x49, "B: NV12 CRC");
    isp_output_to_device(bufs->y[c.index], bufs->uv[c.index]);
    fx1_isp_awb_lease lease, again;
    fx1_isp_awb_global before, after;
    saved = fx1_irq_save();
    fx1_check(fx1_isp_awb_acquire(&dev, &lease) == FX1_ISP_OK, 0x4A, "B: acquire");
    fx1_check(fx1_isp_awb_read_global(&dev, &before) == FX1_ISP_OK, 0x4B, "B: read before");
    fx1_irq_restore(saved);
    expect_eq(lease.frame_id, ISP_FX_FRAMES + 1u, 0x4C, "B: the leased result is frame 9's");

    /* Two frames queued back to back, then the DMA soft reset at once: the
     * second cannot complete inside the call (it waits for the first one's
     * input), so at least one frame is aborted in progress. */
    const unsigned in0 = fx1_isp_next_input(&dev);
    isp_fill_input(bufs->in[in0], 1);
    isp_fill_input(bufs->in[(in0 + 1u) % FX1_ISP_NUM_BUFFERS], 2);
    saved = fx1_irq_save();
    const uint32_t odma_before = isp_hal.read32(isp_hal.ctx, FX1_ISP_ODMA_FRAME_COUNT_OFFSET);
    queue_tagged(0xB0000002u, &ts, 0x50);
    queue_tagged(0xB0000004u, &ts, 0x50);
    fx1_check(fx1_isp_dma_reset(&dev) == 2, 0x52, "B: DMA reset drops the frames in flight");
    const uint32_t odma_at = isp_hal.read32(isp_hal.ctx, FX1_ISP_ODMA_FRAME_COUNT_OFFSET);
    fx1_irq_restore(saved);
    fx1_check(odma_at - odma_before <= 1u, 0x51, "B: the second frame was aborted in progress");
    const uint32_t reports = isr_done_reports;
    wait_us(2000);
    saved = fx1_irq_save();
    unsigned index;
    fx1_check(fx1_isp_next_done(&dev, &index) == FX1_ISP_ENODATA, 0x53, "B: no completion after the reset");
    fx1_check(isr_done_reports == reports, 0x54, "B: no DONE reported after the reset");
    expect_eq(isp_hal.read32(isp_hal.ctx, FX1_ISP_ODMA_FRAME_COUNT_OFFSET), odma_at, 0x55, "B: ODMA_FRAME_COUNT");
    fx1_isp_awb_status st;
    fx1_check(fx1_isp_awb_report(&dev, &st, 0) == FX1_ISP_OK && st.leased, 0x56, "B: the lease survives the DMA reset");
    fx1_check(fx1_isp_awb_read_global(&dev, &after) == FX1_ISP_OK, 0x57, "B: read after");
    for (unsigned k = 0; k < 3; ++k)
        fx1_check(after.sum[k] == before.sum[k], 0x58, "B: the leased result is unchanged");
    fx1_check(after.count == before.count && after.frame_id == before.frame_id, 0x59, "B: identity unchanged");
    fx1_check(fx1_isp_awb_release(&dev) == FX1_ISP_OK, 0x5A, "B: release");
    fx1_irq_restore(saved);

    /* The stream continues from buffer 0. */
    saved = fx1_irq_save();
    fx1_check(fx1_isp_next_input(&dev) == 0, 0x5B, "B: rotation restarted");
    isp_fill_input(bufs->in[0], 2);
    seq = queue_tagged(0xB0000003u, &ts, 0x5C);
    fx1_irq_restore(saved);
    c = wait_done(seq, 0xB0000003u, ts, 0x60);
    fx1_check(c.index == 0, 0x68, "B: output buffer 0");
    isp_output_to_cpu(bufs->y[0], bufs->uv[0]);
    expect_eq(isp_nv12_crc(bufs->y[0], bufs->uv[0]), isp_fx_crc_basic[2], 0x69, "B: NV12 CRC after the reset");
    isp_output_to_device(bufs->y[0], bufs->uv[0]);
    saved = fx1_irq_save();
    fx1_check(fx1_isp_awb_acquire(&dev, &again) == FX1_ISP_OK, 0x6A, "B: a new AWB result");
    fx1_check(again.frame_id > lease.frame_id, 0x6B, "B: newer than the leased one");
    fx1_check(fx1_isp_awb_release(&dev) == FX1_ISP_OK, 0x6C, "B: release");
    fx1_irq_restore(saved);
    fx1_log_hex("B: AWB FRAME_ID after the reset", again.frame_id);
    fx1_log("B: DMA reset mid-frame: lease kept, no stale completion, stream restarts at buffer 0");
}

/* ---- C: Resizer from the first frame, mode switch ---------------------------- */

#define RZ_IN_STRIDE  (2u * ISP_FX_RZ_WIDTH)        /* 1296: multiple of the 16-byte beat */
#define RZ_OUT_STRIDE ((ISP_FX_RZ_WIDTH + 15u) & ~15u) /* 656: the largest output, 648 */
#define RZ_SLOT       0x80000u
#define RZ_UV_OFFSET  0x40000u

uint32_t isp_raw_sample_rz(uint32_t frame, uint32_t x, uint32_t y)
{
    uint32_t v = 400u + 160u * frame + ((x * 37u + y * 91u + frame * 53u) & 511u) + ((x ^ y) & 7u) * 24u;
    if (((x + 2u * frame) & 15u) < 3u) v += 900u;
    return v > 4095u ? 4095u : v;
}

static void rz_fill(uint64_t base, uint32_t frame)
{
    for (uint32_t y = 0; y < ISP_FX_RZ_HEIGHT; ++y) {
        volatile uint32_t* line = (volatile uint32_t*)(uintptr_t)(base + y * RZ_IN_STRIDE);
        for (uint32_t x = 0; x < ISP_FX_RZ_WIDTH; x += 2)
            line[x / 2u] = isp_raw_sample_rz(frame, x, y) | (isp_raw_sample_rz(frame, x + 1u, y) << 16);
    }
    fx1_dma_prepare((const volatile void*)(uintptr_t)base, RZ_IN_STRIDE * ISP_FX_RZ_HEIGHT, FX1_DMA_TO_DEVICE);
}

static void rz_to_device(uint64_t y)
{
    fx1_dma_prepare((const volatile void*)(uintptr_t)y, RZ_OUT_STRIDE * ISP_FX_RZ_HEIGHT, FX1_DMA_FROM_DEVICE);
    fx1_dma_prepare((const volatile void*)(uintptr_t)(y + RZ_UV_OFFSET), RZ_OUT_STRIDE * ISP_FX_RZ_HEIGHT / 2u,
                    FX1_DMA_FROM_DEVICE);
}

static uint32_t crc32_bytes(uint32_t crc, const volatile uint8_t* p, uint32_t n)
{
    for (uint32_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (int k = 0; k < 8; ++k) crc = (crc & 1u) ? 0xEDB88320u ^ (crc >> 1) : crc >> 1;
    }
    return crc;
}

static uint32_t rz_crc(uint64_t y, uint32_t w, uint32_t h)
{
    fx1_dma_complete((const volatile void*)(uintptr_t)y, RZ_OUT_STRIDE * h, FX1_DMA_FROM_DEVICE);
    fx1_dma_complete((const volatile void*)(uintptr_t)(y + RZ_UV_OFFSET), RZ_OUT_STRIDE * h / 2u, FX1_DMA_FROM_DEVICE);
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t r = 0; r < h; ++r) crc = crc32_bytes(crc, (const volatile uint8_t*)(uintptr_t)(y + r * RZ_OUT_STRIDE), w);
    for (uint32_t r = 0; r < h / 2u; ++r)
        crc = crc32_bytes(crc, (const volatile uint8_t*)(uintptr_t)(y + RZ_UV_OFFSET + r * RZ_OUT_STRIDE), w);
    return crc ^ 0xFFFFFFFFu;
}

static void phase_c(void)
{
    fx1_isp_buffers b;
    for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i) {
        b.in[i] = FX1_FW_RAW_BUF_BASE + i * RZ_SLOT;
        b.y[i] = FX1_FW_YUV_BUF_BASE + i * RZ_SLOT;
        b.uv[i] = b.y[i] + RZ_UV_OFFSET;
        rz_to_device(b.y[i]);
    }
    b.in_stride = RZ_IN_STRIDE;
    b.y_stride = b.uv_stride = RZ_OUT_STRIDE;
    b.max_burst_beats = 16;
    fx1_check(fx1_isp_set_geometry(&dev, ISP_FX_RZ_WIDTH, ISP_FX_RZ_HEIGHT, ISP_FX_BAYER) == FX1_ISP_OK, 0x70,
              "C: geometry 648x368 RGGB");
    fx1_isp_apply(&dev, isp_fx_profile_rz, sizeof(isp_fx_profile_rz) / sizeof(isp_fx_profile_rz[0]));
    const fx1_isp_reg_write scaled = {FX1_ISP_RESIZER_CTRL_OFFSET, isp_fx_rz_mode[0]};
    fx1_isp_apply(&dev, &scaled, 1);
    fx1_check(fx1_isp_start(&dev, &b) == FX1_ISP_OK, 0x71, "C: start (waits for the published Resizer set)");
    expect_eq(dev.out_width, 640u, 0x72, "C: RESIZER_OUT_W at start");
    expect_eq(dev.out_height, 360u, 0x73, "C: RESIZER_OUT_H at start");
    for (uint32_t k = 0; k < ISP_FX_RZ_FRAMES; ++k) {
        if (k > 0) {
            /* Between frames: stop, write the mode while idle, start again. */
            fx1_check(fx1_isp_stop(&dev) == 0, 0x75, "C: stop between modes, nothing in flight");
            const fx1_isp_reg_write mode = {FX1_ISP_RESIZER_CTRL_OFFSET, isp_fx_rz_mode[k]};
            fx1_isp_apply(&dev, &mode, 1);
            fx1_check(fx1_isp_start(&dev, &b) == FX1_ISP_OK, 0x76, "C: start with the new mode");
            expect_eq(dev.out_width, isp_fx_rz_mode[k] ? 640u : ISP_FX_RZ_WIDTH, 0x77, "C: RESIZER_OUT_W");
        }
        const unsigned in = fx1_isp_next_input(&dev);
        rz_fill(b.in[in], k);
        uint32_t ts;
        const uint32_t tag = 0xC0000000u | k;
        uint32_t saved = fx1_irq_save();
        const uint32_t seq = queue_tagged(tag, &ts, 0x74);
        fx1_irq_restore(saved);
        const completed c = wait_done(seq, tag, ts, 0x78);
        const uint32_t w = isp_fx_rz_mode[k] ? 640u : ISP_FX_RZ_WIDTH;
        const uint32_t h = isp_fx_rz_mode[k] ? 360u : ISP_FX_RZ_HEIGHT;
        const uint32_t crc = rz_crc(b.y[c.index], w, h);
        if (crc != isp_fx_crc_rz[k]) {
            fx1_log_hex("C: resize frame", k);
            fx1_log_hex("  RESIZER_CTRL", isp_fx_rz_mode[k]);
            fx1_log_hex("  CRC got     ", crc);
            fx1_log_hex("  CRC expected", isp_fx_crc_rz[k]);
            fx1_fail(0x80 + k);
        }
        rz_to_device(b.y[c.index]);
    }
    fx1_check(fx1_isp_stop(&dev) == 0, 0x88, "C: stop with nothing in flight");
    fx1_log("C: 640x360 from the first frame, then 648x368, then 640x360: CRCs match the reference");
}

int main(void)
{
    fx1_isp_buffers bufs;
    isp_buffers(&bufs);
    /* Phase B aborts frames in flight within this stream (isp_common.h). */
    bufs.max_burst_beats = ISP_ABORT_MAX_BURST_BEATS;
    fx1_check(fx1_isp_init(&dev, &isp_hal) == FX1_ISP_OK, 0x01, "driver init");
    fx1_check(fx1_isp_set_geometry(&dev, ISP_FX_WIDTH, ISP_FX_HEIGHT, ISP_FX_BAYER) == FX1_ISP_OK, 0x02,
              "geometry 64x48 RGGB");
    fx1_isp_apply(&dev, isp_fx_profile_stats, sizeof(isp_fx_profile_stats) / sizeof(isp_fx_profile_stats[0]));

    fx1_set_trap_handler(handler);
    fx1_plic_set_priority(FX1_IRQ_ISP, 1);
    fx1_plic_enable(0, FX1_IRQ_ISP, 1);
    fx1_irq_enable(FX1_MIE_MEIE);
    fx1_global_irq_enable();
    isp_outputs_to_device(&bufs);
    fx1_check(fx1_isp_start(&dev, &bufs) == FX1_ISP_OK, 0x03, "stream start");
    fx1_log("ISP CSR/MAS 07/10: metadata, AWB ownership, DMA reset, Resizer");

    phase_a(&bufs);
    phase_b(&bufs);
    fx1_check(fx1_isp_stop(&dev) == 0, 0x6F, "stop after B");
    phase_c();
    fx1_log("ISP CSR/MAS 07/10 platform checks: PASS");
    return 0;
}
