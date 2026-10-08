/*
 * G4a / plan P4: the ISP on the FX1 platform, driven by the CPU through the
 * reference C99 driver (components/FX1_Components/isp/driver) on an MMIO HAL.
 *
 * RAW in DDR -> ISP IDMA -> pipeline -> ISP ODMA -> NV12 in DDR, 8 frames of
 * 64x48 RGGB through the 4-buffer rotation (each buffer reused twice), with up
 * to two frames in flight. The CPU fills every input buffer, takes the ISP
 * interrupt through PLIC source 3 (the driver acknowledges it at the device),
 * and checks every completed frame's NV12 against the CRC-32 the pinned Python
 * reference produced for that frame (isp_fixture.h).
 *
 * Completion is tracked per sequence number: the ISR marks the sequences of
 * the outputs it sees DONE (the oldest `frames_done` entries of the driver
 * queue; that count is a snapshot of the backlog, not a count of new events),
 * and main only consumes an output whose sequence the ISR has marked, so every
 * completion is proven to have been signalled by the interrupt.
 *
 * Build-time selection:
 *   ISP_FX_PROFILE 0 basic, 1 basic + GTM auto (frames after the first depend
 *                  on the previous ones)
 *   ISP_FX_HART1_DMA 1  hart 1 runs SYS_DMA copies concurrently in its own DDR
 *                  region (tests/shared_mem/hart1_dma.c, plan C12)
 *   ISP_FX_BACKLOG 1 the consumer lags: it takes the oldest output only once
 *                  the next one is DONE too (or when draining), and leaves the
 *                  IRQ open 500 us after every queue, so the ISR sees two
 *                  outputs DONE at once and reports the same output in more
 *                  than one snapshot (both checked: summing snapshots fails)
 */
#include "isp_common.h"

#ifndef ISP_FX_BACKLOG
#define ISP_FX_BACKLOG 0
#endif
#ifndef ISP_FX_HART1_DMA
#define ISP_FX_HART1_DMA 0
#endif
#if ISP_FX_HART1_DMA
void shared_mem_start(void);
void shared_mem_mark(uint32_t point); /* 0: first frame queued, k: k frames completed */
void shared_mem_finish(void);
#endif

#if ISP_FX_PROFILE == 0
#define PROFILE isp_fx_profile_basic
#define EXPECTED isp_fx_crc_basic
#define PROFILE_NAME "basic"
#else
#define PROFILE isp_fx_profile_gtm_auto
#define EXPECTED isp_fx_crc_gtm_auto
#define PROFILE_NAME "basic + GTM auto"
#endif

#define MAX_IN_FLIGHT 2u
#define ALL_FRAMES    ((1u << ISP_FX_FRAMES) - 1u)

static fx1_isp_dev dev;
static volatile uint32_t isp_irqs, dma_errors, last_dma_err, flow_events;
static volatile uint32_t notified;         /* bit seq: the ISR saw that output DONE */
static volatile uint32_t max_backlog;      /* largest frames_done snapshot */
static volatile uint32_t snapshot_sum;     /* sum of frames_done: NOT a completion count */
static uint32_t frame_of_seq[ISP_FX_FRAMES];

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
        fx1_isp_irq(&dev, &ev); /* reads and acknowledges at the ISP: the line drops */
        ++isp_irqs;
        /* frames_done counts the DONE outputs still waiting in the queue, in
         * queue order; mark their sequences (idempotent across snapshots). */
        for (unsigned k = 0; k < ev.frames_done && k < dev.in_flight; ++k)
            notified |= 1u << dev.queue[k].seq;
        if (ev.frames_done > max_backlog) max_backlog = ev.frames_done;
        snapshot_sum += ev.frames_done;
        if (ev.dma_err & ISP_DMA_ERR_FATAL) {
            ++dma_errors;
            last_dma_err = ev.dma_err;
        } else if (ev.dma_err) {
            ++flow_events;
        }
    }
    if (id) fx1_plic_complete(0, id);
    return mepc;
}

static void irq_window(uint64_t ticks)
{
    const uint64_t start = fx1_mtime();
    while (fx1_mtime() - start < ticks) {
    }
}

