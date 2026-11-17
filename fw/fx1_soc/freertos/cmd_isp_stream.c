/*
 * `isp stream start|status|stop|wait [MS]`: the ISP RAW-sequence stream, one
 * ISP session over a host-prepared sequence (fx1/fx1_isp_stream.h,
 * platforms/fx1_soc/docs/ISP_STREAM_CONTRACT.md; the host side is
 * isp_stream_prep.py and fx1_soc --isp-stream-spool).
 *
 *   start  checks the descriptor and that a host bridge is attached, claims
 *          the ISP (ISP_OWNER_STREAM), configures it once as `isp run` does
 *          (geometry, the profile, in profile mode built into bank 0, sealed
 *          and applied), starts it and hands the session to the stream task;
 *          returns at once
 *   status the status block
 *   stop   a local stop request (as the host's STOP, USER)
 *   wait   until the session is STOPPED or ERROR (MS: simulated-time limit),
 *          then the summary; counted as a check (ERROR fails)
 *   idle   until no session runs and the stream holds the ISP no more (an
 *          ended session's stop may still drain, below; MS: limit), then the
 *          summary; counted as a check (passes once the ISP is free)
 *
 * Stream task, per physical frame n (source (n / K) mod S, repeat n mod K):
 * SYS_DMA channel 1 copies the pool container into the input slot the driver
 * names (one chain of 1008-byte commands per input slot), the frame is
 * queued (tag = n), up to `queue` frames in flight. Each completion
 * (fx1_isp_next_done_meta) is published in record[output slot] and the
 * doorbell rung; an output slot is queued again only once the host has
 * acknowledged its record (ack_seq == the seq published there, the record
 * still this session's). The spool budget the bridge attached (control
 * block) is an admission limit: at most max_bytes / NV12 frame bytes frames
 * are submitted, then the session stops (LIMIT). The profile is applied once:
 * GTM, 2DNR and statistics history carry across every frame, repeat and
 * wrap. After the first completion the LSC residents are filled as `isp run`
 * does (ISP-UPD-001). Stop requests (host control block, `isp stream stop`)
 * end submission; the frames in flight are completed, published and
 * acknowledged, then the ISP is stopped. A copy error, a fatal DMA_ERR, a
 * completion or ACK missing for frame_budget_us of simulated time, or a host
 * protocol stop ends the session in ERROR (no reset, no replay).
 *
 * The stop (S4 review R3): fx1_isp_stop's soft reset is a recovery that
 * drains the AXI transactions the bus still holds (a read the DDR answers
 * late); until it has retired the ISP ignores W1C writes and its level
 * interrupt stays high (COMMON_IRQ_STATUS.ERROR). The ISP source is masked
 * at the PLIC from the stop (or from the first fatal DMA_ERR, whose automatic
 * recovery does the same) until the stop has finished, and the stop is
 * retried from the task (EBUSY: still STREAMING, the buffers not back), one
 * frame budget long. Past it the terminal state is published with the ISP
 * still held (status.isp_drain PENDING: claimed by the stream, masked, no
 * session starts; the CLI and the timers run), and the task keeps retrying;
 * once the stop has finished the ISP is acknowledged, unmasked and released
 * (isp_drain DONE). A clean end (STOPPED) whose stop does not finish within
 * the budget is reported ERROR TIMEOUT.
 */
#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "dma/fx1_dma_regs.h"
#include "fx1/fx1_isp_stream.h"
#include "isp_common.h"
#include "isp_demo_setup.h"
#include "rtos_app.h"
#include "rtos_bsp.h"

#define DMA(offset) (FX1_SYS_DMA_CSR_BASE + (offset))
#define CH(offset) DMA(FX1_DMA_CH(FX1_ISP_STREAM_DMA_CHANNEL, (offset)))
#define DMA_CFG                                                                                                \
    (FX1_DMA_ADDRESS_INCREMENT | FX1_DMA_OUTSTANDING_ENABLE | FX1_DMA_CFG_PENDING(4) | FX1_DMA_CFG_TOKENS(1) | \
     FX1_DMA_CFG_BURST_BYTES(64))

#define DESC ((const volatile fx1_isp_stream_desc*)FX1_ISP_STREAM_DESC_ADDR)
#define STATUS ((volatile fx1_isp_stream_status*)FX1_ISP_STREAM_STATUS_ADDR)
#define CONTROL ((const volatile fx1_isp_stream_control*)FX1_ISP_STREAM_CONTROL_ADDR)
#define RECORD(i) \
    ((volatile fx1_isp_stream_record*)(FX1_ISP_STREAM_RECORD_ADDR + (i) * FX1_ISP_STREAM_RECORD_BYTES))
#define PROFILE ((const fx1_isp_reg_write*)FX1_ISP_STREAM_PROFILE_ADDR)

/* status.detail of a LIMIT stop */
#define LIMIT_SPOOL 1u /* the host's STOP, LIMIT (spool budget) */
#define LIMIT_SEQ 2u   /* the 32-bit publish sequence would wrap */

static fx1_isp_dev dev;
static SemaphoreHandle_t isp_done, dma_done, start_req;
static volatile uint32_t isr_fatal, dma_status;
static volatile int local_stop; /* `isp stream stop` */
static volatile int active;     /* a session is running (start .. STOPPED / ERROR) */
static volatile int isp_held;   /* an ended session's stop still drains: the ISP stays the stream's */

static struct {
    fx1_isp_stream_desc d; /* the descriptor, copied once at start */
    uint64_t session, frames, submitted, completed;
    uint64_t admit; /* frames the spool budget holds (UINT64_MAX: no budget) */
    uint32_t slot_seq[FX1_ISP_STREAM_SLOTS]; /* seq last published in record i, 0: none */
    uint32_t publish_seq, chain_cmds, out_w, out_h, uv_offset, colorimetry, profile_id;
    uint64_t t_first;
    struct {
        uint64_t ordinal;
        uint32_t driver_seq, source, repeat, copy_ts;
    } fly[FX1_ISP_STREAM_SLOTS]; /* in flight, in queue order */
    unsigned fly_head, fly_count;
    int lsc_done;
} s;

