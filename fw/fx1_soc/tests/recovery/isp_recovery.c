/*
 * G5 / plan P5: ISP error and reset recovery on the platform, driven by the
 * reference driver from firmware. Real bus errors: a buffer address that the
 * FX1 fabric does not decode makes the IDMA/ODMA see DECERR, i.e. an AXI error
 * (ISP programming guide section 8; MAS v1.07 §9.9.8, which superseded DEC-19).
 * An AXI error starts an automatic DMA-scope recovery in the ISP: every
 * admitted, unretired frame is aborted and recorded, every VALID / FREE bit
 * is cleared and both rotations restart at buffer 0.
 *
 *   A  ODMA AXI error. Output buffer 2's Y/UV addresses are unmapped. The
 *      frame that lands there fails; the ISR reports DMA_ERR.ODMA_AXI. The
 *      firmware repairs the buffer's address registers (the hardware cleared
 *      every FREE bit), then fx1_isp_recover_ex() waits for the recovery and
 *      settles the queue: the failed frame and any frame queued behind it
 *      are reported through fx1_isp_next_lost() and the firmware resubmits
 *      them. Every frame 0..7 must complete exactly once with the reference
 *      CRC; one or two losses (the pair partner, if it was behind). Frames are
 *      queued in pairs (both inputs filled first, then queued back to back,
 *      only when nothing is in flight); after an error nothing new is queued
 *      until the frames in flight have drained. The recovery report must name
 *      the failed frame with CH0 failed and cause 2 (write error).
 *   B  IDMA AXI error. Input buffer 2's address is unmapped. Repair, recover:
 *      the frame is lost (no in-place retry since v1.07), resubmitted, all 8
 *      correct; a recorded frame has cause 1 (read error).
 *   C  Soft reset in the middle of a frame (fx1_isp_soft_reset). No
 *      completion or ODMA progress may follow for the aborted frames; the
 *      stream then restarts from buffer 0 and all 8 frames are correct.
 *   D  fx1_isp_stop in the middle of a frame: the frame is discarded,
 *      nothing completes afterwards; fx1_isp_start again, all 8 correct.
 *   E  DMA soft reset in the middle of a stream (fx1_isp_dma_reset, MAS
 *      Table 183): two frames queued back to back, reset at once; at least
 *      the second is aborted, and nothing completes or progresses after the
 *      call; the stream stays enabled and continues from buffer 0 without a
 *      restart; all 8 correct.
 *
 * Every frame is queued with a software tag (fx1_isp_queue_frame_meta) and
 * collected with fx1_isp_next_done_meta: the tag must be the frame's (MAS
 * Table 160). The input and output rotations restart together after every
 * recovery, so each frame completes in the output index of its input.
 *
 * Making the fault transient by repairing the address is a test device: a
 * silicon AXI error has its own cause. The driver sequence (recover, lost
 * frames, resubmission, soft reset, stop/start) is the one firmware uses.
 * Every phase ends with no fatal DMA_ERR bit left, nothing in flight and the
 * ISP interrupt line low.
 */
#include "isp_common.h"

#define UNMAPPED       0x60000000u /* not decoded by the FX1 fabric */
#define MAX_IN_FLIGHT  2u
#define ALL_FRAMES     ((1u << ISP_FX_FRAMES) - 1u)
#define SEQ_SLOTS      64u

static fx1_isp_dev dev;
static volatile uint32_t isr_fatal, isr_done_reports, isr_count;
static uint32_t frame_of_seq[SEQ_SLOTS], tag_of_seq[SEQ_SLOTS], input_of_seq[SEQ_SLOTS];
static uint32_t tags_issued;

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
        fx1_isp_irq(&dev, &ev); /* acknowledges at the ISP: the line drops */
        ++isr_count;
        isr_fatal |= ev.dma_err & ISP_DMA_ERR_FATAL;
        isr_done_reports += ev.frames_done;
    }
    if (id) fx1_plic_complete(0, id);
    return mepc;
}

static void write_addr64(uint32_t lo_offset, uint64_t addr)
{
    isp_hal.write32(isp_hal.ctx, lo_offset, (uint32_t)addr);
    isp_hal.write32(isp_hal.ctx, lo_offset + 4u, (uint32_t)(addr >> 32));
}

