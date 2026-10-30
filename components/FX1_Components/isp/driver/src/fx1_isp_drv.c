/* SPDX-License-Identifier: Apache-2.0 */
/* FX1 ISP reference driver; see fx1_isp_drv.h and docs/ISP_PROGRAMMING_GUIDE.md. */

#include "fx1_isp/fx1_isp_drv.h"

#include "fx1_isp/fx1_isp_csr.h"

#define RD(dev, off) ((dev)->hal.read32((dev)->hal.ctx, (off)))
#define WR(dev, off, v) ((dev)->hal.write32((dev)->hal.ctx, (off), (v)))

#define MAX_WIDTH 3842u      /* PARA_MAX_H_ACTIVE: external, incl. phase overscan (MAS v1.07) */
#define MAX_HEIGHT 2162u     /* PARA_MAX_V_ACTIVE */
#define MAX_ACTIVE_W 3840u   /* derived active image (MAS v1.07 §6.6.3.4) */
#define MAX_ACTIVE_H 2160u
#define MIN_ACTIVE 32u
#define LSC_MESH_MAX 32u     /* nodes per axis (MAS v1.07 §6.8.4, V107-A2) */
/* The Resizer derives its configuration from a stable key in 35 core clocks
 * (MAS §6.20.4.3, Table 123) and only then publishes RESIZER_OUT_W/H; a frame
 * started earlier uses the previous set. A key changed while a derivation runs
 * is derived after it, so publication can take two derivations after the last
 * change: 2 x 36 edges, plus margin (review U4-R1). RESIZER_OUT_W/H alone
 * cannot tell a finished derivation (a new key may give the same size), so the
 * driver waits this long before reading them or starting a frame. */
#define RESIZER_SETTLE_CYCLES 80u
#define DRV_MAGIC 0x46583149u /* "FX1I": set by fx1_isp_init */

static int initialised(const fx1_isp_dev *dev) { return dev && dev->magic == DRV_MAGIC; }

/* Output channel registers (MAS v1.07 §6.25.3.8, Table 6-159); CH2 has one
 * plane (uv_* = 0). Per-buffer words: address + 16 k, metadata + 4 k. */
typedef struct out_regs {
    uint32_t free, done, y_addr, uv_addr, y_stride, uv_stride, tag, ts, sof, eof, frame, profile, out_w;
} out_regs;
static const out_regs out_ch[3] = {
    {FX1_ISP_ODMA_BUF_FREE_OFFSET, FX1_ISP_ODMA_BUF_DONE_OFFSET, FX1_ISP_ODMA_Y_ADDR_L0_OFFSET,
     FX1_ISP_ODMA_UV_ADDR_L0_OFFSET, FX1_ISP_ODMA_Y_STRIDE_OFFSET, FX1_ISP_ODMA_UV_STRIDE_OFFSET,
     FX1_ISP_ODMA_BUF_TAG0_OFFSET, FX1_ISP_ODMA_BUF_TS0_OFFSET, FX1_ISP_ODMA_BUF_SOF_TS0_OFFSET,
     FX1_ISP_ODMA_BUF_EOF_TS0_OFFSET, FX1_ISP_TUNE_CH0_BUF0_FRAME_OFFSET, FX1_ISP_TUNE_CH0_BUF0_PROFILE_OFFSET,
     FX1_ISP_RESIZER_OUT_W_OFFSET},
    {FX1_ISP_CH1_ODMA_BUF_FREE_OFFSET, FX1_ISP_CH1_ODMA_BUF_DONE_OFFSET, FX1_ISP_CH1_ODMA_Y_ADDR_L0_OFFSET,
     FX1_ISP_CH1_ODMA_UV_ADDR_L0_OFFSET, FX1_ISP_CH1_ODMA_Y_STRIDE_OFFSET, FX1_ISP_CH1_ODMA_UV_STRIDE_OFFSET,
     FX1_ISP_CH1_ODMA_BUF_TAG0_OFFSET, FX1_ISP_CH1_ODMA_BUF_TS0_OFFSET, FX1_ISP_CH1_ODMA_BUF_SOF_TS0_OFFSET,
     FX1_ISP_CH1_ODMA_BUF_EOF_TS0_OFFSET, FX1_ISP_TUNE_CH1_BUF0_FRAME_OFFSET, FX1_ISP_TUNE_CH1_BUF0_PROFILE_OFFSET,
     FX1_ISP_CH1_RESIZER_OUT_W_OFFSET},
    {FX1_ISP_CH2_ODMA_BUF_FREE_OFFSET, FX1_ISP_CH2_ODMA_BUF_DONE_OFFSET, FX1_ISP_CH2_ODMA_Y_ADDR_L0_OFFSET, 0u,
     FX1_ISP_CH2_ODMA_Y_STRIDE_OFFSET, 0u, FX1_ISP_CH2_ODMA_BUF_TAG0_OFFSET, FX1_ISP_CH2_ODMA_BUF_TS0_OFFSET,
     FX1_ISP_CH2_ODMA_BUF_SOF_TS0_OFFSET, FX1_ISP_CH2_ODMA_BUF_EOF_TS0_OFFSET, FX1_ISP_TUNE_CH2_BUF0_FRAME_OFFSET,
     FX1_ISP_TUNE_CH2_BUF0_PROFILE_OFFSET, FX1_ISP_CH2_RESIZER_OUT_W_OFFSET},
};
static int streaming(const fx1_isp_dev *dev) { return initialised(dev) && dev->state == FX1_ISP_STATE_STREAMING; }

/* Removes queue entry k (0 = oldest). */
static fx1_isp_frame queue_remove(fx1_isp_dev *dev, unsigned k) {
    unsigned i;
    const fx1_isp_frame f = dev->queue[k];
    for (i = k; i + 1u < dev->in_flight; ++i) {
        dev->queue[i] = dev->queue[i + 1u];
    }
    --dev->in_flight;
    return f;
}

static void delay(fx1_isp_dev *dev, uint32_t cycles) {
    if (dev->hal.delay_cycles) {
        dev->hal.delay_cycles(dev->hal.ctx, cycles);
    }
}

/* The statistics result memories have synchronous read ports: a word is
 * valid one core clock after its address is selected (MAS §6.21.5.1 p237,
 * §6.22.4.3), and an AWB lease is readable one clock after ACQUIRE. A CSR
 * transaction is not guaranteed to last a core clock (csr_latency and the
 * core clock are independent), so the driver waits explicitly. */
#define RAM_READ_CYCLES 1u

static void select_word(fx1_isp_dev *dev, uint32_t addr_off, uint32_t addr) {
    WR(dev, addr_off, addr);
    delay(dev, RAM_READ_CYCLES);
}

static void write_addr(fx1_isp_dev *dev, uint32_t lo_off, uint64_t addr) {
    WR(dev, lo_off, (uint32_t)addr);
    WR(dev, lo_off + 4u, (uint32_t)(addr >> 32));
}

/* Rotation positions restart at 0 after i_rst_n, soft reset and every
 * recovery (M2-A1, MAS v1.07 §9.9.8). The frame counters are kept, so the
 * driver remembers where it started. */
static void restart_rotation(fx1_isp_dev *dev) {
    dev->in_next = dev->out_next = 0;
    ++dev->rotation_gen;
    dev->in_flight = 0;
    dev->lost_count = 0;
}

/* A recovery drains the AXI transactions already issued, then runs its
 * 15-clock pulse (MAS v1.07 §9.9.8): bounded poll for BUSY = 0. */
#define RECOVERY_POLL_CYCLES 16u
#define RECOVERY_POLLS 4096u

static int recovery_idle(fx1_isp_dev *dev) {
    unsigned i;
    for (i = 0; i < RECOVERY_POLLS; ++i) {
        if (!(RD(dev, FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET) & FX1_ISP_COMMON_RECOVERY_STATUS_BUSY_MASK)) {
            return 1;
        }
        delay(dev, RECOVERY_POLL_CYCLES);
    }
    return 0;
}

/* Requests a recovery (off/value: the soft-reset write) and waits, bounded,
 * for its final retirement. A request while another recovery runs is
 * rejected with TUNE_ERROR.PENDING and no new SEQUENCE, so a running one (an
 * AXI error) is waited out first and the request repeated if one started in
 * between. FX1_ISP_EBUSY when the drain outlasts the polling budget (MAS
 * v1.07 §9.9.8 sets no bound): an accepted request then stays pending
 * (dev->reset_pending), and the next call for the same register, or the
 * queue's settling, completes it; nothing is re-requested meanwhile. */
static int run_recovery(fx1_isp_dev *dev, uint32_t off, uint32_t value) {
    unsigned attempt;
    if (dev->reset_completed == off) {  /* a settling call already completed our pending request */
        dev->reset_completed = 0;
        return FX1_ISP_OK;
    }
    dev->reset_completed = 0;
    if (dev->reset_pending) {  /* an accepted request of ours is still running */
        if (!recovery_idle(dev)) {
            return FX1_ISP_EBUSY;
        }
        const int same = dev->reset_pending == off;
        dev->reset_pending = 0;
        dev->rec_seq = RD(dev, FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET);
        if (same) {
            return FX1_ISP_OK;
        }
    }
    for (attempt = 0; attempt < 2u; ++attempt) {
        if (!recovery_idle(dev)) {
            return FX1_ISP_EBUSY;  /* nothing issued */
        }
        const uint32_t seq = RD(dev, FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET);
        const uint32_t pending = RD(dev, FX1_ISP_TUNE_ERROR_OFFSET) & FX1_ISP_TUNE_ERROR_PENDING_MASK;
        WR(dev, off, value);
        /* Accepted on the write edge: SEQUENCE + 1 at once, PENDING otherwise. */
        const int rejected = (!pending && (RD(dev, FX1_ISP_TUNE_ERROR_OFFSET) & FX1_ISP_TUNE_ERROR_PENDING_MASK)) ||
                             RD(dev, FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET) == seq;
        if (!rejected) {
            if (!recovery_idle(dev)) {
                dev->reset_pending = off;
                return FX1_ISP_EBUSY;
            }
            dev->rec_seq = RD(dev, FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET);
            return FX1_ISP_OK;
        }
        if (!pending) {
            WR(dev, FX1_ISP_TUNE_ERROR_OFFSET, FX1_ISP_TUNE_ERROR_PENDING_MASK);
        }
    }
    return FX1_ISP_EBUSY;
}

static void common_reset_done(fx1_isp_dev *dev);
static void dma_reset_done(fx1_isp_dev *dev);

/* Output buffer still held by a frame the caller has not collected: after a
 * recovery the rotation restarts at 0 under completed, uncollected outputs. */
static int output_held(const fx1_isp_dev *dev, unsigned out) {
    unsigned k;
    for (k = 0; k < dev->in_flight; ++k) {
        if (dev->queue[k].output == out) {
            return 1;
        }
    }
    return 0;
}

/* Buffers that every enabled channel has published DONE for. */
static uint32_t outputs_done(fx1_isp_dev *dev) {
    uint32_t done = dev->outputs ? 0xFu : 0u;
    unsigned c;
    for (c = 0; c < 3u; ++c) {
        if (dev->outputs & (1u << c)) {
            done &= RD(dev, out_ch[c].done);
        }
    }
    return done;
}

/* Returns buffer `index` of every enabled channel to software (DONE W1C).
 * A W1C on the core clock on which the hardware set the bit loses (hardware
 * set wins): possible when CSR accesses are shorter than a core clock. The
 * ODMA never clears DONE, and this buffer is not free, so a bit still set is
 * that same completion: clear it again on a later clock. Otherwise a stale
 * DONE would make the buffer look complete when it is queued again (review
 * U6-R3). */
/* A recovery that starts in between drops the W1C (its command block): 0
 * then, and the caller keeps the buffer's bookkeeping until a later call
 * acknowledges it (review V107-7..10 R2). */
static int ack_done(fx1_isp_dev *dev, unsigned index) {
    unsigned c;
    int ok = 1;
    for (c = 0; c < 3u; ++c) {
        if (dev->outputs & (1u << c)) {
            WR(dev, out_ch[c].done, 1u << index);
            if (RD(dev, out_ch[c].done) & (1u << index)) {
                delay(dev, 1u);
                WR(dev, out_ch[c].done, 1u << index);
                ok = ok && !(RD(dev, out_ch[c].done) & (1u << index));
            }
        }
    }
    return ok;
}

/* DONE bits of buffers that left the queue but could not be acknowledged:
 * retried before such a buffer is armed again. */
static void flush_pending_ack(fx1_isp_dev *dev) {
    unsigned k;
    for (k = 0; k < FX1_ISP_NUM_BUFFERS; ++k) {
        if ((dev->pending_ack & (1u << k)) && ack_done(dev, k)) {
            dev->pending_ack &= ~(1u << k);
        }
    }
}

/* Settles the queue against a recovery the driver did not request (an AXI
 * error, MAS v1.07 §9.9.8): at its final retirement every frame without DONE
 * was aborted, every VALID / FREE bit cleared and both rotations restarted
 * at 0; a completed output keeps DONE and stays collectable. Frames complete
 * in order, so the kept ones are a prefix of the queue. SEQUENCE is read
 * before BUSY: a recovery accepted after the BUSY read has a later SEQUENCE
 * and is settled by a later call. FX1_ISP_EBUSY while one is still running. */