/* ---- interrupts ------------------------------------------------------------ */
/* The ISP source is masked with priority 0 (never notifies, never claimed): a
 * register of its own, no read-modify-write of the enable word the other
 * sources share. Called with interrupts disabled (the ISR, or a critical
 * section). */
#define ISP_PRIORITY (FX1_PLIC_BASE + FX1_PLIC_PRIORITY_OFF + 4u * FX1_IRQ_ISP)
static uint32_t isp_priority; /* the priority to restore */
static volatile int isp_masked;

static void mask_isp_irq(void)
{
    if (isp_masked) return;
    isp_priority = fx1_read32(ISP_PRIORITY);
    fx1_write32(ISP_PRIORITY, 0);
    isp_masked = 1;
}

static void unmask_isp_irq(void)
{
    taskENTER_CRITICAL();
    if (isp_masked) {
        fx1_write32(ISP_PRIORITY, isp_priority);
        isp_masked = 0;
    }
    taskEXIT_CRITICAL();
}

BaseType_t isp_stream_isp_isr(void)
{
    BaseType_t woken = pdFALSE;
    fx1_isp_events ev;
    fx1_isp_irq(&dev, &ev); /* reads and acknowledges at the ISP: the line drops */
    isr_fatal |= ev.dma_err & ISP_DMA_ERR_FATAL;
    /* An AXI error starts a recovery: the acknowledge is ignored until it has
     * retired, the line stays high. The session ends in ERROR; its stop unmasks. */
    if (ev.dma_err & ISP_DMA_ERR_FATAL) mask_isp_irq();
    if (ev.frames_done || (ev.dma_err & ISP_DMA_ERR_FATAL)) xSemaphoreGiveFromISR(isp_done, &woken);
    return woken;
}

BaseType_t isp_stream_dma_isr(void)
{
    BaseType_t woken = pdFALSE;
    const uint32_t st = fx1_read32(CH(FX1_DMA_CH_INTERRUPT_STATUS));
    fx1_write32(CH(FX1_DMA_CH_INTERRUPT_CLEAR), st); /* clear at the device first */
    dma_status |= st;
    xSemaphoreGiveFromISR(dma_done, &woken);
    return woken;
}

/* ---- helpers ------------------------------------------------------------------ */
static uint32_t active_w(const fx1_isp_stream_desc* d) { return (d->width - (d->bayer & 1u)) & ~1u; }
static uint32_t active_h(const fx1_isp_stream_desc* d) { return (d->height - ((d->bayer >> 1) & 1u)) & ~1u; }
static TickType_t us_ticks(uint32_t us)
{
    const TickType_t t = pdMS_TO_TICKS((us + 999u) / 1000u);
    return t ? t : 1;
}

static void set_state(uint32_t state) { STATUS->state = state; }

static void ring(uint32_t value)
{
    fx1_mb();
    fx1_write32(FX1_SIM_CTRL_BASE + FX1_SIM_CTRL_DOORBELL_OFF, value);
}

static void put64(volatile uint32_t* lo, uint64_t v)
{
    lo[0] = (uint32_t)v;
    lo[1] = (uint32_t)(v >> 32);
}

/* 0 if the descriptor is usable, else 1 + the index of the first bad word. */
static uint32_t check_desc(const fx1_isp_stream_desc* d)
{
#define BAD(field) return 1u + (uint32_t)(offsetof(fx1_isp_stream_desc, field) / 4u)
    if (d->magic != FX1_ISP_STREAM_DESC_MAGIC) BAD(magic);
    if (d->version != FX1_ISP_STREAM_VERSION) BAD(version);
    if (d->size != sizeof(*d)) BAD(size);
    if (d->flags & ~FX1_ISP_STREAM_FLAG_LOOP) BAD(flags);
    if (!d->session_lo && !d->session_hi) BAD(session_lo);
    if (d->width < 2u || d->width > 3842u || (d->width & 1u)) BAD(width);
    if (d->height < 2u || d->height > 2162u || (d->height & 1u)) BAD(height);
    if (d->bayer > 3u) BAD(bayer);
    if (d->depth > 1u) BAD(depth);
    if (d->in_stride < 2u * d->width || (d->in_stride & 15u)) BAD(in_stride);
    if ((uint64_t)d->in_stride * d->height != d->frame_bytes || d->frame_bytes > FX1_ISP_STREAM_SLOT_BYTES ||
        (d->frame_bytes + FX1_ISP_STREAM_DMA_CHUNK - 1u) / FX1_ISP_STREAM_DMA_CHUNK * 16u > FX1_ISP_STREAM_CHAIN_PITCH)
        BAD(frame_bytes);
    if (!d->source_count) BAD(source_count);
    if (d->source_base < FX1_ISP_STREAM_POOL_BASE || (d->source_base & (FX1_ISP_STREAM_POOL_ALIGN - 1u)))
        BAD(source_base);
    if (d->source_pitch < d->frame_bytes || (d->source_pitch & (FX1_ISP_STREAM_POOL_ALIGN - 1u)) ||
        (uint64_t)d->source_base + (uint64_t)d->source_count * d->source_pitch > FX1_ISP_STREAM_POOL_END)
        BAD(source_pitch);
    if (!d->repeat) BAD(repeat);
    if ((d->flags & FX1_ISP_STREAM_FLAG_LOOP) ? (d->frames_lo || d->frames_hi) : !(d->frames_lo || d->frames_hi))
        BAD(frames_lo);
    if (d->queue != 1u && d->queue != 2u && d->queue != 4u) BAD(queue);
    if (d->mode > FX1_ISP_STREAM_MODE_LEGACY) BAD(mode);
    if (!d->profile_count || d->profile_count > FX1_ISP_STREAM_PROFILE_MAX) BAD(profile_count);
    if (d->out_stride < active_w(d) || (d->out_stride & 15u) ||
        FX1_ISP_STREAM_UV_OFFSET((uint64_t)d->out_stride * active_h(d)) +
                (uint64_t)d->out_stride * active_h(d) / 2u > FX1_ISP_STREAM_SLOT_BYTES)
        BAD(out_stride);
    if (d->frame_budget_us < 1000u) BAD(frame_budget_us);
    return 0;
#undef BAD
}