typedef struct {
    uint32_t index_mismatch; /* completions whose output index differs from the input index */
    uint32_t lost, odma_failed, idma_rearmed, recoveries;
    unsigned odma_output;
    uint32_t fatal_seen;
    fx1_isp_abort_report report; /* of the last recovery */
} phase_result;

/* Streams frames 0..7 (resubmitting every reported loss) until each has
 * completed exactly once with the reference CRC. `fault_out` / `fault_in` name
 * the buffer whose address is unmapped (-1: none); `good` holds the real ones. */
static void stream_all(const fx1_isp_buffers* good, int fault_out, int fault_in, uint32_t code,
                       phase_result* r)
{
    uint32_t pending[16], head = 0, tail = 0, done = 0, repaired = 0, draining = 0;
    for (uint32_t f = 0; f < ISP_FX_FRAMES; ++f) pending[tail++] = f;
    const uint64_t start = fx1_mtime();
    while (done != ALL_FRAMES) {
        const uint32_t saved = fx1_irq_save();
        const uint32_t fatal = isr_fatal;
        if (fatal) {
            isr_fatal = 0;
            r->fatal_seen |= fatal;
            if (!repaired) { /* the faulted buffer is software's again: fix it */
                if (fault_out >= 0) {
                    write_addr64(FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u * (unsigned)fault_out, good->y[fault_out]);
                    write_addr64(FX1_ISP_ODMA_UV_ADDR_L0_OFFSET + 16u * (unsigned)fault_out, good->uv[fault_out]);
                }
                if (fault_in >= 0)
                    write_addr64(FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET + 8u * (unsigned)fault_in, good->in[fault_in]);
                repaired = 1;
            }
            fx1_isp_recovery rec;
            fx1_check(fx1_isp_recover_ex(&dev, fatal, &rec) == FX1_ISP_OK, code + 1, "recover handled the error");
            ++r->recoveries;
            if (rec.odma_failed) {
                ++r->odma_failed;
                r->odma_output = rec.odma_output;
            }
            r->idma_rearmed += (uint32_t)rec.idma_rearmed;
            fx1_check(fx1_isp_recovery_report(&dev, &r->report) == FX1_ISP_OK && r->report.sequence == rec.sequence,
                      code + 8, "recovery report of the settled recovery");
            fx1_check(fx1_isp_recover_ex(&dev, fatal, &rec) == FX1_ISP_ENODATA, code + 2,
                      "a second recover with the same snapshot is a no-op");
            draining = 1; /* quiesce: let the frames in flight finish first */
        }
        if (draining && dev.in_flight == 0) draining = 0;
        uint32_t lost_seq;
        unsigned lost_input;
        while (fx1_isp_next_lost(&dev, &lost_seq, &lost_input) == FX1_ISP_OK) {
            ++r->lost;
            fx1_check(tail - head < 16u, code + 3, "resubmission queue");
            (void)lost_input;
            pending[(tail++) & 15u] = frame_of_seq[lost_seq % SEQ_SLOTS]; /* resubmit */
        }
        if (!draining && head != tail && dev.in_flight == 0 && fx1_isp_input_ready(&dev)) {
            const uint32_t n = (tail - head >= MAX_IN_FLIGHT) ? MAX_IN_FLIGHT : 1u; /* a pair when possible */
            const unsigned first = fx1_isp_next_input(&dev);
            for (uint32_t k = 0; k < n; ++k) /* fill both inputs first, then queue back to back */
                isp_fill_input(good->in[(first + k) % FX1_ISP_NUM_BUFFERS], pending[(head + k) & 15u]);
            for (uint32_t k = 0; k < n; ++k) {
                uint32_t seq = 0;
                /* An automatic recovery (MAS v1.07 §9.9.8) may start or settle
                 * between the two queues of a pair: any driver call settles it
                 * and restarts the rotation at 0. A running one leaves the rest
                 * pending; a settled one moved the slot, so the frame is filled
                 * again where it now goes (V107-11 platform CI). */
                if (!fx1_isp_input_ready(&dev)) break;
                unsigned input = fx1_isp_next_input(&dev);
                if (input != (first + k) % FX1_ISP_NUM_BUFFERS) isp_fill_input(good->in[input], pending[head & 15u]);
                const uint32_t tag = 0x7A000000u | (++tags_issued << 8) | pending[head & 15u];
                int rc = fx1_isp_queue_frame_meta(&dev, tag, fx1_isp_frame_counter(&dev), &seq);
                if (rc == FX1_ISP_EAGAIN) { /* settled inside the queue: the slot read above is stale */
                    input = fx1_isp_next_input(&dev);
                    isp_fill_input(good->in[input], pending[head & 15u]);
                    rc = fx1_isp_queue_frame_meta(&dev, tag, fx1_isp_frame_counter(&dev), &seq);
                }
                fx1_check(rc == FX1_ISP_OK, code + 4, "frame queued");
                frame_of_seq[seq % SEQ_SLOTS] = pending[(head++) & 15u];
                tag_of_seq[seq % SEQ_SLOTS] = tag;
                input_of_seq[seq % SEQ_SLOTS] = input;
            }
        }
        unsigned index = 0;
        uint32_t seq = 0;
        fx1_isp_frame_meta meta;
        const int got = fx1_isp_next_done_meta(&dev, &index, &seq, &meta);
        fx1_irq_restore(saved);
        if (got == FX1_ISP_OK) {
            const uint32_t f = frame_of_seq[seq % SEQ_SLOTS];
            if (meta.tag != tag_of_seq[seq % SEQ_SLOTS]) {
                fx1_log_hex("frame", f);
                fx1_log_hex("  ODMA_BUF_TAG got     ", meta.tag);
                fx1_log_hex("  ODMA_BUF_TAG expected", tag_of_seq[seq % SEQ_SLOTS]);
                fx1_fail(0xC0u + (code >> 4));
            }
            r->index_mismatch += index != input_of_seq[seq % SEQ_SLOTS];
            isp_output_to_cpu(good->y[index], good->uv[index]);
            const uint32_t crc = isp_nv12_crc(good->y[index], good->uv[index]);
            if (crc != isp_fx_crc_basic[f]) {
                fx1_log_hex("frame", f);
                fx1_log_hex("  CRC got     ", crc);
                fx1_log_hex("  CRC expected", isp_fx_crc_basic[f]);
                fx1_fail(code + 5);
            }
            fx1_check(!(done & (1u << f)), code + 6, "each frame completes exactly once");
            done |= 1u << f;
            isp_output_to_device(good->y[index], good->uv[index]);
        }
        fx1_check(fx1_mtime() - start < FX1_MTIME_HZ / 5u, code + 7, "phase within 200 ms");
    }
}