static int settle_recovery(fx1_isp_dev *dev) {
    unsigned k, n = 0;
    const uint32_t seq = RD(dev, FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET);
    if (seq == dev->rec_seq) {
        return FX1_ISP_OK;
    }
    if (RD(dev, FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET) & FX1_ISP_COMMON_RECOVERY_STATUS_BUSY_MASK) {
        return FX1_ISP_EBUSY;
    }
    if (dev->reset_pending) {
        /* Our own reset that outlasted its call (run_recovery): completed
         * now, with the bookkeeping of that reset (every frame dropped). */
        const uint32_t off = dev->reset_pending;
        dev->reset_pending = 0;
        dev->reset_completed = off;  /* the retried reset call returns OK without a new request */
        dev->rec_seq = seq;
        if (off == FX1_ISP_COMMON_CTRL_OFFSET) {
            common_reset_done(dev);
        } else {
            dma_reset_done(dev);
        }
        return FX1_ISP_OK;
    }
    const uint32_t done = outputs_done(dev);
    while (n < dev->in_flight && (done & (1u << dev->queue[n].output))) {
        ++n;
    }
    dev->rec_lost = dev->in_flight - n;
    if (dev->rec_lost) {
        dev->rec_first = dev->queue[n];
    }
    for (k = n; k < dev->in_flight; ++k) {
        if (dev->lost_count < 4u) {
            dev->lost[dev->lost_count++] = dev->queue[k];
        }
        /* A channel that published before the abort keeps its DONE (DMA
         * scope): clear it, or the buffer looks complete when reused. */
        if (!ack_done(dev, dev->queue[k].output)) {
            dev->pending_ack |= 1u << dev->queue[k].output;
        }
    }
    dev->in_flight = n;
    dev->in_next = dev->out_next = 0;
    ++dev->rotation_gen; /* a slot read before this is stale (fx1_isp_queue_frame EAGAIN) */
    dev->rec_seq = seq;
    return FX1_ISP_OK;
}


int fx1_isp_init(fx1_isp_dev *dev, const fx1_isp_hal *hal) {
    if (!dev || !hal || !hal->read32 || !hal->write32 || !hal->delay_cycles) {
        return FX1_ISP_EINVAL;
    }
    dev->hal = *hal;
    dev->magic = DRV_MAGIC;
    dev->state = FX1_ISP_STATE_CONFIG;
    dev->out_width = dev->out_height = 0;
    dev->dma_ctrl = 0;
    dev->started = 0;
    dev->next_seq = 0;
    dev->gtm_en = 0;
    dev->outputs = 1u;  /* OUTPUT_ENABLE reset: CH0 */
    dev->ch_ready = 0;
    dev->reset_pending = dev->reset_completed = 0;
    dev->pending_ack = 0;
    dev->tune_error_prior = 0;
    dev->rotation_gen = dev->next_gen = 0;
    dev->next_read = 0;
    dev->rec_seq = RD(dev, FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET);
    /* FX1_ISP_EBUSY: initialised, but the reset is still running (call
     * fx1_isp_soft_reset_ex until it returns FX1_ISP_OK). */
    return fx1_isp_soft_reset_ex(dev);
}

int fx1_isp_stop(fx1_isp_dev *dev) {
    if (!streaming(dev)) {
        return FX1_ISP_ESTATE;
    }
    const int dropped = (int)dev->in_flight;
    /* Disable before resetting: if DMA_EN survives into the 15-cycle window,
     * IDMA sees no VALID buffer afterwards and reasserts UNDERRUN. */
    WR(dev, FX1_ISP_DMA_CTRL_OFFSET, dev->dma_ctrl & ~(FX1_ISP_DMA_CTRL_IDMA_EN_MASK | FX1_ISP_DMA_CTRL_ODMA_EN_MASK));
    /* Frames in flight are discarded (DEC-14). Until the reset has finished
     * the buffers are not back: EBUSY, still STREAMING; call again. */
    const int rc = fx1_isp_soft_reset_ex(dev);
    if (rc != FX1_ISP_OK) {
        return rc;
    }
    WR(dev, FX1_ISP_COMMON_CTRL_OFFSET, 0);  /* CONFIG state: pipeline disabled */
    dev->started = 0;
    dev->state = FX1_ISP_STATE_CONFIG;
    return dropped;
}

void fx1_isp_soft_reset(fx1_isp_dev *dev) { (void)fx1_isp_soft_reset_ex(dev); }

int fx1_isp_soft_reset_ex(fx1_isp_dev *dev) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    /* COMMON_CTRL.soft_rst is W1SC; isp_en is RW and written back unchanged. */
    const uint32_t ctrl = RD(dev, FX1_ISP_COMMON_CTRL_OFFSET) & FX1_ISP_COMMON_CTRL_ISP_EN_MASK;
    /* A common-scope recovery; W1S/W1C writes are ignored until its final
     * retirement (BUSY = 0). The driver's bookkeeping changes only once it
     * has finished (review V107-7..10 R6). */
    const int rc = run_recovery(dev, FX1_ISP_COMMON_CTRL_OFFSET, ctrl | FX1_ISP_COMMON_CTRL_SOFT_RST_MASK);
    if (rc != FX1_ISP_OK) {
        return rc;
    }
    /* The Resizer published set returned to its reset value and is derived
     * again after the pulse: no frame before it is published (MAS Table 124). */
    delay(dev, RESIZER_SETTLE_CYCLES);
    common_reset_done(dev);
    return FX1_ISP_OK;
}

/* Driver state after a completed common-scope reset. */
static void common_reset_done(fx1_isp_dev *dev) {
    WR(dev, FX1_ISP_COMMON_IRQ_STATUS_OFFSET, 0xFFFFFFFFu);
    WR(dev, FX1_ISP_DMA_IRQ_STAT_OFFSET, 0xFFFFFFFFu);
    WR(dev, FX1_ISP_DMA_ERR_OFFSET, 0xFFFFFFFFu);
    restart_rotation(dev);
    dev->pending_ack = 0;  /* the common scope cleared every DONE */
    /* The COMMON soft reset clears the ownership and results of every
     * statistics engine (MAS v1.07 Table 9-187). */
    dev->awb_leased = dev->aec_leased = dev->af_leased = 0;
    dev->awb_last_valid = dev->aec_last_valid = dev->af_last_valid = 0;
    /* The soft reset keeps the EE configuration: the selector as programmed. */
    dev->ee_sel = (RD(dev, FX1_ISP_EE_CTRL_OFFSET) & FX1_ISP_EE_CTRL_PROFILE_SEL_MASK) >> FX1_ISP_EE_CTRL_PROFILE_SEL_SHIFT;
    if (dev->ee_sel > 2u) {
        dev->ee_sel = 0;  /* code 3 selects profile 0 */
    }
    dev->ee_in_use = 1u << dev->ee_sel;
    dev->gtm_manual = dev->gtm_wait = 0;
}