/* The chain of input slot k: command 0 in the registers, commands 1.. in DDR
 * (16-byte descriptors {src, dst, size, control}); the destinations are fixed,
 * the sources are written per frame. */
static void build_chains(void)
{
    const uint32_t n = s.chain_cmds, bytes = s.d.frame_bytes;
    for (unsigned k = 0; k < FX1_ISP_STREAM_SLOTS; ++k) {
        volatile uint32_t* e = (volatile uint32_t*)(FX1_ISP_STREAM_CHAIN_ADDR + k * FX1_ISP_STREAM_CHAIN_PITCH);
        const uint32_t dst = FX1_ISP_STREAM_IN_ADDR(k);
        for (uint32_t c = 1; c < n; ++c, e += 4) {
            const uint32_t off = c * FX1_ISP_STREAM_DMA_CHUNK;
            e[1] = dst + off;
            e[2] = bytes - off < FX1_ISP_STREAM_DMA_CHUNK ? bytes - off : FX1_ISP_STREAM_DMA_CHUNK;
            e[3] = c + 1u < n ? (uint32_t)(uintptr_t)(e + 4) : (FX1_DMA_CMD_SET_INT | FX1_DMA_CMD_LAST);
        }
    }
}

/* Copies source `src` into input slot k: 0, or the DMA interrupt status (or
 * 0xFFFFFFFF for a timeout). */
static uint32_t copy_frame(unsigned k, uint32_t src)
{
    const uint32_t n = s.chain_cmds, dst = FX1_ISP_STREAM_IN_ADDR(k);
    const uint32_t chain = FX1_ISP_STREAM_CHAIN_ADDR + k * FX1_ISP_STREAM_CHAIN_PITCH;
    volatile uint32_t* e = (volatile uint32_t*)chain;
    for (uint32_t c = 1; c < n; ++c, e += 4) e[0] = src + c * FX1_ISP_STREAM_DMA_CHUNK;
    fx1_dma_prepare((const volatile void*)(uintptr_t)chain, (n - 1u) * 16u, FX1_DMA_TO_DEVICE);
    fx1_dma_prepare((const volatile void*)(uintptr_t)src, s.d.frame_bytes, FX1_DMA_TO_DEVICE);
    fx1_dma_prepare((const volatile void*)(uintptr_t)dst, s.d.frame_bytes, FX1_DMA_FROM_DEVICE);
    xSemaphoreTake(dma_done, 0); /* drop a stale give */
    dma_status = 0;
    fx1_write32(CH(FX1_DMA_CH_ENABLE), 0);
    fx1_write32(CH(FX1_DMA_CH_INTERRUPT_CLEAR), FX1_DMA_INTERRUPT_MASK);
    fx1_write32(CH(FX1_DMA_CH_INTERRUPT_ENABLE), FX1_DMA_INTERRUPT_MASK);
    fx1_write32(CH(FX1_DMA_CH_CMD_READ_ADDR), src);
    fx1_write32(CH(FX1_DMA_CH_CMD_WRITE_ADDR), dst);
    fx1_write32(CH(FX1_DMA_CH_CMD_TRANSFER_SIZE),
                s.d.frame_bytes < FX1_ISP_STREAM_DMA_CHUNK ? s.d.frame_bytes : FX1_ISP_STREAM_DMA_CHUNK);
    fx1_write32(CH(FX1_DMA_CH_CMD_CONTROL), n > 1u ? chain : (FX1_DMA_CMD_SET_INT | FX1_DMA_CMD_LAST));
    fx1_write32(CH(FX1_DMA_CH_READ_CONFIG), DMA_CFG);
    fx1_write32(CH(FX1_DMA_CH_WRITE_CONFIG), DMA_CFG);
    fx1_write32(CH(FX1_DMA_CH_PERIPHERAL_CONFIG), 0);
    fx1_write32(CH(FX1_DMA_CH_MODE_CONFIG), FX1_DMA_MODE_SWAP_NONE);
    fx1_write32(CH(FX1_DMA_CH_ENABLE), 1);
    fx1_write32(CH(FX1_DMA_CH_START), 1);
    const int irq = xSemaphoreTake(dma_done, us_ticks(s.d.frame_budget_us)) == pdTRUE;
    fx1_write32(CH(FX1_DMA_CH_ENABLE), 0);
    fx1_dma_complete((const volatile void*)(uintptr_t)dst, s.d.frame_bytes, FX1_DMA_FROM_DEVICE);
    if (!irq) return 0xFFFFFFFFu;
    return dma_status == FX1_DMA_INT_COMMAND_COMPLETE ? 0u : (dma_status ? dma_status : 0xFFFFFFFEu);
}

/* The host has taken output slot i's last frame (or it never carried one in
 * this session): the ACK must name the seq the firmware published there (its
 * own copy, not the record's) in a record that is still this session's. */
static int slot_acked(unsigned i)
{
    const uint32_t want = s.slot_seq[i];
    if (!want) return 1;
    volatile fx1_isp_stream_record* r = RECORD(i);
    fx1_acquire();
    return r->ack_seq == want && r->magic == FX1_ISP_STREAM_RECORD_MAGIC && r->session_lo == s.d.session_lo &&
           r->session_hi == s.d.session_hi;
}