static void expect_idle(uint32_t code)
{
    fx1_isp_progress p;
    fx1_check(fx1_isp_get_progress(&dev, &p) == FX1_ISP_OK, code, "progress");
    fx1_check(p.in_flight == 0 && (p.dma_err & ISP_DMA_ERR_FATAL) == 0, code + 1,
              "nothing in flight, no fatal DMA_ERR left");
    fx1_check(!(fx1_plic_pending() & (1u << FX1_IRQ_ISP)), code + 2, "ISP interrupt line low");
}

static void wait_us(uint32_t us)
{
    const uint64_t start = fx1_mtime();
    while (fx1_mtime() - start < (uint64_t)us * (FX1_MTIME_HZ / 1000000u)) {
    }
}

/* Queue one frame and return with interrupts masked, so the caller aborts
 * with no handler in between. Measured on the bus trace: the IDMA reads the
 * 6 KiB input in about 9 us (16-byte transactions every ~24 ns) and the ODMA
 * finishes shortly after, while one driver call costs the CPU about 5 us (every
 * instruction fetch and stack access crosses the fabric). A second queue
 * already let the first frame finish, so the abort comes right after one
 * queue, roughly half-way through the IDMA's read: it starts as soon as the
 * input is VALID. abort_point() checks, immediately before the abort, that
 * the IDMA is actually working on it (DMA_STAT.IDMA_BUSY; the frame counters
 * and the driver's in_flight alone also hold for a frame that is queued but
 * not started, review G5-R2) and that neither frame counter has moved.
 * The IDMA counter is sampled again right after the abort call returns: a
 * soft reset takes effect at once, so it must not have moved (phase C);
 * fx1_isp_stop first disables the DMA engines and only then resets, which
 * takes a few microseconds more, so the input read may finish inside the
 * call (at most +1, phase D). From that sample on, expect_no_stale_completion()
 * requires that neither counter ever moves again and that the ODMA never
 * finished the aborted frame. */