int fx1_isp_dma_reset(fx1_isp_dev *dev) {
    if (!streaming(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (!dev->reset_pending) {
        (void)settle_recovery(dev);  /* frames an earlier recovery aborted are not in flight */
    }
    const int dropped = (int)dev->in_flight;
    /* DMA_CTRL.soft_reset is W1SC; the enables and burst length are RW and
     * written back unchanged (DEC-14). The frame in flight is discarded and the
     * pipeline flushed to a frame boundary; configuration, statistics results
     * and AWB ownership are kept (MAS Table 183). */
    /* The driver wrote DMA_CTRL itself at the first queue; reading it back
     * would only delay the reset of the frame in flight. */
    const uint32_t ctrl = dev->started ? dev->dma_ctrl : RD(dev, FX1_ISP_DMA_CTRL_OFFSET);
    /* A DMA-scope recovery (MAS v1.07 §9.9.8): it keeps DMA_ERR, DMA_IRQ_STAT
     * and the DONE bits of completed outputs. Every frame in flight is
     * discarded here, so those are cleared too. An aborted measurement
     * publishes nothing (MAS Table 9-198). */
    const int rc = run_recovery(dev, FX1_ISP_DMA_CTRL_OFFSET, ctrl | FX1_ISP_DMA_CTRL_SOFT_RESET_MASK);
    if (rc != FX1_ISP_OK) {
        return rc;  /* EBUSY: not finished; the queue is kept (review V107-7..10 R6) */
    }
    dma_reset_done(dev);
    return dropped;
}

/* Driver state after a completed DMA-scope reset. */
static void dma_reset_done(fx1_isp_dev *dev) {
    WR(dev, FX1_ISP_DMA_IRQ_STAT_OFFSET, 0xFFFFFFFFu);
    WR(dev, FX1_ISP_DMA_ERR_OFFSET, 0xFFFFFFFFu);
    WR(dev, FX1_ISP_ODMA_BUF_DONE_OFFSET, 0xFu);
    WR(dev, FX1_ISP_CH1_ODMA_BUF_DONE_OFFSET, 0xFu);
    WR(dev, FX1_ISP_CH2_ODMA_BUF_DONE_OFFSET, 0xFu);
    restart_rotation(dev);
    dev->pending_ack = 0;
    /* GTM hand-over (MAS §9.2 step 2): the frames counted at step 1 that the
     * reset dropped will never be taken. A dropped frame builds no curve
     * (MAS §9.1.1), so the next frame taken after the reset, queued after
     * step 1, is the one whose SOF consumes any pending curve. Cold as at
     * step 1: no frame taken since i_rst_n and nothing queued (review U6-R2). */
    if (dev->gtm_wait) {
        const uint32_t count = RD(dev, FX1_ISP_IDMA_FRAME_COUNT_OFFSET);
        if ((int32_t)(count - dev->gtm_target) < 0) {
            dev->gtm_target = count + 1u;
            dev->gtm_wait = count != 0u;
        }
    }
}

/* MAS v1.07 §9.9.3: an IDLE-class write is accepted only with the input DMA
 * stopped (or ISP_EN clear) and nothing in flight; otherwise the hardware
 * rejects it with TUNE_ERROR.MODE behind an OKAY response. */
static int idle_for_input_domain(fx1_isp_dev *dev) {
    const uint32_t stopped = !(RD(dev, FX1_ISP_DMA_CTRL_OFFSET) & FX1_ISP_DMA_CTRL_IDMA_EN_MASK) ||
                             !(RD(dev, FX1_ISP_COMMON_CTRL_OFFSET) & FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
    return stopped && !(RD(dev, FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) && !dev->in_flight;
}

/* Read-back after IDLE-class writes: a rejected write leaves the old value
 * and sets TUNE_ERROR.MODE, which is cleared here and reported as EBUSY. */
static int idle_write_result(fx1_isp_dev *dev, int took) {
    if (took) {
        return FX1_ISP_OK;
    }
    WR(dev, FX1_ISP_TUNE_ERROR_OFFSET, FX1_ISP_TUNE_ERROR_MODE_MASK);
    return FX1_ISP_EBUSY;
}

int fx1_isp_set_geometry(fx1_isp_dev *dev, uint32_t width, uint32_t height, uint32_t bayer) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (width < 2u || width > MAX_WIDTH || (width & 1u) || height < 2u || height > MAX_HEIGHT || (height & 1u) ||
        bayer > 3u) {
        return FX1_ISP_EINVAL;
    }
    {
        const uint32_t aw = (width - (bayer & 1u)) & ~1u, ah = (height - ((bayer >> 1) & 1u)) & ~1u;
        if (aw < MIN_ACTIVE || aw > MAX_ACTIVE_W || ah < MIN_ACTIVE || ah > MAX_ACTIVE_H) {
            return FX1_ISP_EINVAL; /* MAS v1.07 §6.6.3.5: the DMA does not check the active image */
        }
    }
    if (!idle_for_input_domain(dev)) {
        return FX1_ISP_EBUSY;
    }
    WR(dev, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, width);
    WR(dev, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, height);
    WR(dev, FX1_ISP_COMMON_BAYER_OFFSET, bayer);
    return idle_write_result(dev, (RD(dev, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET) & 0xFFFFu) == width &&
                                       (RD(dev, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET) & 0xFFFFu) == height &&
                                       (RD(dev, FX1_ISP_COMMON_BAYER_OFFSET) & 3u) == bayer);
}

/* Per-channel geometry registers: CH0 Resizer page 0x2000, CH1 0x3200,
 * CH2 0x3400; crop/status blocks 0x40 apart (CSR v1.07). */
static const uint32_t ch_resizer[3] = {FX1_ISP_RESIZER_CTRL_OFFSET, FX1_ISP_CH1_RESIZER_CTRL_OFFSET,
                                       FX1_ISP_CH2_RESIZER_CTRL_OFFSET};
static const uint32_t ch_crop[3] = {FX1_ISP_CH0_CROP_CTRL_OFFSET, FX1_ISP_CH1_CROP_CTRL_OFFSET,
                                    FX1_ISP_CH2_CROP_CTRL_OFFSET};
#define CROP_ORIGIN(c) ((c) + (FX1_ISP_CH0_CROP_ORIGIN_OFFSET - FX1_ISP_CH0_CROP_CTRL_OFFSET))
#define CROP_SIZE(c) ((c) + (FX1_ISP_CH0_CROP_SIZE_OFFSET - FX1_ISP_CH0_CROP_CTRL_OFFSET))
#define GEOMETRY_STATUS(c) ((c) + (FX1_ISP_CH0_GEOMETRY_STATUS_OFFSET - FX1_ISP_CH0_CROP_CTRL_OFFSET))

int fx1_isp_geometry_status(fx1_isp_dev *dev, unsigned ch, uint32_t *status) {
    unsigned guard;
    uint32_t st = 0;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (ch > 2u || !status) {
        return FX1_ISP_EINVAL;
    }
    /* A write lands on the next edge; BUSY then holds until publication. */
    delay(dev, 1u);
    for (guard = 0; guard <= RESIZER_SETTLE_CYCLES; ++guard) {
        st = RD(dev, GEOMETRY_STATUS(ch_crop[ch]));
        if (!(st & FX1_ISP_CH0_GEOMETRY_STATUS_BUSY_MASK)) {
            break;
        }
        delay(dev, 1u);
    }
    *status = st;
    return (st & FX1_ISP_CH0_GEOMETRY_STATUS_BUSY_MASK) ? FX1_ISP_EAGAIN : FX1_ISP_OK;  /* still deriving */
}

static int channel_result(fx1_isp_dev *dev, unsigned ch) {
    uint32_t st = 0;
    const int rc = fx1_isp_geometry_status(dev, ch, &st);
    if (rc != FX1_ISP_OK) {
        return rc;
    }
    return (st & FX1_ISP_CH0_GEOMETRY_STATUS_VALID_MASK) ? FX1_ISP_OK : FX1_ISP_EINVAL;
}

int fx1_isp_set_crop(fx1_isp_dev *dev, unsigned ch, int enable, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (ch > 2u || x > 0x1FFFu || y > 0x1FFFu || w > 0x1FFFu || h > 0x1FFFu) {
        return FX1_ISP_EINVAL;
    }
    if (!idle_for_input_domain(dev)) {
        return FX1_ISP_EBUSY;
    }
    const uint32_t c = ch_crop[ch];
    WR(dev, c, 0);  /* disabled: the full active image while origin and size change */
    WR(dev, CROP_ORIGIN(c), x | (y << FX1_ISP_CH0_CROP_ORIGIN_Y_SHIFT));
    WR(dev, CROP_SIZE(c), w | (h << FX1_ISP_CH0_CROP_SIZE_H_SHIFT));
    WR(dev, c, enable ? FX1_ISP_CH0_CROP_CTRL_ENABLE_MASK : 0u);
    if ((RD(dev, c) & FX1_ISP_CH0_CROP_CTRL_ENABLE_MASK) != (enable ? FX1_ISP_CH0_CROP_CTRL_ENABLE_MASK : 0u)) {
        return idle_write_result(dev, 0);
    }
    return channel_result(dev, ch);
}

int fx1_isp_set_resize(fx1_isp_dev *dev, unsigned ch, int enable, uint32_t scale) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (ch > 2u || scale > 31u) {
        return FX1_ISP_EINVAL;
    }
    if (!idle_for_input_domain(dev)) {
        return FX1_ISP_EBUSY;
    }
    const uint32_t v = (enable ? FX1_ISP_RESIZER_CTRL_EN_MASK : 0u) | (scale << FX1_ISP_RESIZER_CTRL_SCALE_SHIFT);
    WR(dev, ch_resizer[ch], v);
    if ((RD(dev, ch_resizer[ch]) & (FX1_ISP_RESIZER_CTRL_EN_MASK | FX1_ISP_RESIZER_CTRL_SCALE_MASK)) != v) {
        return idle_write_result(dev, 0);
    }
    return channel_result(dev, ch);
}

int fx1_isp_set_input_format(fx1_isp_dev *dev, uint32_t depth) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (depth > FX1_ISP_INPUT_RAW10) {
        return FX1_ISP_EINVAL; /* 2/3 are invalid encodings */
    }
    if (!idle_for_input_domain(dev)) {
        return FX1_ISP_EBUSY; /* IDLE update (MAS v1.07 Table 8-184) */
    }
    WR(dev, FX1_ISP_COMMON_INPUT_FORMAT_OFFSET, depth);
    return idle_write_result(dev, (RD(dev, FX1_ISP_COMMON_INPUT_FORMAT_OFFSET) & 3u) == depth);
}

size_t fx1_isp_raw_upper_bits(const uint16_t *samples, size_t count, uint32_t depth) {
    const uint16_t unused = depth == FX1_ISP_INPUT_RAW10 ? 0xFC00u : 0xF000u;
    size_t i, bad = 0;
    if (!samples) {
        return 0;
    }
    for (i = 0; i < count; ++i) {
        bad += (samples[i] & unused) != 0u;
    }
    return bad;
}

static void disable_for_load(fx1_isp_dev *dev, uint32_t ctrl, uint32_t en) {
    const uint32_t v = RD(dev, ctrl);
    if (v & en) {
        WR(dev, ctrl, v & ~en);
    }
}

int fx1_isp_apply(fx1_isp_dev *dev, const fx1_isp_reg_write *writes, size_t n) {
    size_t i;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (n && !writes) {
        return FX1_ISP_EINVAL;
    }
    if (RD(dev, FX1_ISP_TUNE_MODE_OFFSET) & FX1_ISP_TUNE_MODE_ENABLE_MASK) {
        for (i = 0; i < n; ++i) { /* profile mode: the writes land in the selected bank */
            WR(dev, writes[i].offset, writes[i].value);
        }
        return FX1_ISP_OK;
    }
    /* Legacy tables load and confirm only while their block is disabled (MAS
     * v1.07 §6.14.4 Gamma, §6.16.4.2 GTM manual curve, §9.9.1 EE). A profile
     * applied over an earlier configuration (camera reconfigure, a second
     * run) keeps EN from it: its loads would lose the confirmation, or leave
     * the old GTM curve. The block is disabled first when the profile loads
     * its table; the profile's own writes enable it again. */
    int gamma = 0, gamma_confirm = 0, gtm = 0, ee = 0;
    unsigned ee_sel = (RD(dev, FX1_ISP_EE_CTRL_OFFSET) & FX1_ISP_EE_CTRL_PROFILE_WR_SEL_MASK) >>
                      FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT;
    uint32_t ee_confirmed = 0; /* residents the profile confirms */
    for (i = 0; i < n; ++i) {
        const uint32_t off = writes[i].offset, v = writes[i].value;
        if (off == FX1_ISP_GAMMA_LUT_DATA_OFFSET) {
            gamma = 1;
        } else if (off == FX1_ISP_GAMMA_CTRL_OFFSET && (v & FX1_ISP_GAMMA_CTRL_LUT_LOAD_CONFIRM_MASK)) {
            gamma = gamma_confirm = 1;
        } else if (off == FX1_ISP_GTM_LUT_DATA_OFFSET) {
            gtm = 1;
        } else if (off == FX1_ISP_EE_CTRL_OFFSET) {
            ee_sel = (v & FX1_ISP_EE_CTRL_PROFILE_WR_SEL_MASK) >> FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT;
        } else if (off == FX1_ISP_EE_LUT_WDATA_OFFSET) {
            ee = 1;
        } else if (off == FX1_ISP_EE_LUT_CTRL_OFFSET && (v & FX1_ISP_EE_LUT_CTRL_PROFILE_LOAD_CONFIRM_MASK)) {
            ee = 1;
            ee_confirmed |= ee_sel < 3u ? 1u << ee_sel : 0u; /* code 3: nothing to confirm */
        }
    }
    if (gamma) {
        disable_for_load(dev, FX1_ISP_GAMMA_CTRL_OFFSET, FX1_ISP_GAMMA_CTRL_EN_MASK);
    }
    if (gtm) {
        disable_for_load(dev, FX1_ISP_GTM_CTRL_OFFSET, FX1_ISP_GTM_CTRL_EN_MASK);
    }
    if (ee) {
        disable_for_load(dev, FX1_ISP_EE_CTRL_OFFSET, FX1_ISP_EE_CTRL_EN_MASK);
    }
    for (i = 0; i < n; ++i) {
        WR(dev, writes[i].offset, writes[i].value);
    }
    /* A refused legacy confirmation raises no error: check the result. */
    if (gamma_confirm && !(RD(dev, FX1_ISP_GAMMA_STATUS_OFFSET) & FX1_ISP_GAMMA_STATUS_LUT_LOADED_MASK)) {
        return FX1_ISP_EIO;
    }
    if (ee_confirmed & ~((RD(dev, FX1_ISP_EE_STATUS_OFFSET) & FX1_ISP_EE_STATUS_LUT_VALID_MASK) >>
                         FX1_ISP_EE_STATUS_LUT_VALID_SHIFT)) {
        return FX1_ISP_EIO;
    }
    return FX1_ISP_OK;
}

/* ---- Discovery ------------------------------------------------------------------ */
int fx1_isp_discover(fx1_isp_dev *dev, fx1_isp_identity *out) {
    if (!initialised(dev) || !out) {
        return initialised(dev) ? FX1_ISP_EINVAL : FX1_ISP_ESTATE;
    }
    out->magic = RD(dev, FX1_ISP_TUNE_ID_OFFSET);
    out->revision = RD(dev, FX1_ISP_TUNE_REV_OFFSET);
    out->caps = RD(dev, FX1_ISP_TUNE_CAPS_OFFSET);
    return FX1_ISP_OK;
}

int fx1_isp_require_caps(fx1_isp_dev *dev, uint32_t caps) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    return (RD(dev, FX1_ISP_TUNE_CAPS_OFFSET) & caps) == caps ? FX1_ISP_OK : FX1_ISP_ENOTSUP;
}

/* ---- Output channels -------------------------------------------------------------- */
int fx1_isp_set_channel_buffers(fx1_isp_dev *dev, unsigned ch, const fx1_isp_channel_buffers *b) {
    unsigned k;
    if (!initialised(dev) || dev->state == FX1_ISP_STATE_STREAMING) {
        return FX1_ISP_ESTATE;
    }
    if ((ch != 1u && ch != 2u) || !b || !b->y_stride || (ch == 1u && !b->uv_stride)) {
        return FX1_ISP_EINVAL;
    }
    WR(dev, out_ch[ch].y_stride, b->y_stride);
    if (ch == 1u) {
        WR(dev, out_ch[ch].uv_stride, b->uv_stride);
    }
    for (k = 0; k < FX1_ISP_NUM_BUFFERS; ++k) {
        write_addr(dev, out_ch[ch].y_addr + 16u * k, b->y[k]);
        if (ch == 1u) {
            write_addr(dev, out_ch[ch].uv_addr + 16u * k, b->uv[k]);
        }
    }
    dev->ch_ready |= 1u << ch;
    return FX1_ISP_OK;
}

int fx1_isp_output_meta(fx1_isp_dev *dev, unsigned ch, unsigned index, fx1_isp_frame_meta *meta, uint32_t *frame,
                        uint32_t *profile) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (ch > 2u || index >= FX1_ISP_NUM_BUFFERS) {
        return FX1_ISP_EINVAL;
    }
    if (meta) {
        meta->tag = RD(dev, out_ch[ch].tag + 4u * index);
        meta->ts = RD(dev, out_ch[ch].ts + 4u * index);
        meta->sof_ts = RD(dev, out_ch[ch].sof + 4u * index);
        meta->eof_ts = RD(dev, out_ch[ch].eof + 4u * index);
    }
    if (frame) {
        *frame = RD(dev, out_ch[ch].frame + 4u * index);
    }
    if (profile) {
        *profile = RD(dev, out_ch[ch].profile + 4u * index);
    }
    return FX1_ISP_OK;
}

/* ---- Profile transaction ---------------------------------------------------------- */
/* One TUNE_CMD (or other profile write) and the TUNE_ERROR bits it raised.
 * The register is sticky W1C and a bit already set cannot show that the
 * command set it again, so the command runs in a clean window: bits left by
 * earlier accesses are acknowledged first (and kept in dev->tune_error_prior),
 * and the window is used only once the acknowledgement took (review V107-7..10
 * R4). FX1_ISP_EBUSY with *err = the bits that stayed set if it did not (a
 * recovery's command block, or a condition raising the bit again). */
static int tune_write(fx1_isp_dev *dev, uint32_t off, uint32_t value, uint32_t *err) {
    const uint32_t before = RD(dev, FX1_ISP_TUNE_ERROR_OFFSET);
    if (before) {
        dev->tune_error_prior |= before;
        WR(dev, FX1_ISP_TUNE_ERROR_OFFSET, before);
        const uint32_t left = RD(dev, FX1_ISP_TUNE_ERROR_OFFSET);
        if (left) {
            if (err) {
                *err = left;
            }
            return FX1_ISP_EBUSY;
        }
    }
    WR(dev, off, value);
    const uint32_t raised = RD(dev, FX1_ISP_TUNE_ERROR_OFFSET);
    if (err) {
        *err = raised;
    }
    if (raised) {
        WR(dev, FX1_ISP_TUNE_ERROR_OFFSET, raised);
        return FX1_ISP_EIO;
    }
    return FX1_ISP_OK;
}

static int select_bank(fx1_isp_dev *dev, unsigned bank, uint32_t *err) {
    if (err) {
        *err = 0;
    }
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (bank > 1u) {
        return FX1_ISP_EINVAL;
    }
    WR(dev, FX1_ISP_TUNE_BANK_OFFSET, bank);
    return FX1_ISP_OK;
}

int fx1_isp_profile_mode(fx1_isp_dev *dev, int enable) {
    uint32_t err = 0;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    /* IDLE class (MAS v1.07 §9.9.3): rejected with MODE unless idle. */
    const int rc = tune_write(dev, FX1_ISP_TUNE_MODE_OFFSET, enable ? FX1_ISP_TUNE_MODE_ENABLE_MASK : 0u, &err);
    return rc == FX1_ISP_OK ? FX1_ISP_OK : ((err & FX1_ISP_TUNE_ERROR_MODE_MASK) ? FX1_ISP_EBUSY : rc);
}

int fx1_isp_profile_begin(fx1_isp_dev *dev, unsigned bank, uint32_t *err) {
    const int rc = select_bank(dev, bank, err);
    return rc ? rc : tune_write(dev, FX1_ISP_TUNE_CMD_OFFSET, FX1_ISP_TUNE_CMD_INIT_MASK, err);
}