static void init_records(void)
{
    for (unsigned i = 0; i < FX1_ISP_STREAM_SLOTS; ++i) {
        volatile fx1_isp_stream_record* r = RECORD(i);
        r->state = FX1_ISP_STREAM_REC_FREE;
        fx1_release();
        volatile uint32_t* w = (volatile uint32_t*)r;
        for (unsigned k = 2; k < FX1_ISP_STREAM_RECORD_BYTES / 4u; ++k) w[k] = 0;
        r->session_lo = s.d.session_lo;
        r->session_hi = s.d.session_hi;
        r->slot = i;
        r->magic = FX1_ISP_STREAM_RECORD_MAGIC;
    }
    fx1_release();
}

static void publish(unsigned index, uint32_t driver_seq, const fx1_isp_frame_meta* meta)
{
    volatile fx1_isp_stream_record* r = RECORD(index);
    const unsigned h = s.fly_head;
    uint32_t frame_id = 0, profile = 0;
    fx1_isp_output_meta(&dev, 0, index, NULL, &frame_id, &profile);
    fx1_dma_complete((const volatile void*)(uintptr_t)FX1_ISP_STREAM_OUT_ADDR(index),
                     s.uv_offset + s.d.out_stride * s.out_h / 2u, FX1_DMA_FROM_DEVICE);
    r->state = FX1_ISP_STREAM_REC_FREE;
    fx1_release();
    r->magic = FX1_ISP_STREAM_RECORD_MAGIC;
    r->session_lo = s.d.session_lo;
    r->session_hi = s.d.session_hi;
    put64(&r->ordinal_lo, s.fly[h].ordinal);
    r->source_index = s.fly[h].source;
    r->repeat_index = s.fly[h].repeat;
    r->driver_seq = driver_seq;
    r->tag = meta->tag;
    r->frame_id = frame_id;
    r->profile_id = profile;
    r->ts = meta->ts;
    r->sof_ts = meta->sof_ts;
    r->eof_ts = meta->eof_ts;
    r->slot = index;
    r->y_addr = FX1_ISP_STREAM_OUT_ADDR(index);
    r->uv_addr = FX1_ISP_STREAM_OUT_ADDR(index) + s.uv_offset;
    r->width = s.out_w;
    r->height = s.out_h;
    r->stride = s.d.out_stride;
    r->colorimetry = s.colorimetry;
    r->copy_ts = s.fly[h].copy_ts;
    /* The frame is the one queued first (in order, tag = ordinal); any other is a driver fault. */
    r->status = (driver_seq == s.fly[h].driver_seq && meta->tag == (uint32_t)s.fly[h].ordinal) ? 0u : 1u;
    fx1_release();
    r->seq = ++s.publish_seq;
    s.slot_seq[index] = s.publish_seq;
    fx1_release();
    r->state = FX1_ISP_STREAM_REC_PUBLISHED;
    ++s.completed;
    put64(&STATUS->completed_lo, s.completed);
    STATUS->publish_seq = s.publish_seq;
    STATUS->elapsed_ticks = (uint32_t)(fx1_mtime() - s.t_first);
    s.fly_head = (h + 1u) % FX1_ISP_STREAM_SLOTS;
    --s.fly_count;
    ring(s.publish_seq);
}

/* Publishes every completed frame: the number collected, or -1 (fatal). */
static int collect(void)
{
    int got = 0;
    for (;;) {
        unsigned index = 0;
        uint32_t seq = 0;
        fx1_isp_frame_meta meta;
        taskENTER_CRITICAL();
        const int rc = fx1_isp_next_done_meta(&dev, &index, &seq, &meta);
        taskEXIT_CRITICAL();
        if (rc != FX1_ISP_OK) break;
        if (!s.fly_count) return -1;
        publish(index, seq, &meta);
        ++got;
        if (!s.lsc_done) { /* ISP-UPD-001: after the first SOF, as `isp run` */
            uint32_t filled = 0;
            int lrc = FX1_ISP_OK;
            taskENTER_CRITICAL();
            (void)isp_demo_lsc_fill(&dev, PROFILE, s.d.profile_count, &filled, &lrc);
            taskEXIT_CRITICAL();
            s.lsc_done = 1;
            if (lrc != FX1_ISP_OK) return -1;
        }
    }
    return got;
}

/* The host's (or a local) stop request: FX1_ISP_STREAM_REASON_*, 0 for none. */
static uint32_t stop_request(void)
{
    const volatile fx1_isp_stream_control* c = CONTROL;
    fx1_acquire();
    if (c->magic == FX1_ISP_STREAM_CONTROL_MAGIC && c->request == FX1_ISP_STREAM_REQ_STOP &&
        c->session_lo == s.d.session_lo && c->session_hi == s.d.session_hi)
        return c->reason ? c->reason : FX1_ISP_STREAM_REASON_USER;
    return local_stop ? FX1_ISP_STREAM_REASON_USER : 0u;
}

/* The ISP's frame counters when the session's ISP work began (run_session). */
static fx1_isp_progress initial;

/* fx1_isp_stop, retried from the task while its recovery drains: 1 once
 * stopped (the buffers back, CONFIG state), 0 if not within `limit` ticks
 * (portMAX_DELAY: no limit). The ISP interrupt is masked from the first
 * attempt on and stays masked. Each attempt polls BUSY only for the driver's
 * bounded wait; between attempts the task yields. */
static int stop_isp(TickType_t limit)
{
    const TickType_t t0 = xTaskGetTickCount();
    for (;;) {
        taskENTER_CRITICAL();
        mask_isp_irq();
        const int rc = fx1_isp_stop(&dev);
        taskEXIT_CRITICAL();
        if (rc >= 0 || rc == FX1_ISP_ESTATE) return 1; /* the frames dropped, or not streaming */
        if (limit != portMAX_DELAY && xTaskGetTickCount() - t0 >= limit) return 0;
        vTaskDelay(1);
    }
}