static uint32_t idma_before;
static uint32_t frame_mid_flight(const fx1_isp_buffers* good, uint32_t code, uint32_t* odma_before)
{
    isp_fill_input(good->in[fx1_isp_next_input(&dev)], 0); /* filled first: the queue starts the hardware */
    const uint32_t saved = fx1_irq_save();
    *odma_before = isp_hal.read32(isp_hal.ctx, FX1_ISP_ODMA_FRAME_COUNT_OFFSET);
    idma_before = isp_hal.read32(isp_hal.ctx, FX1_ISP_IDMA_FRAME_COUNT_OFFSET);
    uint32_t seq;
    fx1_check(fx1_isp_input_ready(&dev), code + 1, "input ready");
    fx1_check(fx1_isp_queue_frame_ex(&dev, &seq) == FX1_ISP_OK, code + 2, "frame queued");
    return saved;
}

static void abort_point(uint32_t odma_before, uint32_t code)
{
    fx1_check(isp_hal.read32(isp_hal.ctx, FX1_ISP_DMA_STAT_OFFSET) & FX1_ISP_DMA_STAT_IDMA_BUSY_MASK, code,
              "abort point: the IDMA is busy reading the frame (not merely queued)");
    fx1_check(isp_hal.read32(isp_hal.ctx, FX1_ISP_IDMA_FRAME_COUNT_OFFSET) == idma_before &&
                  isp_hal.read32(isp_hal.ctx, FX1_ISP_ODMA_FRAME_COUNT_OFFSET) == odma_before &&
                  dev.in_flight == 1,
              code + 1, "abort point: the frame is in flight, its input still being read");
}

static uint32_t idma_after_abort(void)
{
    return isp_hal.read32(isp_hal.ctx, FX1_ISP_IDMA_FRAME_COUNT_OFFSET);
}

/* After an abort: for 2 ms neither a completion nor IDMA/ODMA progress may
 * appear; `idma_at_abort` is the IDMA count sampled right after the abort. */
static void expect_no_stale_completion(uint32_t odma_before, uint32_t idma_at_abort, uint32_t code)
{
    const uint32_t reports = isr_done_reports;
    wait_us(2000);
    const uint32_t saved = fx1_irq_save();
    unsigned index;
    const int got = fx1_isp_next_done(&dev, &index);
    fx1_isp_progress p;
    fx1_isp_get_progress(&dev, &p);
    fx1_irq_restore(saved);
    fx1_check(got != FX1_ISP_OK, code, "no completion of an aborted frame");
    fx1_check(isr_done_reports == reports, code + 1, "no DONE reported by the ISR after the abort");
    fx1_check(p.odma_frames == odma_before && p.idma_frames == idma_at_abort && p.in_flight == 0, code + 2,
              "no ODMA completion of the aborted frame, no IDMA progress after the abort, nothing in flight");
}