int fx1_isp_profile_confirm_tables(fx1_isp_dev *dev, unsigned bank, uint32_t mask, uint32_t *err) {
    int rc = select_bank(dev, bank, err);
    if (rc) {
        return rc;
    }
    rc = tune_write(dev, FX1_ISP_TUNE_TABLE_CMD_OFFSET, mask, err);
    if (rc == FX1_ISP_OK && (RD(dev, FX1_ISP_TUNE_TABLE_STATUS_OFFSET) & mask) != mask) {
        rc = FX1_ISP_EIO;
    }
    return rc;
}

int fx1_isp_profile_load_gamma(fx1_isp_dev *dev, unsigned bank, const uint16_t lut[256], uint32_t *err) {
    unsigned i;
    int rc = select_bank(dev, bank, err);
    if (rc) {
        return rc;
    }
    if (!lut) {
        return FX1_ISP_EINVAL;
    }
    /* The banked port (MAS v1.07 §6.14.4.2): explicit index, the TUNE_BANK bank. */
    for (i = 0; i < 256u; ++i) {
        WR(dev, FX1_ISP_TUNE_GAMMA_PORT_OFFSET, i);
        rc = tune_write(dev, FX1_ISP_TUNE_GAMMA_DATA_OFFSET, lut[i] & FX1_ISP_TUNE_GAMMA_DATA_VALUE_MASK, err);
        if (rc) {
            return rc;  /* LOCKED: a sealed, active, pending or referenced bank */
        }
    }
    for (i = 0; i < 256u; ++i) {
        WR(dev, FX1_ISP_TUNE_GAMMA_PORT_OFFSET, i);
        if (RD(dev, FX1_ISP_TUNE_GAMMA_DATA_OFFSET) != (lut[i] & FX1_ISP_TUNE_GAMMA_DATA_VALUE_MASK)) {
            return FX1_ISP_EIO;
        }
    }
    return fx1_isp_profile_confirm_tables(dev, bank, FX1_ISP_TUNE_TABLE_CMD_GAMMA_CONFIRM_MASK, err);
}

/* Bank payload ports a legacy profile writes, replayed into the bank after
 * INIT (it clears the bank's payload write flags, MAS v1.07 §9.7): the GTM
 * manual curve (legacy port, routed to the TUNE_BANK bank in profile mode
 * while GTM is disabled in it: the profile's own GTM_CTRL writes do that)
 * and the TUNE extension payloads with their confirmations. Gamma goes
 * through fx1_isp_profile_load_gamma. */
static int bank_payload_port(uint32_t off) {
    return off == FX1_ISP_GTM_CTRL_OFFSET || off == FX1_ISP_GTM_LUT_ADDR_OFFSET || off == FX1_ISP_GTM_LUT_DATA_OFFSET ||
           off == FX1_ISP_TUNE_NR_LUT_ADDR_OFFSET || off == FX1_ISP_TUNE_NR_LUT_DATA_OFFSET ||
           off == FX1_ISP_TUNE_BPC_COUNT_OFFSET || off == FX1_ISP_TUNE_BPC_ADDR_OFFSET ||
           off == FX1_ISP_TUNE_BPC_DATA_OFFSET || off == FX1_ISP_TUNE_LSC_SECTOR_ADDR_OFFSET ||
           off == FX1_ISP_TUNE_LSC_SECTOR_DATA_OFFSET || off == FX1_ISP_TUNE_TABLE_CMD_OFFSET;
}

int fx1_isp_profile_from_legacy(fx1_isp_dev *dev, unsigned bank, const fx1_isp_reg_write *writes, size_t n,
                                uint32_t profile_id, uint32_t *err) {
    uint16_t lut[256];
    unsigned i;
    if (err) {
        *err = 0;
    }
    if (!initialised(dev) || dev->state == FX1_ISP_STATE_STREAMING) {
        return FX1_ISP_ESTATE;
    }
    if (bank > 1u || (n && !writes)) {
        return FX1_ISP_EINVAL;
    }
    /* The legacy curve as the hardware holds it (bank 0, read back before
     * INIT): whatever order, segments or rewrites loaded it, and whether the
     * profile or fx1_isp_load_gamma did (review V107-10 completion R1). Only
     * a confirmed curve is carried over; an unconfirmed one is not applied
     * in legacy mode either, and SEAL then reports NOT_READY if Gamma is on. */
    const int gamma = (RD(dev, FX1_ISP_GAMMA_STATUS_OFFSET) & FX1_ISP_GAMMA_STATUS_LUT_LOADED_MASK) != 0;
    if (gamma) {
        const uint32_t addr = RD(dev, FX1_ISP_GAMMA_LUT_ADDR_OFFSET);
        for (i = 0; i < 256u; ++i) {
            WR(dev, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, i);
            lut[i] = (uint16_t)RD(dev, FX1_ISP_GAMMA_LUT_RDATA_OFFSET);
        }
        WR(dev, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, addr);
    }
    int rc = fx1_isp_profile_mode(dev, 1);
    if (rc == FX1_ISP_OK) {
        rc = fx1_isp_profile_begin(dev, bank, err);
    }
    if (rc == FX1_ISP_OK && gamma) {
        rc = fx1_isp_profile_load_gamma(dev, bank, lut, err);
    }
    for (size_t k = 0; rc == FX1_ISP_OK && k < n; ++k) {
        if (bank_payload_port(writes[k].offset)) {  /* the same port: its own addressing and increments */
            rc = tune_write(dev, writes[k].offset, writes[k].value, err);
        }
    }
    if (rc == FX1_ISP_OK) {
        rc = fx1_isp_profile_seal(dev, bank, profile_id, err);
    }
    if (rc == FX1_ISP_OK) {
        rc = fx1_isp_profile_apply(dev, bank, 0, 0, err);
    }
    return rc;
}

int fx1_isp_profile_seal(fx1_isp_dev *dev, unsigned bank, uint32_t profile_id, uint32_t *err) {
    int rc = select_bank(dev, bank, err);
    if (rc) {
        return rc;
    }
    rc = tune_write(dev, FX1_ISP_TUNE_PROFILE_ID_OFFSET, profile_id, err);
    return rc ? rc : tune_write(dev, FX1_ISP_TUNE_CMD_OFFSET, FX1_ISP_TUNE_CMD_SEAL_MASK, err);
}

int fx1_isp_profile_apply(fx1_isp_dev *dev, unsigned bank, int exact, uint32_t frame, uint32_t *err) {
    const int rc = select_bank(dev, bank, err);
    if (rc) {
        return rc;
    }
    if (exact) {
        WR(dev, FX1_ISP_TUNE_APPLY_FRAME_OFFSET, frame);
    }
    return tune_write(dev, FX1_ISP_TUNE_CMD_OFFSET,
                      FX1_ISP_TUNE_CMD_APPLY_MASK | (exact ? FX1_ISP_TUNE_CMD_EXACT_MASK : 0u), err);
}

int fx1_isp_profile_cancel(fx1_isp_dev *dev, uint32_t *err) {
    if (err) {
        *err = 0;
    }
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    return tune_write(dev, FX1_ISP_TUNE_CMD_OFFSET, FX1_ISP_TUNE_CMD_CANCEL_MASK, err);
}

int fx1_isp_profile_unseal(fx1_isp_dev *dev, unsigned bank, uint32_t *err) {
    const int rc = select_bank(dev, bank, err);
    return rc ? rc : tune_write(dev, FX1_ISP_TUNE_CMD_OFFSET, FX1_ISP_TUNE_CMD_UNSEAL_MASK, err);
}

int fx1_isp_profile_state(fx1_isp_dev *dev, fx1_isp_profile_status *out) {
    if (!initialised(dev) || !out) {
        return initialised(dev) ? FX1_ISP_EINVAL : FX1_ISP_ESTATE;
    }
    out->status = RD(dev, FX1_ISP_TUNE_STATUS_OFFSET);
    out->active_id = RD(dev, FX1_ISP_TUNE_ACTIVE_ID_OFFSET);
    out->active_frame = RD(dev, FX1_ISP_TUNE_ACTIVE_FRAME_OFFSET);
    out->next_frame = RD(dev, FX1_ISP_TUNE_NEXT_FRAME_OFFSET);
    return FX1_ISP_OK;
}

static void ee_ctrl_fields(fx1_isp_dev *dev, uint32_t mask, uint32_t value);

int fx1_isp_load_gamma(fx1_isp_dev *dev, const uint16_t lut[256]) {
    unsigned i;
    uint32_t ctrl;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (!lut) {
        return FX1_ISP_EINVAL;
    }
    ctrl = RD(dev, FX1_ISP_GAMMA_CTRL_OFFSET);
    if (ctrl & FX1_ISP_GAMMA_CTRL_EN_MASK) {
        return FX1_ISP_EBUSY; /* confirmation needs EN = 0 (MAS v1.07 §6.14.4) */
    }
    WR(dev, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, 0);  /* the address auto-increments per DATA write */
    for (i = 0; i < 256u; ++i) {
        WR(dev, FX1_ISP_GAMMA_LUT_DATA_OFFSET, lut[i]);
    }
    for (i = 0; i < 256u; ++i) { /* read back the full table before confirming it */
        WR(dev, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, i);
        if ((RD(dev, FX1_ISP_GAMMA_LUT_RDATA_OFFSET) & FX1_ISP_GAMMA_LUT_DATA_DATA_MASK) != lut[i]) {
            return FX1_ISP_EIO;
        }
    }
    WR(dev, FX1_ISP_GAMMA_CTRL_OFFSET, ctrl | FX1_ISP_GAMMA_CTRL_LUT_LOAD_CONFIRM_MASK);
    return (RD(dev, FX1_ISP_GAMMA_STATUS_OFFSET) & FX1_ISP_GAMMA_STATUS_LUT_LOADED_MASK) ? FX1_ISP_OK
                                                                                      : FX1_ISP_EAGAIN;
}

int fx1_isp_ee_confirm(fx1_isp_dev *dev, unsigned profile) {
    uint32_t err;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (profile > 2u) {
        return FX1_ISP_EINVAL;
    }
    if (RD(dev, FX1_ISP_EE_CTRL_OFFSET) & FX1_ISP_EE_CTRL_EN_MASK) {
        return FX1_ISP_EAGAIN; /* legacy mode: EE disabled and idle (MAS v1.07 §9.9.1) */
    }
    ee_ctrl_fields(dev, FX1_ISP_EE_CTRL_PROFILE_WR_SEL_MASK, profile << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT);
    WR(dev, FX1_ISP_TUNE_ERROR_OFFSET, FX1_ISP_TUNE_ERROR_NOT_READY_MASK);
    WR(dev, FX1_ISP_EE_LUT_CTRL_OFFSET,
       (RD(dev, FX1_ISP_EE_LUT_CTRL_OFFSET) & FX1_ISP_EE_LUT_CTRL_LUT_SEL_MASK) |
           FX1_ISP_EE_LUT_CTRL_PROFILE_LOAD_CONFIRM_MASK);
    if (RD(dev, FX1_ISP_EE_STATUS_OFFSET) & (1u << (FX1_ISP_EE_STATUS_LUT_VALID_SHIFT + profile))) {
        return FX1_ISP_OK;
    }
    err = RD(dev, FX1_ISP_TUNE_ERROR_OFFSET) & FX1_ISP_TUNE_ERROR_NOT_READY_MASK;
    if (err) {
        WR(dev, FX1_ISP_TUNE_ERROR_OFFSET, err);
        return FX1_ISP_EIO;
    }
    return FX1_ISP_EAGAIN; /* not idle */
}

int fx1_isp_load_ee_table(fx1_isp_dev *dev, unsigned bank, const uint16_t *table, unsigned n) {
    unsigned i;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (bank > 3u || n != (bank < 2u ? 64u : 32u) || !table) {
        return FX1_ISP_EINVAL;
    }
    const uint32_t ctrl = RD(dev, FX1_ISP_EE_LUT_CTRL_OFFSET) & ~FX1_ISP_EE_LUT_CTRL_LUT_SEL_MASK;
    WR(dev, FX1_ISP_EE_LUT_CTRL_OFFSET, ctrl | (bank << FX1_ISP_EE_LUT_CTRL_LUT_SEL_SHIFT));
    WR(dev, FX1_ISP_EE_LUT_ADDR_OFFSET, 0);
    for (i = 0; i < n; ++i) {
        WR(dev, FX1_ISP_EE_LUT_WDATA_OFFSET, table[i]);
    }
    return FX1_ISP_OK;
}

int fx1_isp_gtm_manual_begin(fx1_isp_dev *dev) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    const uint32_t ctrl = RD(dev, FX1_ISP_GTM_CTRL_OFFSET);
    /* IDMA_FRAME_COUNT counts the frames the pipeline took completely: every
     * frame whose end built a curve, and conservatively also one a recovery
     * aborted after the IDMA finished it (review U3-R3, MAS v1.07 §9.9.8).
     * Only i_rst_n clears it; soft reset and fx1_isp_init keep it. */
    const uint32_t count = RD(dev, FX1_ISP_IDMA_FRAME_COUNT_OFFSET);
    /* Step 1: EN = 0 (table writes accepted, frames pass through), MANUAL = 1
     * (no new automatic curve; a build already running still completes). */
    if (!dev->gtm_manual) {
        dev->gtm_en = (ctrl & FX1_ISP_GTM_CTRL_EN_MASK) != 0;
    }
    WR(dev, FX1_ISP_GTM_CTRL_OFFSET, (ctrl & ~FX1_ISP_GTM_CTRL_EN_MASK) | FX1_ISP_GTM_CTRL_MANUAL_MASK);
    dev->gtm_manual = 1;
    /* Step 2: a curve built at the end of an earlier frame waits for the next
     * SOF. Every frame in flight may have started before step 1, so the frame
     * queued after them must be taken by the pipeline: its SOF consumed any
     * pending curve, and MANUAL kept its end from building one. Cold only when
     * no frame has been taken since i_rst_n and none is queued. */
    dev->gtm_wait = !(count == 0u && dev->in_flight == 0);
    dev->gtm_target = count + dev->in_flight + 1u;
    return FX1_ISP_OK;
}