/* After the stop: the interrupt state acknowledged, legacy mode for the other
 * commands, the source unmasked, then the ISP released. A request the PLIC
 * latched while masked is taken at the unmask by this owner's handler (it
 * finds nothing), before another owner's. */
static void release_isp(void)
{
    fx1_isp_events ev;
    taskENTER_CRITICAL();
    fx1_isp_irq(&dev, &ev);
    taskEXIT_CRITICAL();
    (void)fx1_isp_profile_mode(&dev, 0);
    unmask_isp_irq();
    vTaskDelay(1);
    isp_release(ISP_OWNER_STREAM);
}

static void end_session(uint32_t state, uint32_t reason, uint32_t error, uint32_t detail)
{
    fx1_isp_progress p = {0};
    fx1_isp_get_progress(&dev, &p);
    /* One frame budget for the stop; a clean end needs a stopped ISP. */
    const int stopped = stop_isp(us_ticks(s.d.frame_budget_us));
    if (!stopped && state == FX1_ISP_STREAM_STOPPED) {
        state = FX1_ISP_STREAM_ERROR;
        reason = FX1_ISP_STREAM_STOP_ERROR;
        error = FX1_ISP_STREAM_ERR_TIMEOUT;
        detail = (uint32_t)s.completed;
    }
    /* Every end (DONE, USER, LIMIT and each ERROR) reports what the ISP took in and
     * wrote out in this session, read before the ISP was stopped. */
    STATUS->idma_frames = p.idma_frames - initial.idma_frames;
    STATUS->odma_frames = p.odma_frames - initial.odma_frames;
    STATUS->dma_err = p.dma_err;
    STATUS->stop_reason = reason;
    STATUS->error = error;
    STATUS->detail = detail;
    STATUS->failed = (uint32_t)(s.submitted - s.completed);
    if (!(s.d.flags & FX1_ISP_STREAM_FLAG_LOOP) && s.frames > s.submitted && state == FX1_ISP_STREAM_STOPPED)
        STATUS->cancelled = (uint32_t)(s.frames - s.submitted);
    STATUS->isp_drain = stopped ? FX1_ISP_STREAM_DRAIN_NONE : FX1_ISP_STREAM_DRAIN_PENDING;
    if (!stopped) isp_held = 1; /* before the terminal state: no start in between */
    fx1_write32(CH(FX1_DMA_CH_ENABLE), 0);
    fx1_release();
    set_state(state);
    ring(0xFFFFFFFFu);
    if (stopped) {
        release_isp();
        active = 0;
        return;
    }
    /* The stop still drains: the session has ended (the CLI and the timers
     * run), the ISP, its buffers and its masked interrupt stay the stream's
     * until the stop has finished. */
    active = 0;
    (void)stop_isp(portMAX_DELAY);
    release_isp();
    STATUS->isp_drain = FX1_ISP_STREAM_DRAIN_DONE;
    fx1_release();
    isp_held = 0;
}

static void fail(uint32_t error, uint32_t detail)
{
    end_session(FX1_ISP_STREAM_ERROR, FX1_ISP_STREAM_STOP_ERROR, error, detail);
}