int main(void)
{
    fx1_isp_buffers good, bad;
    isp_buffers(&good);
    fx1_check(fx1_isp_init(&dev, &isp_hal) == FX1_ISP_OK, 0x01, "driver init");
    fx1_check(fx1_isp_set_geometry(&dev, ISP_FX_WIDTH, ISP_FX_HEIGHT, ISP_FX_BAYER) == FX1_ISP_OK, 0x02,
              "geometry 64x48 RGGB");
    fx1_isp_apply(&dev, isp_fx_profile_basic, sizeof(isp_fx_profile_basic) / sizeof(isp_fx_profile_basic[0]));
    fx1_set_trap_handler(handler);
    fx1_plic_set_priority(FX1_IRQ_ISP, 1);
    fx1_plic_enable(0, FX1_IRQ_ISP, 1);
    fx1_irq_enable(FX1_MIE_MEIE);
    fx1_global_irq_enable();

    /* A: ODMA AXI error on output buffer 1 */
    /* static (zeroed .bss, main runs once): a {0} initialiser of this size
     * is compiled to a memset call, which -nostdlib does not provide. */
    static phase_result a;
    bad = good;
    bad.y[2] = UNMAPPED;
    bad.uv[2] = UNMAPPED + 0x1000u;
    isp_outputs_to_device(&good);
    fx1_check(fx1_isp_start(&dev, &bad) == FX1_ISP_OK, 0x10, "A: start with output 2 unmapped");
    stream_all(&good, 2, -1, 0x10, &a);
    fx1_check(a.fatal_seen == FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT, 0x18, "A: exactly ODMA_AXI reported");
    fx1_check(a.recoveries == 1 && a.odma_failed == 1 && a.odma_output == 2 && a.lost >= 1 && a.lost <= 2 &&
                  a.idma_rearmed == 0,
              0x19, "A: one recovery, output buffer 2 failed, its frame (and the one behind) resubmitted");
    fx1_check(a.report.count >= 1 && (a.report.rec[0].failed_outputs & 1u) &&
                  a.report.rec[0].cause == FX1_ISP_ABORT_CAUSE_WRITE,
              0x1F, "A: the record names a frame with CH0 failed by a write error");
    fx1_check(a.index_mismatch == 0, 0x1E, "A: the rotations restarted together: output index = input index");
    fx1_log_hex("A: frames lost and resubmitted", a.lost);
    expect_idle(0x1A);
    fx1_check(fx1_isp_stop(&dev) == 0, 0x1D, "A: stop with nothing in flight");
    fx1_log("A: ODMA AXI error -> recovery, lost frames resubmitted, 8 frames correct");

    /* B: IDMA AXI error on input buffer 2 */
    static phase_result b;
    bad = good;
    bad.in[2] = UNMAPPED;
    isp_outputs_to_device(&good);
    fx1_check(fx1_isp_start(&dev, &bad) == FX1_ISP_OK, 0x20, "B: start with input 2 unmapped");
    stream_all(&good, -1, 2, 0x20, &b);
    fx1_check(b.fatal_seen == FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT, 0x28, "B: exactly IDMA_AXI reported");
    fx1_check(b.recoveries == 1 && b.idma_rearmed == 0 && b.lost >= 1 && b.odma_failed == 0, 0x29,
              "B: one recovery, the frame of input 2 lost and resubmitted (no in-place retry)");
    for (unsigned i = 0; i < b.report.count; ++i)
        fx1_check(b.report.rec[i].cause == FX1_ISP_ABORT_CAUSE_READ, 0x2E, "B: recorded frames have cause 1 (read)");
    fx1_log_hex("B: frames lost and resubmitted", b.lost);
    expect_idle(0x2A);
    fx1_check(fx1_isp_stop(&dev) == 0, 0x2D, "B: stop with nothing in flight");
    fx1_log("B: IDMA AXI error -> recovery, lost frame resubmitted, 8 frames correct");

    /* C: soft reset in the middle of a frame. C, D and E abort frames in
     * flight: short ISP bursts keep the abort window open (isp_common.h). */
    good.max_burst_beats = ISP_ABORT_MAX_BURST_BEATS;
    static phase_result c;
    uint32_t odma_before = 0;
    isp_outputs_to_device(&good);
    fx1_check(fx1_isp_start(&dev, &good) == FX1_ISP_OK, 0x30, "C: start");
    uint32_t saved = frame_mid_flight(&good, 0x31, &odma_before);
    abort_point(odma_before, 0x34);
    fx1_isp_soft_reset(&dev);
    const uint32_t idma_c = idma_after_abort();
    fx1_check(dev.in_flight == 0, 0x37, "C: soft reset drops the frames in flight");
    fx1_irq_restore(saved);
    fx1_check(idma_c == idma_before, 0x3B, "C: the soft reset stopped the IDMA before it finished the input");
    expect_no_stale_completion(odma_before, idma_c, 0x38);
    isp_outputs_to_device(&good);
    stream_all(&good, -1, -1, 0x40, &c);
    fx1_check(c.fatal_seen == 0 && c.lost == 0, 0x48, "C: clean stream after the soft reset");
    expect_idle(0x49);
    fx1_check(fx1_isp_stop(&dev) == 0, 0x4C, "C: stop");
    fx1_log("C: soft reset mid-frame: no stale completion, restart from buffer 0, 8 frames correct");

    /* D: stop in the middle of a frame, then start again */
    static phase_result d;
    isp_outputs_to_device(&good);
    fx1_check(fx1_isp_start(&dev, &good) == FX1_ISP_OK, 0x50, "D: start");
    saved = frame_mid_flight(&good, 0x51, &odma_before);
    abort_point(odma_before, 0x54);
    const int discarded = fx1_isp_stop(&dev);
    const uint32_t idma_d = idma_after_abort();
    fx1_irq_restore(saved);
    fx1_check(discarded == 1, 0x57, "D: stop discards the frame in flight");
    fx1_check(idma_d - idma_before <= 1u, 0x5C, "D: at most the aborted input finished during the stop call");
    fx1_log_hex("D: inputs finished during the stop call", idma_d - idma_before);
    expect_no_stale_completion(odma_before, idma_d, 0x58);
    isp_outputs_to_device(&good);
    fx1_check(fx1_isp_start(&dev, &good) == FX1_ISP_OK, 0x5B, "D: start again after stop");
    stream_all(&good, -1, -1, 0x60, &d);
    fx1_check(d.fatal_seen == 0 && d.lost == 0, 0x68, "D: clean stream after the restart");
    expect_idle(0x69);
    fx1_check(fx1_isp_stop(&dev) == 0, 0x6C, "D: stop");
    fx1_log_hex("D: frames discarded by stop", (uint32_t)discarded);
    fx1_log("D: stop mid-frame, start again, 8 frames correct");

    /* E: DMA soft reset in the middle of a frame, stream kept enabled */
    static phase_result e;
    isp_outputs_to_device(&good);
    fx1_check(fx1_isp_start(&dev, &good) == FX1_ISP_OK, 0x70, "E: start");
    /* Two frames queued back to back, then the reset at once: the second
     * cannot complete inside the call (it waits for the first one's input),
     * so at least one frame is aborted whatever the driver's own latency
     * (it samples ODMA_BUF_DONE before the reset, review U6-R1). From the
     * counters sampled right after the call, nothing may progress. */
    for (uint32_t k = 0; k < 2; ++k) isp_fill_input(good.in[k], k);
    saved = fx1_irq_save();
    odma_before = isp_hal.read32(isp_hal.ctx, FX1_ISP_ODMA_FRAME_COUNT_OFFSET);
    idma_before = isp_hal.read32(isp_hal.ctx, FX1_ISP_IDMA_FRAME_COUNT_OFFSET);
    for (uint32_t k = 0; k < 2; ++k) {
        uint32_t seq;
        fx1_check(fx1_isp_input_ready(&dev) && fx1_isp_queue_frame_ex(&dev, &seq) == FX1_ISP_OK, 0x71 + k,
                  "E: frame queued");
    }
    const int dropped = fx1_isp_dma_reset(&dev);
    const uint32_t idma_e = idma_after_abort();
    const uint32_t odma_e = isp_hal.read32(isp_hal.ctx, FX1_ISP_ODMA_FRAME_COUNT_OFFSET);
    fx1_irq_restore(saved);
    fx1_check(dropped == 2 && dev.in_flight == 0, 0x77, "E: the DMA reset drops the frames in flight");
    fx1_check(odma_e - odma_before <= 1u && idma_e - idma_before <= 1u, 0x7B,
              "E: the second frame was aborted (at most the first one finished during the call)");
    fx1_log_hex("E: outputs completed during the reset call", odma_e - odma_before);
    expect_no_stale_completion(odma_e, idma_e, 0x78);
    fx1_check(fx1_isp_next_input(&dev) == 0, 0x7C, "E: rotation restarts at buffer 0");
    isp_outputs_to_device(&good);
    stream_all(&good, -1, -1, 0x80, &e); /* no fx1_isp_start: the stream stayed enabled */
    fx1_check(e.fatal_seen == 0 && e.lost == 0, 0x88, "E: clean stream after the DMA reset");
    expect_idle(0x89);
    fx1_check(fx1_isp_stop(&dev) == 0, 0x8C, "E: stop");
    fx1_log("E: DMA reset mid-frame: no stale completion, stream continues from buffer 0, 8 frames correct");
    fx1_log_hex("ISP interrupts taken", isr_count);
    return 0;
}