int fx1_isp_load_gtm_lut(fx1_isp_dev *dev, const uint16_t lut[65]) {
    unsigned i;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (!lut) {
        return FX1_ISP_EINVAL;
    }
    if (!dev->gtm_manual) {
        fx1_isp_gtm_manual_begin(dev);
    }
    if (dev->gtm_wait && (int32_t)(RD(dev, FX1_ISP_IDMA_FRAME_COUNT_OFFSET) - dev->gtm_target) < 0) {
        return FX1_ISP_EAGAIN;  /* step 2 not done: a pending automatic curve could replace the load */
    }
    WR(dev, FX1_ISP_GTM_LUT_ADDR_OFFSET, 0);
    for (i = 0; i < 65u; ++i) {
        WR(dev, FX1_ISP_GTM_LUT_DATA_OFFSET, lut[i]);
    }
    for (i = 0; i < 65u; ++i) {  /* step 4 */
        WR(dev, FX1_ISP_GTM_LUT_ADDR_OFFSET, i);
        if ((RD(dev, FX1_ISP_GTM_LUT_RDATA_OFFSET) & 0xFFFFu) != lut[i]) {
            return FX1_ISP_EIO;
        }
    }
    /* Step 5: restore EN; MANUAL stays set so the curve persists. */
    if (dev->gtm_en) {
        const uint32_t ctrl = RD(dev, FX1_ISP_GTM_CTRL_OFFSET);
        WR(dev, FX1_ISP_GTM_CTRL_OFFSET, ctrl | FX1_ISP_GTM_CTRL_EN_MASK | FX1_ISP_GTM_CTRL_MANUAL_MASK);
    }
    dev->gtm_manual = 0;
    dev->gtm_wait = 0;
    return FX1_ISP_OK;
}

/* ---- EE resident profiles (MAS §6.18.4.1, §9.2) ---- */

static void ee_ctrl_fields(fx1_isp_dev *dev, uint32_t mask, uint32_t value) {
    const uint32_t ctrl = RD(dev, FX1_ISP_EE_CTRL_OFFSET);
    WR(dev, FX1_ISP_EE_CTRL_OFFSET, (ctrl & ~mask) | (value & mask));
}

int fx1_isp_ee_load_profile(fx1_isp_dev *dev, unsigned profile, const fx1_isp_ee_profile *p) {
    const uint32_t scalars[10] = {p ? p->alpha : 0u, p ? p->beta : 0u, p ? p->clamp : 0u, p ? p->feature_en : 0u,
                                  p ? p->radial_center : 0u, p ? p->r2_th[0] : 0u, p ? p->r2_th[1] : 0u,
                                  p ? p->r2_th[2] : 0u, p ? p->radial_gain_01 : 0u, p ? p->radial_gain_23 : 0u};
    const uint16_t *tables[4];
    unsigned k, bank, i;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (profile > 2u || !p) {
        return FX1_ISP_EINVAL;
    }
    /* A profile a frame may still use is not rewritten while EE works: any
     * profile selected since EE was last seen idle. Seen idle, no frame is in
     * EE, and the next one will use the current selection. */
    if (RD(dev, FX1_ISP_EE_STATUS_OFFSET) & FX1_ISP_EE_STATUS_BUSY_MASK) {
        if (dev->ee_in_use & (1u << profile)) {
            return FX1_ISP_EBUSY;
        }
    } else {
        dev->ee_in_use = 1u << dev->ee_sel;
    }
    tables[0] = p->luma;
    tables[1] = p->activity;
    tables[2] = p->contrast_pos;
    tables[3] = p->contrast_neg;
    ee_ctrl_fields(dev, FX1_ISP_EE_CTRL_PROFILE_WR_SEL_MASK, profile << FX1_ISP_EE_CTRL_PROFILE_WR_SEL_SHIFT);
    /* Full 32-bit writes (MAS v1.07 §9.9.1: a partial write merges with the
     * selected resident's value). */
    for (k = 0; k < 10u; ++k) {
        WR(dev, FX1_ISP_EE_ALPHA_OFFSET + 4u * k, scalars[k]);
    }
    for (bank = 0; bank < 4u; ++bank) {
        const unsigned n = bank < 2u ? 64u : 32u;
        const int rc = fx1_isp_load_ee_table(dev, bank, tables[bank], n);
        if (rc != FX1_ISP_OK) {
            return rc;
        }
        WR(dev, FX1_ISP_EE_LUT_ADDR_OFFSET, 0);  /* WDATA advanced the address */
        for (i = 0; i < n; ++i) {
            WR(dev, FX1_ISP_EE_LUT_ADDR_OFFSET, i);
            if ((RD(dev, FX1_ISP_EE_LUT_RDATA_OFFSET) & 0xFFFFu) != tables[bank][i]) {
                return FX1_ISP_EIO;
            }
        }
    }
    return fx1_isp_ee_confirm(dev, profile);
}

int fx1_isp_ee_select(fx1_isp_dev *dev, unsigned profile, int enable) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (profile > 2u) {
        return FX1_ISP_EINVAL;
    }
    dev->ee_sel = profile;
    dev->ee_in_use |= 1u << profile;  /* the profiles selected before stay in use until EE is seen idle */
    ee_ctrl_fields(dev, FX1_ISP_EE_CTRL_PROFILE_SEL_MASK | FX1_ISP_EE_CTRL_EN_MASK,
                   (profile << FX1_ISP_EE_CTRL_PROFILE_SEL_SHIFT) | (enable ? FX1_ISP_EE_CTRL_EN_MASK : 0u));
    return FX1_ISP_OK;
}

int fx1_isp_lsc_load(fx1_isp_dev *dev, unsigned profile, unsigned nx, unsigned ny, const uint32_t *coef,
                     uint32_t *lsc_error) {
    unsigned i;
    uint32_t err;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (profile > 2u || nx < 2u || nx > LSC_MESH_MAX || ny < 2u || ny > LSC_MESH_MAX || !coef) {
        return FX1_ISP_EINVAL;
    }
    const uint32_t nodes = nx | (ny << 8);
    /* Any change of the mesh size invalidates every profile (HAS p62), so it is
     * written only when it differs. */
    if ((RD(dev, FX1_ISP_LSC_MESH_NODES_OFFSET) & 0xFFFFu) != nodes) {
        WR(dev, FX1_ISP_LSC_MESH_NODES_OFFSET, nodes);
    }
    WR(dev, FX1_ISP_LSC_ERROR_OFFSET, 0xFFFFFFFFu);  /* W1C: start from a clean error state */
    WR(dev, FX1_ISP_LSC_LOAD_CTRL_OFFSET, profile | FX1_ISP_LSC_LOAD_CTRL_LOAD_BEGIN_MASK);
    err = RD(dev, FX1_ISP_LSC_ERROR_OFFSET);
    if (err) {  /* e.g. ACTIVE_LOAD_REJECT: the target is the active profile, valid or not (MAS Table 31) */
        if (lsc_error) {
            *lsc_error = err;
        }
        return FX1_ISP_EIO;
    }
    for (i = 0; i < 4u * nx * ny; ++i) {
        WR(dev, FX1_ISP_LSC_COEF_DATA_OFFSET, coef[i]);
    }
    WR(dev, FX1_ISP_LSC_LOAD_CTRL_OFFSET, profile | FX1_ISP_LSC_LOAD_CTRL_LOAD_VALIDATE_MASK);
    err = RD(dev, FX1_ISP_LSC_ERROR_OFFSET);
    if (lsc_error) {
        *lsc_error = err;
    }
    if (err & ~FX1_ISP_LSC_ERROR_COEF_RANGE_MASK) {  /* COEF_RANGE flags but does not block (ALG-LSC-03) */
        return FX1_ISP_EIO;
    }
    return (RD(dev, FX1_ISP_LSC_PROFILE_STATUS_OFFSET) & (1u << profile)) ? FX1_ISP_OK : FX1_ISP_EIO;
}

int fx1_isp_lsc_select(fx1_isp_dev *dev, unsigned profile) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (profile > 2u) {
        return FX1_ISP_EINVAL;
    }
    WR(dev, FX1_ISP_LSC_ERROR_OFFSET, FX1_ISP_LSC_ERROR_PROFILE_SEL_REJECT_MASK);
    WR(dev, FX1_ISP_LSC_PROFILE_SEL_OFFSET, profile);
    return (RD(dev, FX1_ISP_LSC_ERROR_OFFSET) & FX1_ISP_LSC_ERROR_PROFILE_SEL_REJECT_MASK) ? FX1_ISP_EIO : FX1_ISP_OK;
}

void fx1_isp_ccm_set(fx1_isp_dev *dev, const int16_t m[9], const int16_t ofs[3], int enable) {
    unsigned i;
    const uint32_t en = enable ? FX1_ISP_CCM_CTRL_EN_MASK : 0u;
    if (!initialised(dev) || !m || !ofs) {
        return;
    }
    /* The block captures the registers at each accepted SOF; UPDATED is a
     * compatibility field with no effect (CSR/MAS 07/10, owner Q1). Nothing
     * protects a multi-register update: a SOF between two writes captures a
     * mixed set, so the caller must finish before the next frame starts.
     * The coefficients go first, the enable last. */
    for (i = 0; i < 9u; ++i) {
        WR(dev, FX1_ISP_CCM_CRR_OFFSET + 4u * i, (uint32_t)m[i] & 0xFFFu);
    }
    for (i = 0; i < 3u; ++i) {
        WR(dev, FX1_ISP_CCM_OFS_R_OFFSET + 4u * i, (uint32_t)ofs[i] & 0xFFFu);
    }
    WR(dev, FX1_ISP_CCM_CTRL_OFFSET, en | FX1_ISP_CCM_CTRL_UPDATED_MASK);
}

void fx1_isp_aec_configure(fx1_isp_dev *dev, const fx1_isp_aec_cfg *c, int enable) {
    if (!initialised(dev) || !c) {
        return;
    }
    WR(dev, FX1_ISP_AEC_ZONE_CFG_OFFSET, (c->nx & 0x3Fu) | ((c->ny & 0x1Fu) << 8));
    WR(dev, FX1_ISP_AEC_ZONE_SIZE_OFFSET, (c->zone_w & 0xFFFu) | ((c->zone_h & 0xFFFu) << 16));
    WR(dev, FX1_ISP_AEC_SAMPLE_CLIP_OFFSET, (c->clip_min & 0xFFFu) | ((c->clip_max & 0xFFFu) << 16));
    WR(dev, FX1_ISP_AEC_THRESH_OFFSET, (c->th_ue & 0xFFFu) | ((c->th_oe & 0xFFFu) << 16));
    WR(dev, FX1_ISP_TUNE_AEC_SOURCE_OFFSET, c->source & FX1_ISP_TUNE_AEC_SOURCE_SOURCE_MASK);
    WR(dev, FX1_ISP_AEC_CONTEXT_ID_OFFSET, c->context_id);
    WR(dev, FX1_ISP_AEC_CTRL_OFFSET, (enable ? FX1_ISP_AEC_CTRL_EN_MASK : 0u) | FX1_ISP_AEC_CTRL_COMMIT_MASK);
}