/* ---- the session (stream task) --------------------------------------------------- */
static void run_session(void)
{
    const int loop = (s.d.flags & FX1_ISP_STREAM_FLAG_LOOP) != 0;
    const TickType_t budget = us_ticks(s.d.frame_budget_us);
    memset(&initial, 0, sizeof(initial));
    fx1_isp_get_progress(&dev, &initial);
    uint32_t reason = 0;
    TickType_t last = xTaskGetTickCount(); /* last progress (a queue, a completion, an ACK) */
    set_state(FX1_ISP_STREAM_RUNNING);
    for (;;) {
        if (isr_fatal) return fail(FX1_ISP_STREAM_ERR_ISP, isr_fatal);
        const int got = collect();
        if (got < 0) return fail(FX1_ISP_STREAM_ERR_ISP, 0xFFFFFFFFu);
        if (got) last = xTaskGetTickCount();
        if (!reason) {
            reason = stop_request();
            if (!reason && s.publish_seq >= 0xFFFFFFF0u) reason = 0x100u | LIMIT_SEQ; /* never wrap seq */
            /* The spool budget is full once every admitted frame is submitted: the
             * frames in flight still fit, no later one is queued. */
            if (!reason && s.submitted >= s.admit && (loop || s.submitted < s.frames))
                reason = FX1_ISP_STREAM_REASON_LIMIT;
            if (reason) set_state(FX1_ISP_STREAM_DRAINING);
            if (reason == FX1_ISP_STREAM_REASON_HOST) return fail(FX1_ISP_STREAM_ERR_HOST, FX1_ISP_STREAM_HOST_STOP);
        }
        const int more = !reason && (loop || s.submitted < s.frames);
        if (!more && !s.fly_count) break; /* schedule done, or stopped and drained */
        if (more && s.fly_count < s.d.queue) {
            const unsigned out = dev.out_next; /* the output slot this queue uses */
            taskENTER_CRITICAL();
            const int ready = fx1_isp_input_ready(&dev);
            taskEXIT_CRITICAL();
            if (ready && slot_acked(out)) {
                const uint64_t n = s.submitted;
                const uint32_t src = (uint32_t)((n / s.d.repeat) % s.d.source_count);
                const uint32_t base = s.d.source_base + src * s.d.source_pitch;
                int rc;
                uint32_t seq = 0, copy_ts = 0;
                do { /* EAGAIN: a recovery restarted the rotation: fill the new next input */
                    taskENTER_CRITICAL();
                    const unsigned k = fx1_isp_next_input(&dev);
                    taskEXIT_CRITICAL();
                    copy_ts = fx1_isp_frame_counter(&dev);
                    const uint32_t st = copy_frame(k, base);
                    if (st) return fail(FX1_ISP_STREAM_ERR_COPY, st);
                    fx1_dma_prepare((const volatile void*)(uintptr_t)FX1_ISP_STREAM_OUT_ADDR(out),
                                    s.uv_offset + s.d.out_stride * s.out_h / 2u, FX1_DMA_FROM_DEVICE);
                    taskENTER_CRITICAL();
                    rc = fx1_isp_queue_frame_meta(&dev, (uint32_t)n, fx1_isp_frame_counter(&dev), &seq);
                    taskEXIT_CRITICAL();
                } while (rc == FX1_ISP_EAGAIN);
                if (rc != FX1_ISP_OK) return fail(FX1_ISP_STREAM_ERR_CONFIG, (uint32_t)rc);
                const unsigned t = (s.fly_head + s.fly_count) % FX1_ISP_STREAM_SLOTS;
                s.fly[t].ordinal = n;
                s.fly[t].driver_seq = seq;
                s.fly[t].source = src;
                s.fly[t].repeat = (uint32_t)(n % s.d.repeat);
                s.fly[t].copy_ts = copy_ts;
                ++s.fly_count;
                if (!s.submitted) s.t_first = fx1_mtime();
                ++s.submitted;
                put64(&STATUS->submitted_lo, s.submitted);
                last = xTaskGetTickCount();
                continue;
            }
        }
        /* Nothing to submit now: a completion, the host's ACK or a free input is
         * awaited. Without a frame in flight only the host can make progress. */
        if (xSemaphoreTake(isp_done, 1) != pdTRUE && xTaskGetTickCount() - last > budget)
            return s.fly_count ? fail(FX1_ISP_STREAM_ERR_TIMEOUT, (uint32_t)s.completed)
                               : fail(FX1_ISP_STREAM_ERR_HOST, FX1_ISP_STREAM_HOST_NO_ACK);
    }
    /* Every published frame acknowledged before the session ends. */
    for (unsigned i = 0; i < FX1_ISP_STREAM_SLOTS; ++i)
        while (!slot_acked(i)) {
            if (xTaskGetTickCount() - last > budget) return fail(FX1_ISP_STREAM_ERR_HOST, FX1_ISP_STREAM_HOST_FINAL);
            vTaskDelay(1);
        }
    const uint32_t stop = !reason                                ? FX1_ISP_STREAM_STOP_DONE
                          : reason == FX1_ISP_STREAM_REASON_USER ? FX1_ISP_STREAM_STOP_USER
                                                                 : FX1_ISP_STREAM_STOP_LIMIT;
    end_session(FX1_ISP_STREAM_STOPPED, stop, 0,
                stop == FX1_ISP_STREAM_STOP_LIMIT ? (reason & 0x100u ? LIMIT_SEQ : LIMIT_SPOOL) : 0u);
}

static void stream_task(void* arg)
{
    (void)arg;
    for (;;) {
        xSemaphoreTake(start_req, portMAX_DELAY);
        run_session();
    }
}

void isp_stream_service_init(void)
{
    isp_done = xSemaphoreCreateBinary();
    dma_done = xSemaphoreCreateBinary();
    start_req = xSemaphoreCreateBinary();
    configASSERT(isp_done && dma_done && start_req);
    configASSERT(xTaskCreate(stream_task, "isp-stream", 1024, NULL, 2, NULL) == pdPASS);
}

/* ---- commands ------------------------------------------------------------------- */
static void start_error(uint32_t error, uint32_t detail, const char* why)
{
    STATUS->state = FX1_ISP_STREAM_ERROR;
    STATUS->stop_reason = FX1_ISP_STREAM_STOP_ERROR;
    STATUS->error = error;
    STATUS->detail = detail;
    fx1_release();
    out("isp stream start: ");
    out(why);
    out(" (detail ");
    out_hex(detail);
    out_line(")");
}