int main(void)
{
    fx1_isp_buffers bufs;
    isp_buffers(&bufs);
    fx1_check(fx1_isp_init(&dev, &isp_hal) == FX1_ISP_OK, 0x01, "driver init");
    fx1_check(fx1_isp_set_geometry(&dev, ISP_FX_WIDTH, ISP_FX_HEIGHT, ISP_FX_BAYER) == FX1_ISP_OK, 0x02,
              "geometry 64x48 RGGB");
    fx1_isp_apply(&dev, PROFILE, sizeof(PROFILE) / sizeof(PROFILE[0]));

    fx1_set_trap_handler(handler);
    fx1_plic_set_priority(FX1_IRQ_ISP, 1);
    fx1_plic_enable(0, FX1_IRQ_ISP, 1);
    fx1_irq_enable(FX1_MIE_MEIE);
    fx1_global_irq_enable();
    isp_outputs_to_device(&bufs); /* the ODMA may write any of them from here on */
    fx1_check(fx1_isp_start(&dev, &bufs) == FX1_ISP_OK, 0x03, "stream start");
    fx1_log(ISP_FX_BACKLOG ? "ISP streaming, profile " PROFILE_NAME ", lagging consumer"
                           : "ISP streaming, profile " PROFILE_NAME);

#if ISP_FX_HART1_DMA
    shared_mem_start();
#endif
    uint32_t queued = 0, completed = 0;
    const uint64_t start = fx1_mtime();
    while (completed < ISP_FX_FRAMES) {
        /* The driver is not reentrant: keep the ISR out while main uses it. */
        uint32_t saved = fx1_irq_save();
        int queued_now = 0;
        if (queued < ISP_FX_FRAMES && dev.in_flight < MAX_IN_FLIGHT && fx1_isp_input_ready(&dev)) {
            isp_fill_input(bufs.in[fx1_isp_next_input(&dev)], queued); /* ends with the hand-over */
            uint32_t seq = 0;
            fx1_check(fx1_isp_queue_frame_ex(&dev, &seq) == FX1_ISP_OK && seq < ISP_FX_FRAMES, 0x10,
                      "frame queued");
            frame_of_seq[seq] = queued++;
            queued_now = 1;
#if ISP_FX_HART1_DMA
            if (queued == 1) shared_mem_mark(0);
#endif
        }
        const int consumer_may_run = !ISP_FX_BACKLOG || queued == ISP_FX_FRAMES ||
                                     (dev.in_flight >= 2 && (notified & (1u << dev.queue[1].seq)));
        unsigned index = 0;
        uint32_t seq = 0;
        int got = FX1_ISP_ENODATA;
        if (consumer_may_run && dev.in_flight && (notified & (1u << dev.queue[0].seq)))
            got = fx1_isp_next_done_ex(&dev, &index, &seq);
        fx1_irq_restore(saved);
        if (ISP_FX_BACKLOG && queued_now) irq_window(FX1_MTIME_HZ / 2000u); /* 500 us, IRQ open */
        if (got == FX1_ISP_OK) {
            isp_output_to_cpu(bufs.y[index], bufs.uv[index]); /* after the completion */
            const uint32_t frame = frame_of_seq[seq];
            const uint32_t crc = isp_nv12_crc(bufs.y[index], bufs.uv[index]);
            if (crc != EXPECTED[frame]) {
                fx1_log_hex("frame", frame);
                fx1_log_hex("  output buffer", index);
                fx1_log_hex("  CRC got     ", crc);
                fx1_log_hex("  CRC expected", EXPECTED[frame]);
                fx1_fail(0x20 + frame);
            }
            fx1_check(frame == completed, 0x30, "frames complete in queue order");
            isp_output_to_device(bufs.y[index], bufs.uv[index]); /* consumed: may be re-queued */
            ++completed;
#if ISP_FX_HART1_DMA
            shared_mem_mark(completed);
#endif
        } else {
            fx1_check(got == FX1_ISP_ENODATA, 0x31, "next_done: only OK or nothing yet");
        }
        if (dma_errors) {
            fx1_log_hex("ISP DMA_ERR", last_dma_err);
            fx1_fail(0x40);
        }
        fx1_check(fx1_mtime() - start < (uint64_t)FX1_MTIME_HZ * 2u, 0x41, "stream finishes within 2 s");
    }

#if ISP_FX_HART1_DMA
    shared_mem_finish();
#endif
    fx1_isp_progress progress;
    fx1_check(fx1_isp_get_progress(&dev, &progress) == FX1_ISP_OK, 0x50, "progress");
    fx1_check(progress.idma_frames == ISP_FX_FRAMES && progress.odma_frames == ISP_FX_FRAMES &&
                  progress.in_flight == 0 && (progress.dma_err & ISP_DMA_ERR_FATAL) == 0, 0x51,
              "IDMA/ODMA frame counters 8, nothing in flight, no fatal DMA error");
    fx1_check(notified == ALL_FRAMES, 0x52, "the ISR signalled every frame's completion (one mark per sequence)");
    fx1_check(!(fx1_plic_pending() & (1u << FX1_IRQ_ISP)), 0x53, "ISP interrupt line low after handling");
    fx1_log_hex("ISP interrupts taken", isp_irqs);
    fx1_log_hex("largest DONE backlog seen by the ISR", max_backlog);
    fx1_log_hex("sum of frames_done snapshots", snapshot_sum);
    fx1_log_hex("lossless underrun/overflow reports", flow_events);
    if (ISP_FX_BACKLOG) {
        fx1_check(max_backlog >= 2, 0x55, "the lagging consumer produced a real backlog");
        fx1_check(snapshot_sum > ISP_FX_FRAMES, 0x56, "outputs were reported in several snapshots");
    }
    fx1_check(fx1_isp_stop(&dev) >= 0, 0x54, "stream stopped");
    fx1_log("8 frames: NV12 CRC matches the Python reference for every frame");
    return 0;
}