int fx1_isp_start(fx1_isp_dev *dev, const fx1_isp_buffers *b) {
    unsigned k;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (dev->state == FX1_ISP_STATE_STREAMING) {
        /* Draining does not reset the hardware's buffer rotation. START must
         * follow STOP, which resets both hardware and software pointers. */
        return dev->in_flight ? FX1_ISP_EBUSY : FX1_ISP_ESTATE;
    }
    if (!b || b->max_burst_beats < 1u || b->max_burst_beats > 256u) {
        return FX1_ISP_EINVAL;
    }
    const uint32_t width = RD(dev, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET) & FX1_ISP_COMMON_FRAME_WIDTH_H_ACTIVE_MASK;
    if (b->in_stride < 2u * width) {
        return FX1_ISP_EINVAL;
    }
    /* HAS §9.1: the pipeline is enabled before the DMA (DEC-20: with ISP_EN = 0
     * the output geometry is 0x0 and the ODMA refuses to start). */
    const uint32_t old_isp_en = RD(dev, FX1_ISP_COMMON_CTRL_OFFSET) & FX1_ISP_COMMON_CTRL_ISP_EN_MASK;
    WR(dev, FX1_ISP_COMMON_CTRL_OFFSET, FX1_ISP_COMMON_CTRL_ISP_EN_MASK);
    /* Valid output geometry of every enabled channel before its output DMA
     * claims a buffer (MAS §11.6 step 5; v1.07 §8.3: an invalid enabled
     * geometry admits no frame), with buffers programmed and strides that
     * hold a line (Table 6-153; CH2 3 bytes per pixel). OUTPUT_ENABLE is
     * IDLE class, so the mask holds for the whole stream. */
    const unsigned outputs = RD(dev, FX1_ISP_OUTPUT_ENABLE_OFFSET) & FX1_ISP_OUTPUT_ENABLE_CHANNELS_MASK;
    int bad = outputs == 0u;
    for (k = 0; k < 3u && !bad; ++k) {
        if (!(outputs & (1u << k))) {
            continue;
        }
        uint32_t gst = 0;
        fx1_isp_geometry_status(dev, k, &gst);
        const uint32_t w = RD(dev, out_ch[k].out_w);
        const uint32_t ys = k ? RD(dev, out_ch[k].y_stride) : b->y_stride;
        const uint32_t uvs = k == 0 ? b->uv_stride : (k == 1 ? RD(dev, out_ch[k].uv_stride) : ~0u);
        bad = !(gst & FX1_ISP_CH0_GEOMETRY_STATUS_VALID_MASK) || (k && !(dev->ch_ready & (1u << k))) ||
              ys < (k == 2 ? 3u * w : w) || uvs < w;
    }
    if (bad) {
        WR(dev, FX1_ISP_COMMON_CTRL_OFFSET, old_isp_en);  /* failed start has no lasting enable */
        return FX1_ISP_EINVAL;
    }
    const uint32_t out_width = RD(dev, FX1_ISP_RESIZER_OUT_W_OFFSET);
    const uint32_t out_height = RD(dev, FX1_ISP_RESIZER_OUT_H_OFFSET);
    dev->outputs = outputs;
    dev->out_width = out_width;
    dev->out_height = out_height;
    WR(dev, FX1_ISP_COMMON_IRQ_EN_OFFSET, FX1_ISP_COMMON_IRQ_EN_FRAME_DONE_EN_MASK | FX1_ISP_COMMON_IRQ_EN_ERROR_EN_MASK |
                                              FX1_ISP_COMMON_IRQ_EN_STATS_READY_EN_MASK);
    /* IDMA_DONE as well: an input buffer back to software. COMMON ERROR_IRQ
     * reports every accepted recovery (MAS v1.07 Table 9-188). CFG_ERROR: an illegal start
     * configuration (MAS Table 162), reported in fx1_isp_events.dma. */
    WR(dev, FX1_ISP_DMA_IRQ_EN_OFFSET, FX1_ISP_DMA_IRQ_EN_IRQ_EN_ODMA_DONE_BIT | FX1_ISP_DMA_IRQ_EN_IRQ_EN_AXI_ERROR_BIT |
                                           FX1_ISP_DMA_IRQ_EN_IRQ_EN_ODMA_OVERFLOW_BIT |
                                           FX1_ISP_DMA_IRQ_EN_IRQ_EN_IDMA_DONE_BIT |
                                           FX1_ISP_DMA_IRQ_EN_IRQ_EN_CFG_ERROR_BIT);
    WR(dev, FX1_ISP_IDMA_STRIDE_OFFSET, b->in_stride);
    WR(dev, FX1_ISP_ODMA_Y_STRIDE_OFFSET, b->y_stride);
    WR(dev, FX1_ISP_ODMA_UV_STRIDE_OFFSET, b->uv_stride);
    for (k = 0; k < FX1_ISP_NUM_BUFFERS; ++k) {
        write_addr(dev, FX1_ISP_IDMA_BUF_ADDR_L0_OFFSET + 8u * k, b->in[k]);
        write_addr(dev, FX1_ISP_ODMA_Y_ADDR_L0_OFFSET + 16u * k, b->y[k]);
        write_addr(dev, FX1_ISP_ODMA_UV_ADDR_L0_OFFSET + 16u * k, b->uv[k]);
    }
    /* The engines are enabled with the first queued frame, so a stream does
     * not begin with an underrun report (M4-R5). */
    dev->dma_ctrl = ((b->max_burst_beats - 1u) << FX1_ISP_DMA_CTRL_MAX_BURST_M1_SHIFT) | FX1_ISP_DMA_CTRL_IDMA_EN_MASK |
                    FX1_ISP_DMA_CTRL_ODMA_EN_MASK;
    dev->started = 0;
    restart_rotation(dev);
    dev->next_read = 0; /* a slot read before the start is the restarted rotation's */
    dev->state = FX1_ISP_STATE_STREAMING;
    return FX1_ISP_OK;
}

unsigned fx1_isp_next_input(fx1_isp_dev *dev) {
    dev->next_gen = dev->rotation_gen;
    dev->next_read = 1;
    return dev->in_next;
}

int fx1_isp_input_ready(fx1_isp_dev *dev) {
    if (!streaming(dev)) {
        return 0;
    }
    /* Not while a recovery runs: its final retirement clears VALID / FREE. */
    if (settle_recovery(dev) != FX1_ISP_OK) {
        return 0;
    }
    flush_pending_ack(dev);
    return dev->in_flight < FX1_ISP_NUM_BUFFERS && !output_held(dev, dev->out_next) &&
           !(dev->pending_ack & (1u << dev->out_next)) &&
           !(RD(dev, FX1_ISP_IDMA_BUF_VALID_OFFSET) & (1u << dev->in_next));
}

static int queue_frame(fx1_isp_dev *dev, uint32_t *seq, const uint32_t *meta);

int fx1_isp_queue_frame(fx1_isp_dev *dev) { return fx1_isp_queue_frame_ex(dev, NULL); }

int fx1_isp_queue_frame_ex(fx1_isp_dev *dev, uint32_t *seq) { return queue_frame(dev, seq, NULL); }

int fx1_isp_queue_frame_meta(fx1_isp_dev *dev, uint32_t tag, uint32_t ts, uint32_t *seq) {
    const uint32_t meta[2] = {tag, ts};
    return queue_frame(dev, seq, meta);
}

static int queue_frame(fx1_isp_dev *dev, uint32_t *seq, const uint32_t *meta) {
    if (!streaming(dev)) {
        return FX1_ISP_ESTATE;
    }
    /* Never re-arm a VALID input or a held output, nor during a recovery. A
     * recovery settled here, or by any call since the caller read
     * fx1_isp_next_input(), may have restarted the rotation after it filled
     * that slot: then the caller refills the new slot. */
    const unsigned slot = dev->in_next;
    if (!fx1_isp_input_ready(dev)) {
        return FX1_ISP_EBUSY;
    }
    if (dev->in_next != slot || (dev->next_read && dev->next_gen != dev->rotation_gen)) {
        dev->next_read = 0; /* the caller reads the new slot */
        return FX1_ISP_EAGAIN;
    }
    dev->next_read = 0;
    /* The input's tag and capture timestamp, before IDMA_BUF_VALID (MAS
     * Table 160): the IDMA captures them when it starts the frame. */
    if (meta) {
        WR(dev, FX1_ISP_IDMA_BUF_TAG0_OFFSET + 4u * dev->in_next, meta[0]);
        WR(dev, FX1_ISP_IDMA_BUF_TS0_OFFSET + 4u * dev->in_next, meta[1]);
    }
    /* Outputs first, so every enabled channel is armed when the frame
     * arrives; the same index on each (their rotations move together). */
    for (unsigned c = 0; c < 3u; ++c) {
        if (dev->outputs & (1u << c)) {
            WR(dev, out_ch[c].free, 1u << dev->out_next);
        }
    }
    WR(dev, FX1_ISP_IDMA_BUF_VALID_OFFSET, 1u << dev->in_next);
    dev->queue[dev->in_flight].seq = dev->next_seq;
    dev->queue[dev->in_flight].input = dev->in_next;
    dev->queue[dev->in_flight].output = dev->out_next;
    dev->reset_completed = 0;
    if (seq) {
        *seq = dev->next_seq;
    }
    ++dev->next_seq;
    dev->out_next = (dev->out_next + 1u) % FX1_ISP_NUM_BUFFERS;
    dev->in_next = (dev->in_next + 1u) % FX1_ISP_NUM_BUFFERS;
    ++dev->in_flight;
    if (!dev->started) {
        WR(dev, FX1_ISP_DMA_CTRL_OFFSET, dev->dma_ctrl);
        dev->started = 1;
    }
    return FX1_ISP_OK;
}

void fx1_isp_irq(fx1_isp_dev *dev, fx1_isp_events *ev) {
    unsigned k, n = 0;
    if (!ev) {
        return;
    }
    ev->common = ev->dma = ev->dma_err = ev->output = 0;
    ev->frames_done = ev->frames_lost = 0;
    if (!initialised(dev)) {
        return;
    }
    ev->common = RD(dev, FX1_ISP_COMMON_IRQ_STATUS_OFFSET);
    ev->dma = RD(dev, FX1_ISP_DMA_IRQ_STAT_OFFSET);
    ev->dma_err = RD(dev, FX1_ISP_DMA_ERR_OFFSET);
    ev->output = RD(dev, FX1_ISP_OUTPUT_IRQ_STATUS_OFFSET);
    /* W1C, per bit: a source that fires again between the read and the
     * write is set again by hardware (hardware set wins, HAS Table 7-10). */
    WR(dev, FX1_ISP_COMMON_IRQ_STATUS_OFFSET, ev->common);
    WR(dev, FX1_ISP_DMA_IRQ_STAT_OFFSET, ev->dma);
    WR(dev, FX1_ISP_OUTPUT_IRQ_STATUS_OFFSET, ev->output);
    if (dev->state == FX1_ISP_STATE_STREAMING) {
        (void)settle_recovery(dev);  /* a running recovery is settled by a later call */
    }
    ev->frames_lost = dev->lost_count;
    const uint32_t done = outputs_done(dev);
    for (k = 0; k < dev->in_flight; ++k) {  /* completed buffers, in queue order */
        if (!(done & (1u << dev->queue[k].output))) {
            break;
        }
        ++n;
    }
    ev->frames_done = n;
}

int fx1_isp_next_done(fx1_isp_dev *dev, unsigned *index) { return fx1_isp_next_done_ex(dev, index, NULL); }

int fx1_isp_next_done_ex(fx1_isp_dev *dev, unsigned *index, uint32_t *seq) {
    return fx1_isp_next_done_meta(dev, index, seq, NULL);
}

int fx1_isp_next_done_meta(fx1_isp_dev *dev, unsigned *index, uint32_t *seq, fx1_isp_frame_meta *meta) {
    if (!streaming(dev) || !index) {
        return streaming(dev) ? FX1_ISP_EINVAL : FX1_ISP_ESTATE;
    }
    /* Nothing is collected while a recovery runs or is not settled: its
     * command block would drop the acknowledgement (review V107-7..10 R2). */
    if (settle_recovery(dev) != FX1_ISP_OK) {
        return FX1_ISP_ENODATA;
    }
    if (!dev->in_flight || !(outputs_done(dev) & (1u << dev->queue[0].output))) {
        return FX1_ISP_ENODATA;
    }
    *index = dev->queue[0].output;
    if (meta) {
        /* Published with the buffer at completion; stable until software
         * frees this buffer again (MAS Table 160). */
        meta->tag = RD(dev, FX1_ISP_ODMA_BUF_TAG0_OFFSET + 4u * *index);
        meta->ts = RD(dev, FX1_ISP_ODMA_BUF_TS0_OFFSET + 4u * *index);
        meta->sof_ts = RD(dev, FX1_ISP_ODMA_BUF_SOF_TS0_OFFSET + 4u * *index);
        meta->eof_ts = RD(dev, FX1_ISP_ODMA_BUF_EOF_TS0_OFFSET + 4u * *index);
    }
    if (!ack_done(dev, *index)) {
        /* A recovery started meanwhile: the frame stays queued (its DONE is
         * kept by the DMA scope) and is collected again once settled. */
        return FX1_ISP_EAGAIN;
    }
    const fx1_isp_frame f = queue_remove(dev, 0);
    if (seq) {
        *seq = f.seq;
    }
    return FX1_ISP_OK;
}

uint32_t fx1_isp_frame_counter(fx1_isp_dev *dev) {
    return initialised(dev) ? RD(dev, FX1_ISP_FRAME_COUNTER_OFFSET) : 0u;
}

int fx1_isp_recover(fx1_isp_dev *dev, uint32_t dma_err) { return fx1_isp_recover_ex(dev, dma_err, NULL); }

int fx1_isp_recover_ex(fx1_isp_dev *dev, uint32_t dma_err, fx1_isp_recovery *r) {
    fx1_isp_recovery info = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (!streaming(dev)) {
        return FX1_ISP_ESTATE;
    }
    /* Only errors still flagged in hardware: recover clears what it handles
     * (W1C), so a repeated call with an old interrupt snapshot is a no-op. */
    const uint32_t pending = dma_err & RD(dev, FX1_ISP_DMA_ERR_OFFSET) &
                             (FX1_ISP_DMA_ERR_ERR_AXI_RD_BIT | FX1_ISP_DMA_ERR_ERR_AXI_WR_BIT);
    if (pending) {
        /* The error started an automatic DMA-scope recovery (or joined a
         * running one): the queue is settled once it has finished. */
        if (!recovery_idle(dev) || settle_recovery(dev) != FX1_ISP_OK) {
            return FX1_ISP_EBUSY;
        }
        WR(dev, FX1_ISP_DMA_ERR_OFFSET,
           pending | ((pending & FX1_ISP_DMA_ERR_ERR_AXI_WR_BIT) ? FX1_ISP_DMA_ERR_ERR_ODMA_OVERFLOW_BIT : 0u));
        info.handled = pending;
        info.aborted = dev->rec_lost;
        info.sequence = dev->rec_seq;
        if ((pending & FX1_ISP_DMA_ERR_ERR_AXI_WR_BIT) && dev->rec_lost) {
            info.odma_failed = 1;
            info.odma_output = dev->rec_first.output;
            info.odma_seq = dev->rec_first.seq;
        }
    }
    if (r) {
        *r = info;
    }
    return info.handled ? FX1_ISP_OK : FX1_ISP_ENODATA;
}