static void start(void)
{
    if (active) {
        out_line("isp stream start: a session is running (isp stream stop / wait first)");
        return;
    }
    if (isp_held) { /* the status block is still the last session's */
        out_line("isp stream start: the last session's ISP stop still drains (isp stream idle first)");
        report("isp stream start", 0);
        return;
    }
    memcpy(&s.d, (const void*)DESC, sizeof(s.d));
    fx1_acquire();
    const uint32_t bad = check_desc(&s.d);
    if (bad) {
        out("isp stream start: descriptor word ");
        out_dec(bad - 1u);
        out_line(" invalid (prepare the session with isp_stream_prep.py)");
        report("isp stream start", 0);
        return;
    }
    if (STATUS->magic == FX1_ISP_STREAM_STATUS_MAGIC && STATUS->session_lo == s.d.session_lo &&
        STATUS->session_hi == s.d.session_hi) {
        out_line("isp stream start: this session already ran (a session runs once: prepare a new one)");
        report("isp stream start", 0);
        return;
    }
    volatile fx1_isp_stream_status* st = STATUS;
    volatile uint32_t* w = (volatile uint32_t*)st;
    for (unsigned k = 0; k < sizeof(*st) / 4u; ++k) w[k] = 0;
    st->session_lo = s.d.session_lo;
    st->session_hi = s.d.session_hi;
    st->version = FX1_ISP_STREAM_VERSION;
    st->state = FX1_ISP_STREAM_IDLE;
    fx1_release();
    st->magic = FX1_ISP_STREAM_STATUS_MAGIC;
    uint32_t bell = 0;
    if (fx1_probe_read32(FX1_SIM_CTRL_BASE + FX1_SIM_CTRL_DOORBELL_OFF, &bell) ||
        !(bell & FX1_SIM_CTRL_DOORBELL_ATTACHED)) {
        start_error(FX1_ISP_STREAM_ERR_HOST, bell, "no host bridge attached (fx1_soc --isp-stream-spool): frames would be lost");
        report("isp stream start", 0);
        return;
    }
    /* The bridge attaches the session it found in DDR before the CPUs ran: the
     * control block names it and carries the spool budget. */
    const volatile fx1_isp_stream_control* c = CONTROL;
    fx1_acquire();
    if (c->magic != FX1_ISP_STREAM_CONTROL_MAGIC || c->version != FX1_ISP_STREAM_VERSION ||
        c->session_lo != s.d.session_lo || c->session_hi != s.d.session_hi) {
        start_error(FX1_ISP_STREAM_ERR_HOST, FX1_ISP_STREAM_HOST_DETACHED,
                    "the host bridge did not attach this session (control block)");
        report("isp stream start", 0);
        return;
    }
    const uint64_t max_bytes = ((uint64_t)c->max_bytes_hi << 32) | c->max_bytes_lo;
    if (!isp_claim(ISP_OWNER_STREAM)) {
        start_error(FX1_ISP_STREAM_ERR_BUSY, 0, "the ISP is in use (camera stop first)");
        report("isp stream start", 0);
        return;
    }
    s.session = ((uint64_t)s.d.session_hi << 32) | s.d.session_lo;
    s.frames = ((uint64_t)s.d.frames_hi << 32) | s.d.frames_lo;
    s.submitted = s.completed = 0;
    s.publish_seq = 0;
    memset(s.slot_seq, 0, sizeof(s.slot_seq));
    s.fly_head = s.fly_count = 0;
    s.lsc_done = 0;
    s.t_first = 0;
    s.chain_cmds = (s.d.frame_bytes + FX1_ISP_STREAM_DMA_CHUNK - 1u) / FX1_ISP_STREAM_DMA_CHUNK;
    s.uv_offset = FX1_ISP_STREAM_UV_OFFSET(s.d.out_stride * active_h(&s.d));
    s.profile_id = s.d.profile_id ? s.d.profile_id : 1u;
    local_stop = 0;
    isr_fatal = 0;
    xSemaphoreTake(isp_done, 0);
    init_records();
    build_chains();

    /* As `isp run` (isp_demo_configure): geometry, the profile in legacy mode,
     * CH0 only, then in profile mode the profile built into bank 0. */
    uint32_t tune_error = 0;
    int rc = fx1_isp_init(&dev, &isp_hal);
    if (rc == FX1_ISP_OK) rc = fx1_isp_profile_mode(&dev, 0);
    if (rc == FX1_ISP_OK) rc = fx1_isp_set_geometry(&dev, s.d.width, s.d.height, s.d.bayer);
    if (rc != FX1_ISP_OK) {
        start_error(FX1_ISP_STREAM_ERR_CONFIG, (uint32_t)rc, "geometry refused by the driver");
        isp_release(ISP_OWNER_STREAM);
        report("isp stream start", 0);
        return;
    }
    rc = fx1_isp_apply(&dev, PROFILE, s.d.profile_count);
    if (rc == FX1_ISP_OK) {
        const fx1_isp_reg_write ch0 = {FX1_ISP_OUTPUT_ENABLE_OFFSET, 1u};
        fx1_isp_apply(&dev, &ch0, 1);
        if (s.d.mode == FX1_ISP_STREAM_MODE_PROFILE)
            rc = fx1_isp_profile_from_legacy(&dev, 0, PROFILE, s.d.profile_count, s.profile_id, &tune_error);
    }
    if (rc != FX1_ISP_OK) {
        start_error(FX1_ISP_STREAM_ERR_PROFILE, tune_error ? tune_error : (uint32_t)rc,
                    "profile refused (TUNE_ERROR, or the driver code)");
        (void)fx1_isp_profile_mode(&dev, 0);
        isp_release(ISP_OWNER_STREAM);
        report("isp stream start", 0);
        return;
    }
    fx1_isp_buffers bufs;
    for (unsigned i = 0; i < FX1_ISP_NUM_BUFFERS; ++i) {
        bufs.in[i] = FX1_ISP_STREAM_IN_ADDR(i);
        bufs.y[i] = FX1_ISP_STREAM_OUT_ADDR(i);
        bufs.uv[i] = FX1_ISP_STREAM_OUT_ADDR(i) + s.uv_offset;
    }
    bufs.in_stride = s.d.in_stride;
    bufs.y_stride = bufs.uv_stride = s.d.out_stride;
    bufs.max_burst_beats = 16;
    s.colorimetry = isp_demo_colorimetry_word(&dev); /* the committed configuration, fixed for the session */
    rc = fx1_isp_start(&dev, &bufs);
    if (rc != FX1_ISP_OK || !dev.out_width || !dev.out_height || dev.out_width > s.d.out_stride ||
        dev.out_height > active_h(&s.d)) {
        start_error(FX1_ISP_STREAM_ERR_CONFIG, (uint32_t)rc, "start refused (CH0 geometry not valid?)");
        taskENTER_CRITICAL();
        (void)fx1_isp_stop(&dev);
        taskEXIT_CRITICAL();
        (void)fx1_isp_profile_mode(&dev, 0);
        isp_release(ISP_OWNER_STREAM);
        report("isp stream start", 0);
        return;
    }
    s.out_w = dev.out_width;
    s.out_h = dev.out_height;
    s.admit = max_bytes ? max_bytes / ((uint64_t)s.out_w * s.out_h * 3u / 2u) : UINT64_MAX;
    if (!s.admit) {
        start_error(FX1_ISP_STREAM_ERR_HOST, FX1_ISP_STREAM_HOST_BUDGET,
                    "the spool budget (--isp-stream-max-mib) holds no frame of this output");
        taskENTER_CRITICAL();
        (void)fx1_isp_stop(&dev);
        taskEXIT_CRITICAL();
        (void)fx1_isp_profile_mode(&dev, 0);
        isp_release(ISP_OWNER_STREAM);
        report("isp stream start", 0);
        return;
    }
    st->out_width = s.out_w;
    st->out_height = s.out_h;
    st->out_stride = s.d.out_stride;
    st->colorimetry = s.colorimetry;
    fx1_release();
    st->state = FX1_ISP_STREAM_PREPARED;
    active = 1;
    out("isp stream start: session ");
    for (int b = 60; b >= 0; b -= 4) {
        const char hexd[] = "0123456789abcdef";
        const char c[2] = {hexd[(s.session >> b) & 15u], 0};
        out(c);
    }
    out(", ");
    out_dec(s.d.source_count);
    out(" source(s) ");
    out_dec(s.d.width);
    out("x");
    out_dec(s.d.height);
    out(" -> ");
    out_dec(s.out_w);
    out("x");
    out_dec(s.out_h);
    out(" NV12, repeat ");
    out_dec(s.d.repeat);
    if (s.d.flags & FX1_ISP_STREAM_FLAG_LOOP) {
        out(", loop");
    } else {
        out(", ");
        out_dec((uint32_t)s.frames);
        out(" frame(s)");
    }
    out(", queue ");
    out_dec(s.d.queue);
    out(s.d.mode == FX1_ISP_STREAM_MODE_PROFILE ? ", profile mode (ID " : ", legacy mode");
    if (s.d.mode == FX1_ISP_STREAM_MODE_PROFILE) {
        out_dec(s.profile_id);
        out(")");
    }
    if (max_bytes) {
        out(", spool budget ");
        out_dec(s.admit > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)s.admit);
        out(" frame(s)");
    }
    out_line("");
    xSemaphoreGive(start_req);
}