int fx1_isp_recovery_report(fx1_isp_dev *dev, fx1_isp_abort_report *out) {
    unsigned i, attempt;
    if (!initialised(dev) || !out) {
        return initialised(dev) ? FX1_ISP_EINVAL : FX1_ISP_ESTATE;
    }
    /* Records are stable with BUSY = 0 and the same SEQUENCE around them. */
    for (attempt = 0; attempt < 4u; ++attempt) {
        const uint32_t seq = RD(dev, FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET);
        const uint32_t st = RD(dev, FX1_ISP_COMMON_RECOVERY_STATUS_OFFSET);
        if (st & FX1_ISP_COMMON_RECOVERY_STATUS_BUSY_MASK) {
            return FX1_ISP_EBUSY;
        }
        out->sequence = seq;
        out->count = (st & FX1_ISP_COMMON_RECOVERY_STATUS_COUNT_MASK) >> FX1_ISP_COMMON_RECOVERY_STATUS_COUNT_SHIFT;
        if (out->count > 4u) {
            out->count = 4u;
        }
        for (i = 0; i < 4u; ++i) {
            const uint32_t o = RD(dev, FX1_ISP_COMMON_ABORT0_OUTCOME_OFFSET + 8u * i);
            out->rec[i].frame = RD(dev, FX1_ISP_COMMON_ABORT0_FRAME_OFFSET + 8u * i);
            out->rec[i].failed_outputs =
                (o & FX1_ISP_COMMON_ABORT0_OUTCOME_FAILED_OUTPUTS_MASK) >> FX1_ISP_COMMON_ABORT0_OUTCOME_FAILED_OUTPUTS_SHIFT;
            out->rec[i].cause = (o & FX1_ISP_COMMON_ABORT0_OUTCOME_CAUSE_MASK) >> FX1_ISP_COMMON_ABORT0_OUTCOME_CAUSE_SHIFT;
        }
        if (RD(dev, FX1_ISP_COMMON_RECOVERY_SEQUENCE_OFFSET) == seq) {
            return seq ? FX1_ISP_OK : FX1_ISP_ENODATA;
        }
    }
    return FX1_ISP_EAGAIN;
}

int fx1_isp_next_lost(fx1_isp_dev *dev, uint32_t *seq, unsigned *input) {
    unsigned i;
    if (!streaming(dev)) {
        return FX1_ISP_ESTATE;
    }
    (void)settle_recovery(dev);
    if (!dev->lost_count) {
        return FX1_ISP_ENODATA;
    }
    if (seq) {
        *seq = dev->lost[0].seq;
    }
    if (input) {
        *input = dev->lost[0].input;
    }
    for (i = 0; i + 1u < dev->lost_count; ++i) {
        dev->lost[i] = dev->lost[i + 1u];
    }
    --dev->lost_count;
    return FX1_ISP_OK;
}

int fx1_isp_get_progress(fx1_isp_dev *dev, fx1_isp_progress *p) {
    if (!initialised(dev) || !p) {
        return initialised(dev) ? FX1_ISP_EINVAL : FX1_ISP_ESTATE;
    }
    p->idma_frames = RD(dev, FX1_ISP_IDMA_FRAME_COUNT_OFFSET);
    p->odma_frames = RD(dev, FX1_ISP_ODMA_FRAME_COUNT_OFFSET);
    p->in_flight = dev->in_flight;
    p->dma_err = RD(dev, FX1_ISP_DMA_ERR_OFFSET);
    p->pipeline_busy = (RD(dev, FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) != 0;
    p->isp_enabled = (RD(dev, FX1_ISP_COMMON_CTRL_OFFSET) & FX1_ISP_COMMON_CTRL_ISP_EN_MASK) != 0;
    return FX1_ISP_OK;
}

/* ---- Statistics --------------------------------------------------------------- */
static uint32_t sel_read(fx1_isp_dev *dev, uint32_t sel_off, uint32_t sel, uint32_t data_off) {
    WR(dev, sel_off, sel);
    return RD(dev, data_off);
}

/* ---- AEC and AF leases (MAS v1.07 §9.8) ----
 * ACQUIRE takes the oldest READY bank; the lease (and LEASE_VALID) is
 * readable one core clock later. LEASE_VALID = 0 after that clock means
 * nothing was taken (a reset in between). */
typedef struct stat_engine {
    uint32_t cmd, status, index, data_lo, data_hi, drops;
} stat_engine;
static const stat_engine aec_engine = {FX1_ISP_TUNE_AEC_CMD_OFFSET, FX1_ISP_TUNE_AEC_STATUS_OFFSET,
                                       FX1_ISP_TUNE_AEC_INDEX_OFFSET, FX1_ISP_TUNE_AEC_DATA_LO_OFFSET,
                                       FX1_ISP_TUNE_AEC_DATA_HI_OFFSET, FX1_ISP_TUNE_AEC_DROPS_OFFSET};
static const stat_engine af_engine = {FX1_ISP_TUNE_AF_CMD_OFFSET, FX1_ISP_TUNE_AF_STATUS_OFFSET,
                                      FX1_ISP_TUNE_AF_INDEX_OFFSET, FX1_ISP_TUNE_AF_DATA_LO_OFFSET,
                                      FX1_ISP_TUNE_AF_DATA_HI_OFFSET, FX1_ISP_TUNE_AF_DROPS_OFFSET};

static int stat_acquire(fx1_isp_dev *dev, const stat_engine *e, int *leased) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (*leased) {
        return FX1_ISP_EBUSY;
    }
    if (!(RD(dev, e->status) & FX1_ISP_TUNE_AEC_STATUS_READY_COUNT_MASK)) {
        return FX1_ISP_ENODATA;
    }
    /* LEASE_VALID rises one core clock after ACQUIRE: wait that clock, as
     * for AWB, so a CSR access shorter than a clock cannot miss the lease. */
    WR(dev, e->cmd, FX1_ISP_TUNE_AEC_CMD_ACQUIRE_MASK);
    delay(dev, 1u);
    if (!(RD(dev, e->status) & FX1_ISP_TUNE_AEC_STATUS_LEASE_VALID_MASK)) {
        return FX1_ISP_ENODATA;
    }
    *leased = 1;
    return FX1_ISP_OK;
}

/* The lease is still held. If not, a COMMON soft reset the driver did not
 * issue cleared every result: drop it. */
static int stat_alive(fx1_isp_dev *dev, const stat_engine *e, int *leased, int *last_valid) {
    if (RD(dev, e->status) & FX1_ISP_TUNE_AEC_STATUS_LEASE_VALID_MASK) {
        return 1;
    }
    *leased = 0;
    *last_valid = 0;
    return 0;
}

/* One 64-bit result: INDEX, then DATA_LO (snapshots the high word), DATA_HI. */
static uint64_t stat_wide(fx1_isp_dev *dev, const stat_engine *e, uint32_t index) {
    uint32_t lo;
    WR(dev, e->index, index);
    lo = RD(dev, e->data_lo);
    return ((uint64_t)RD(dev, e->data_hi) << 32) | lo;
}

static int stat_release(fx1_isp_dev *dev, const stat_engine *e, int *leased) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (!*leased) {
        return FX1_ISP_ESTATE;
    }
    WR(dev, e->cmd, FX1_ISP_TUNE_AEC_CMD_RELEASE_MASK);
    *leased = 0;
    return FX1_ISP_OK;
}

static int stat_drops(fx1_isp_dev *dev, const stat_engine *e, uint32_t *drops, int clear) {
    uint32_t d;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    d = RD(dev, e->drops);
    if (drops) {
        *drops = d;
    }
    if (clear && d) {
        WR(dev, e->drops, d);  /* a nonzero W1C clears the word; a new drop wins */
    }
    return FX1_ISP_OK;
}

int fx1_isp_aec_acquire(fx1_isp_dev *dev, fx1_isp_stat_lease *out) {
    uint32_t v;
    const int rc = stat_acquire(dev, &aec_engine, &dev->aec_leased);
    if (rc != FX1_ISP_OK) {
        return rc;
    }
    fx1_isp_stat_lease *l = &dev->aec_lease;
    l->frame_id = RD(dev, FX1_ISP_TUNE_AEC_FRAME_OFFSET);
    l->context_id = RD(dev, FX1_ISP_AEC_RESULT_CONTEXT_ID_OFFSET);
    l->profile_id = RD(dev, FX1_ISP_TUNE_AEC_PROFILE_OFFSET);
    v = RD(dev, FX1_ISP_TUNE_AEC_RESULT_META_OFFSET);
    l->source = (v & FX1_ISP_TUNE_AEC_RESULT_META_SOURCE_MASK) >> FX1_ISP_TUNE_AEC_RESULT_META_SOURCE_SHIFT;
    l->native_depth = (v & FX1_ISP_TUNE_AEC_RESULT_META_NATIVE_DEPTH_MASK) >> FX1_ISP_TUNE_AEC_RESULT_META_NATIVE_DEPTH_SHIFT;
    v = RD(dev, FX1_ISP_TUNE_AE_FRAME_SIZE_OFFSET);
    l->width = v & 0xFFFFu;
    l->height = v >> 16;
    v = RD(dev, FX1_ISP_TUNE_AE_GRID_OFFSET);
    l->nx = v & 0xFFFFu;
    l->ny = v >> 16;
    v = RD(dev, FX1_ISP_TUNE_AE_ZONE_SIZE_OFFSET);
    l->zone_w = v & 0xFFFFu;
    l->zone_h = v >> 16;
    if (out) {
        *out = *l;
    }
    return FX1_ISP_OK;
}

static int aec_index_valid(uint32_t i) {
    return i <= 8u || (i >= 0x10u && i <= 0x12u) || (i >= 0x100u && i < 0x180u) || (i >= 0x200u && i < 0x200u + 8u * 768u);
}

int fx1_isp_aec_result(fx1_isp_dev *dev, uint32_t index, uint64_t *value) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (!value || !aec_index_valid(index)) {
        return FX1_ISP_EINVAL;
    }
    if (!dev->aec_leased) {
        return FX1_ISP_ESTATE;
    }
    *value = stat_wide(dev, &aec_engine, index);
    return stat_alive(dev, &aec_engine, &dev->aec_leased, &dev->aec_last_valid) ? FX1_ISP_OK : FX1_ISP_ENODATA;
}

int fx1_isp_aec_read_global(fx1_isp_dev *dev, fx1_isp_aec_global *o) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (!o) {
        return FX1_ISP_EINVAL;
    }
    if (!dev->aec_leased) {
        return FX1_ISP_ESTATE;
    }
    o->sum = stat_wide(dev, &aec_engine, 0);
    o->count = stat_wide(dev, &aec_engine, 1);
    o->oe = stat_wide(dev, &aec_engine, 2);
    o->ue = stat_wide(dev, &aec_engine, 3);
    o->min = (uint32_t)stat_wide(dev, &aec_engine, 4);
    o->max = (uint32_t)stat_wide(dev, &aec_engine, 5);
    o->weighted_total = stat_wide(dev, &aec_engine, 6);
    o->weighted_sum = stat_wide(dev, &aec_engine, 7);
    o->overflow = (stat_wide(dev, &aec_engine, 8) & 1u) != 0;
    o->frame_id = dev->aec_lease.frame_id;
    o->context_id = dev->aec_lease.context_id;
    return stat_alive(dev, &aec_engine, &dev->aec_leased, &dev->aec_last_valid) ? FX1_ISP_OK : FX1_ISP_ENODATA;
}

int fx1_isp_aec_read_zones(fx1_isp_dev *dev, unsigned first, unsigned n, fx1_isp_aec_zone *zones) {
    unsigned i;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (!dev->aec_leased) {
        return FX1_ISP_ESTATE;
    }
    if ((n && !zones) || first + n > dev->aec_lease.nx * dev->aec_lease.ny) {
        return FX1_ISP_EINVAL;
    }
    for (i = 0; i < n; ++i) {
        const uint32_t base = 0x200u + 8u * (first + i);
        zones[i].sum = stat_wide(dev, &aec_engine, base);
        zones[i].count = stat_wide(dev, &aec_engine, base + 1u);
        zones[i].oe = stat_wide(dev, &aec_engine, base + 2u);
        zones[i].ue = stat_wide(dev, &aec_engine, base + 3u);
        zones[i].min = (uint32_t)stat_wide(dev, &aec_engine, base + 4u);
        zones[i].max = (uint32_t)stat_wide(dev, &aec_engine, base + 5u);
    }
    return stat_alive(dev, &aec_engine, &dev->aec_leased, &dev->aec_last_valid) ? FX1_ISP_OK : FX1_ISP_ENODATA;
}

int fx1_isp_aec_read_hist(fx1_isp_dev *dev, uint64_t hist[64], uint64_t *weighted) {
    unsigned i;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (!hist) {
        return FX1_ISP_EINVAL;
    }
    if (!dev->aec_leased) {
        return FX1_ISP_ESTATE;
    }
    for (i = 0; i < 64u; ++i) {
        hist[i] = stat_wide(dev, &aec_engine, 0x100u + i);
        if (weighted) {
            weighted[i] = stat_wide(dev, &aec_engine, 0x140u + i);
        }
    }
    return stat_alive(dev, &aec_engine, &dev->aec_leased, &dev->aec_last_valid) ? FX1_ISP_OK : FX1_ISP_ENODATA;
}

int fx1_isp_aec_release(fx1_isp_dev *dev) { return stat_release(dev, &aec_engine, &dev->aec_leased); }

int fx1_isp_aec_drops(fx1_isp_dev *dev, uint32_t *drops, int clear) { return stat_drops(dev, &aec_engine, drops, clear); }

int fx1_isp_read_aec_global(fx1_isp_dev *dev, fx1_isp_aec_global *o) {
    fx1_isp_aec_global g;
    unsigned guard;
    if (!initialised(dev) || !o) {
        return initialised(dev) ? FX1_ISP_EINVAL : FX1_ISP_ESTATE;
    }
    if (dev->aec_leased) {
        return FX1_ISP_EBUSY;
    }
    for (guard = 0; guard < 4u && fx1_isp_aec_acquire(dev, NULL) == FX1_ISP_OK; ++guard) {  /* two banks */
        if (fx1_isp_aec_read_global(dev, &g) == FX1_ISP_OK) {
            dev->aec_last = g;
            dev->aec_last_valid = 1;
        }
        if (dev->aec_leased) {
            fx1_isp_aec_release(dev);
        }
    }
    if (!dev->aec_last_valid) {
        return FX1_ISP_ENODATA;
    }
    *o = dev->aec_last;
    return FX1_ISP_OK;
}

int fx1_isp_af_acquire(fx1_isp_dev *dev, fx1_isp_stat_lease *out) {
    const int rc = stat_acquire(dev, &af_engine, &dev->af_leased);
    if (rc != FX1_ISP_OK) {
        return rc;
    }
    fx1_isp_stat_lease *l = &dev->af_lease;
    *l = (fx1_isp_stat_lease){0};
    l->frame_id = RD(dev, FX1_ISP_TUNE_AF_FRAME_OFFSET);
    l->context_id = RD(dev, FX1_ISP_AF_RESULT_CONTEXT_ID_OFFSET);
    l->profile_id = RD(dev, FX1_ISP_TUNE_AF_PROFILE_OFFSET);
    l->width = (unsigned)stat_wide(dev, &af_engine, 0x10u);
    l->height = (unsigned)stat_wide(dev, &af_engine, 0x11u);
    if (out) {
        *out = *l;
    }
    return FX1_ISP_OK;
}

int fx1_isp_af_read(fx1_isp_dev *dev, fx1_isp_af_window win[16], uint32_t *mask, unsigned *mode) {
    unsigned w;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (!win) {
        return FX1_ISP_EINVAL;
    }
    if (!dev->af_leased) {
        return FX1_ISP_ESTATE;
    }
    for (w = 0; w < 16u; ++w) {
        const uint64_t s = stat_wide(dev, &af_engine, 0x200u + 2u * w), e = stat_wide(dev, &af_engine, 0x201u + 2u * w);
        win[w].focus = stat_wide(dev, &af_engine, 0x100u + 4u * w);
        win[w].luma = stat_wide(dev, &af_engine, 0x101u + 4u * w);
        win[w].count = stat_wide(dev, &af_engine, 0x102u + 4u * w);
        win[w].x0 = (unsigned)(s & 0xFFFFu);
        win[w].y0 = (unsigned)((s >> 16) & 0xFFFFu);
        win[w].x1 = (unsigned)(e & 0xFFFFu);
        win[w].y1 = (unsigned)((e >> 16) & 0xFFFFu);
    }
    if (mask) {
        *mask = (uint32_t)stat_wide(dev, &af_engine, 0x12u);
    }
    if (mode) {
        *mode = (unsigned)stat_wide(dev, &af_engine, 0x13u);
    }
    return stat_alive(dev, &af_engine, &dev->af_leased, &dev->af_last_valid) ? FX1_ISP_OK : FX1_ISP_ENODATA;
}

int fx1_isp_af_release(fx1_isp_dev *dev) { return stat_release(dev, &af_engine, &dev->af_leased); }

int fx1_isp_af_drops(fx1_isp_dev *dev, uint32_t *drops, int clear) { return stat_drops(dev, &af_engine, drops, clear); }

static uint64_t rd_split(fx1_isp_dev *dev, uint32_t hi, uint32_t lo) {
    return ((uint64_t)(RD(dev, hi) & 0x7u) << 32) | RD(dev, lo);
}

/* The lease is still valid. If not, a COMMON soft reset the driver did not
 * issue has cleared every AWB result (MAS p248): drop the lease and the
 * cached result of the compatibility reader. */
static int awb_lease_alive(fx1_isp_dev *dev) {
    if (RD(dev, FX1_ISP_AWB_RESULT_STATUS_OFFSET) & FX1_ISP_AWB_RESULT_STATUS_READ_VALID_MASK) {
        return 1;
    }
    dev->awb_leased = 0;
    dev->awb_last_valid = 0;
    return 0;
}

int fx1_isp_awb_acquire(fx1_isp_dev *dev, fx1_isp_awb_lease *out) {
    uint32_t st;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (dev->awb_leased) {
        return FX1_ISP_EBUSY;
    }
    if (!(RD(dev, FX1_ISP_AWB_RESULT_STATUS_OFFSET) & FX1_ISP_AWB_RESULT_STATUS_READY_COUNT_MASK)) {
        return FX1_ISP_ENODATA;
    }
    /* READ_VALID (= LEASED) rises one core clock after ACQUIRE (MAS
     * §6.22.4.3): wait that clock before confirming. Then READ_VALID = 0
     * means no bank was taken (a reset in between); nothing is owned. */
    select_word(dev, FX1_ISP_AWB_RESULT_CTRL_OFFSET, FX1_ISP_AWB_RESULT_CTRL_ACQUIRE_MASK);
    st = RD(dev, FX1_ISP_AWB_RESULT_STATUS_OFFSET);
    if (!(st & FX1_ISP_AWB_RESULT_STATUS_READ_VALID_MASK)) {
        return FX1_ISP_ENODATA;
    }
    dev->awb_leased = 1;
    dev->awb_lease.bank = (st & FX1_ISP_AWB_RESULT_STATUS_READ_BANK_MASK) >> FX1_ISP_AWB_RESULT_STATUS_READ_BANK_SHIFT;
    dev->awb_lease.nx = (st & FX1_ISP_AWB_RESULT_STATUS_READ_NUM_ZONE_X_MASK) >> FX1_ISP_AWB_RESULT_STATUS_READ_NUM_ZONE_X_SHIFT;
    dev->awb_lease.ny = (st & FX1_ISP_AWB_RESULT_STATUS_READ_NUM_ZONE_Y_MASK) >> FX1_ISP_AWB_RESULT_STATUS_READ_NUM_ZONE_Y_SHIFT;
    dev->awb_lease.frame_id = RD(dev, FX1_ISP_AWB_FRAME_ID_OFFSET);
    dev->awb_lease.context_id = RD(dev, FX1_ISP_AWB_RESULT_CONTEXT_ID_OFFSET);
    if (out) {
        *out = dev->awb_lease;
    }
    return FX1_ISP_OK;
}

int fx1_isp_awb_read_global(fx1_isp_dev *dev, fx1_isp_awb_global *o) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (!o) {
        return FX1_ISP_EINVAL;
    }
    if (!dev->awb_leased) {
        return FX1_ISP_ESTATE;
    }
    o->sum[0] = rd_split(dev, FX1_ISP_AWB_GLOBAL_SUM_R_H_OFFSET, FX1_ISP_AWB_GLOBAL_SUM_R_L_OFFSET);
    o->sum[1] = rd_split(dev, FX1_ISP_AWB_GLOBAL_SUM_G_H_OFFSET, FX1_ISP_AWB_GLOBAL_SUM_G_L_OFFSET);
    o->sum[2] = rd_split(dev, FX1_ISP_AWB_GLOBAL_SUM_B_H_OFFSET, FX1_ISP_AWB_GLOBAL_SUM_B_L_OFFSET);
    o->count = RD(dev, FX1_ISP_AWB_GLOBAL_COUNT_OFFSET);
    o->frame_id = dev->awb_lease.frame_id;
    o->context_id = dev->awb_lease.context_id;
    /* The leased bank is immutable; only a reset can end the lease. */
    return awb_lease_alive(dev) ? FX1_ISP_OK : FX1_ISP_ENODATA;
}

int fx1_isp_awb_read_zones(fx1_isp_dev *dev, unsigned first, unsigned n, fx1_isp_awb_zone *zones) {
    unsigned i;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (n && !zones) {
        return FX1_ISP_EINVAL;
    }
    if (!dev->awb_leased) {
        return FX1_ISP_ESTATE;
    }
    if (first > dev->awb_lease.nx * dev->awb_lease.ny || n > dev->awb_lease.nx * dev->awb_lease.ny - first) {
        return FX1_ISP_EINVAL;  /* beyond the captured grid */
    }
    for (i = 0; i < n; ++i) {
        fx1_isp_awb_zone *z = &zones[i];
        select_word(dev, FX1_ISP_AWB_ZONE_ADDR_OFFSET, first + i);
        z->count = RD(dev, FX1_ISP_AWB_ZONE_COUNT_OFFSET);
        z->sum[0] = rd_split(dev, FX1_ISP_AWB_ZONE_SUM_R_H_OFFSET, FX1_ISP_AWB_ZONE_SUM_R_L_OFFSET);
        z->sum[1] = rd_split(dev, FX1_ISP_AWB_ZONE_SUM_G_H_OFFSET, FX1_ISP_AWB_ZONE_SUM_G_L_OFFSET);
        z->sum[2] = rd_split(dev, FX1_ISP_AWB_ZONE_SUM_B_H_OFFSET, FX1_ISP_AWB_ZONE_SUM_B_L_OFFSET);
    }
    return awb_lease_alive(dev) ? FX1_ISP_OK : FX1_ISP_ENODATA;
}

int fx1_isp_awb_release(fx1_isp_dev *dev) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (!dev->awb_leased) {
        return FX1_ISP_ESTATE;
    }
    /* A bank released on an SOF edge is eligible only from the next edge
     * (MAS §6.22.4.3): wait that clock, so a frame the caller starts next can
     * use the bank. */
    select_word(dev, FX1_ISP_AWB_RESULT_CTRL_OFFSET, FX1_ISP_AWB_RESULT_CTRL_RELEASE_MASK);
    dev->awb_leased = 0;
    return FX1_ISP_OK;
}

int fx1_isp_awb_report(fx1_isp_dev *dev, fx1_isp_awb_status *o, int clear) {
    uint32_t st;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (!o) {
        return FX1_ISP_EINVAL;
    }
    st = RD(dev, FX1_ISP_AWB_RESULT_STATUS_OFFSET);
    o->ready_count = (st & FX1_ISP_AWB_RESULT_STATUS_READY_COUNT_MASK) >> FX1_ISP_AWB_RESULT_STATUS_READY_COUNT_SHIFT;
    o->leased = (st & FX1_ISP_AWB_RESULT_STATUS_LEASED_MASK) != 0;
    o->write_busy = (st & FX1_ISP_AWB_RESULT_STATUS_WRITE_BUSY_MASK) != 0;
    o->overrun = (st & FX1_ISP_AWB_RESULT_STATUS_OVERRUN_MASK) != 0;
    o->drop_count = RD(dev, FX1_ISP_AWB_DROP_COUNT_OFFSET);
    if (clear) {
        WR(dev, FX1_ISP_AWB_RESULT_CTRL_OFFSET, FX1_ISP_AWB_RESULT_CTRL_CLEAR_REPORT_MASK);
    }
    return FX1_ISP_OK;
}

int fx1_isp_read_awb_global(fx1_isp_dev *dev, fx1_isp_awb_global *o) {
    unsigned guard;
    if (!initialised(dev) || !o) {
        return initialised(dev) ? FX1_ISP_EINVAL : FX1_ISP_ESTATE;
    }
    if (dev->awb_leased) {
        return FX1_ISP_EBUSY;  /* the caller holds a result through the explicit API */
    }
    /* Drain every ready result, oldest first, keeping the newest. At most two
     * banks exist; the guard bounds the loop if frames keep completing. */
    for (guard = 0; guard < 4u && fx1_isp_awb_acquire(dev, NULL) == FX1_ISP_OK; ++guard) {
        fx1_isp_awb_global g;
        const int rc = fx1_isp_awb_read_global(dev, &g);
        if (dev->awb_leased) {
            fx1_isp_awb_release(dev);
        }
        if (rc == FX1_ISP_OK) {
            dev->awb_last = g;
            dev->awb_last_valid = 1;
        }
    }
    if (!dev->awb_last_valid) {
        return FX1_ISP_ENODATA;
    }
    *o = dev->awb_last;
    return FX1_ISP_OK;
}

int fx1_isp_read_af(fx1_isp_dev *dev, fx1_isp_af_scores *o) {
    unsigned z, guard;
    if (!initialised(dev) || !o) {
        return initialised(dev) ? FX1_ISP_EINVAL : FX1_ISP_ESTATE;
    }
    if (dev->af_leased) {
        return FX1_ISP_EBUSY;
    }
    for (guard = 0; guard < 4u && fx1_isp_af_acquire(dev, NULL) == FX1_ISP_OK; ++guard) {  /* two banks */
        fx1_isp_af_scores s;
        for (z = 0; z < 16u; ++z) {
            s.fv[z] = sel_read(dev, FX1_ISP_AF_STAT_ADDR_OFFSET, z, FX1_ISP_AF_STAT_DATA_OFFSET);
        }
        s.score_valid = 1;
        s.frame_id = dev->af_lease.frame_id;
        s.context_id = dev->af_lease.context_id;
        if (stat_alive(dev, &af_engine, &dev->af_leased, &dev->af_last_valid)) {
            dev->af_last = s;
            dev->af_last_valid = 1;
            fx1_isp_af_release(dev);
        }
    }
    if (!dev->af_last_valid) {
        return FX1_ISP_ENODATA;
    }
    *o = dev->af_last;
    return FX1_ISP_OK;
}