static const char* const state_names[] = {"IDLE", "PREPARED", "RUNNING", "DRAINING", "STOPPED", "ERROR"};
static const char* const stop_names[] = {"NONE", "DONE", "USER", "LIMIT", "ERROR"};
static const char* const error_names[] = {"NONE", "DESC", "BUSY", "PROFILE", "CONFIG", "COPY", "ISP", "TIMEOUT", "HOST"};

static void status(void)
{
    const volatile fx1_isp_stream_status* st = STATUS;
    fx1_acquire();
    if (st->magic != FX1_ISP_STREAM_STATUS_MAGIC) {
        out_line("isp stream: no session (isp stream start)");
        return;
    }
    const uint32_t state = st->state < 6u ? st->state : 5u;
    out("isp stream: ");
    out(state_names[state]);
    if (state == FX1_ISP_STREAM_STOPPED) {
        out(" (");
        out(stop_names[st->stop_reason < 5u ? st->stop_reason : 4u]);
        out(")");
    } else if (state == FX1_ISP_STREAM_ERROR) {
        out(" (");
        out(error_names[st->error < 9u ? st->error : 0u]);
        out(", detail ");
        out_hex(st->detail);
        out(")");
    }
    out(", submitted ");
    out_dec(st->submitted_lo);
    out(", published ");
    out_dec(st->completed_lo);
    out(", cancelled ");
    out_dec(st->cancelled);
    out(", failed ");
    out_dec(st->failed);
    out(", out ");
    out_dec(st->out_width);
    out("x");
    out_dec(st->out_height);
    out(", DMA_ERR ");
    out_hex(st->dma_err);
    out(", ISP frames in ");
    out_dec(st->idma_frames);
    out(" out ");
    out_dec(st->odma_frames);
    out(", ");
    out_dec(st->elapsed_ticks / (FX1_MTIME_HZ / 1000u));
    out(" ms");
    if (st->isp_drain == FX1_ISP_STREAM_DRAIN_PENDING)
        out(", ISP stop draining (the ISP held by the stream)");
    else if (st->isp_drain == FX1_ISP_STREAM_DRAIN_DONE)
        out(", ISP stopped after the end (released)");
    out_line("");
}

static void wait_done(uint32_t limit_ms)
{
    const TickType_t t0 = xTaskGetTickCount();
    while (active && (!limit_ms || xTaskGetTickCount() - t0 < pdMS_TO_TICKS(limit_ms))) vTaskDelay(pdMS_TO_TICKS(1));
    status();
    const uint32_t state = STATUS->state;
    if (active) {
        out_line("isp stream wait: still running at the limit");
        report("isp stream", 0);
        return;
    }
    report("isp stream", STATUS->magic == FX1_ISP_STREAM_STATUS_MAGIC && state == FX1_ISP_STREAM_STOPPED);
}

static void wait_idle(uint32_t limit_ms)
{
    const TickType_t t0 = xTaskGetTickCount();
    while ((active || isp_held) && (!limit_ms || xTaskGetTickCount() - t0 < pdMS_TO_TICKS(limit_ms)))
        vTaskDelay(pdMS_TO_TICKS(1));
    status();
    if (active || isp_held) out_line(active ? "isp stream idle: a session still runs at the limit"
                                            : "isp stream idle: the ISP stop still drains at the limit");
    report("isp stream idle", !active && !isp_held);
}

int cmd_isp_stream(int argc, char** argv)
{
    if (argc == 2 && !strcmp(argv[1], "start")) {
        start();
    } else if (argc == 2 && !strcmp(argv[1], "status")) {
        status();
    } else if (argc == 2 && !strcmp(argv[1], "stop")) {
        if (active) {
            local_stop = 1;
            out_line("isp stream stop: requested (the frames in flight complete)");
        } else {
            out_line("isp stream stop: not running");
        }
    } else if ((argc == 2 || argc == 3) && !strcmp(argv[1], "wait")) {
        uint32_t ms = 0;
        if (argc == 3 && !parse_u32(argv[2], &ms)) return 1;
        wait_done(ms);
    } else if ((argc == 2 || argc == 3) && !strcmp(argv[1], "idle")) {
        uint32_t ms = 0;
        if (argc == 3 && !parse_u32(argv[2], &ms)) return 1;
        wait_idle(ms);
    } else {
        return 1;
    }
    return 0;
}
