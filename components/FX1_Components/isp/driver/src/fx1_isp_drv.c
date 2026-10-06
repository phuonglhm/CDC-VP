/* SPDX-License-Identifier: Apache-2.0 */
/* FX1 ISP reference driver; see fx1_isp_drv.h and docs/ISP_PROGRAMMING_GUIDE.md. */

#include "fx1_isp/fx1_isp_drv.h"

#include "fx1_isp/fx1_isp_csr.h"

#define RD(dev, off) ((dev)->hal.read32((dev)->hal.ctx, (off)))
#define WR(dev, off, v) ((dev)->hal.write32((dev)->hal.ctx, (off), (v)))

#define MAX_WIDTH 3840u      /* PARA_MAX_H_ACTIVE */
#define MAX_HEIGHT 2160u     /* PARA_MAX_V_ACTIVE */
#define LSC_MESH_MAX 32u     /* PARA_LSC_MESH_MAX (DEC-31) */
#define SOFT_RESET_CYCLES 32u
#define STATS_RETRIES 4
#define DRV_MAGIC 0x46583149u /* "FX1I": set by fx1_isp_init */

static int initialised(const fx1_isp_dev *dev) { return dev && dev->magic == DRV_MAGIC; }
static int streaming(const fx1_isp_dev *dev) { return initialised(dev) && dev->state == FX1_ISP_STATE_STREAMING; }

/* Zone/histogram memory is in place (DEC-30): after an abort or a reset it
 * holds partial or cleared data under the previous FRAME_ID (M3-A7). Readers
 * refuse it until a new AEC publication moves FRAME_ID. */
/* Every event moves the mark to the current FRAME_ID: an older mark could
 * already be passed by a publication nobody read. No publication can happen
 * between an IDMA abort and its re-arm (the IDMA stops on the failed input). */
static void stats_untrust(fx1_isp_dev *dev, int aborted) {
    /* FRAME_ID 0 can also mean the first frame was aborted after writing
     * partial zone/histogram data. It is not a valid publication. */
    dev->stats_mark = RD(dev, FX1_ISP_AEC_FRAME_ID_OFFSET);
    dev->stats_untrusted = aborted || dev->stats_mark != 0u;
}

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

static void write_addr(fx1_isp_dev *dev, uint32_t lo_off, uint64_t addr) {
    WR(dev, lo_off, (uint32_t)addr);
    WR(dev, lo_off + 4u, (uint32_t)(addr >> 32));
}

/* Rotation positions restart at 0 after i_rst_n and soft reset (M2-A1). The
 * frame counters are kept, so the driver remembers where it started. */
static void restart_rotation(fx1_isp_dev *dev) {
    dev->in_next = dev->out_next = dev->out_done = 0;
    dev->in_flight = 0;
    dev->in_failed = 0;
    dev->lost_count = 0;
}

/* Queue position of the frame the IDMA failed on: in queue order, the inputs
 * the IDMA retired and the one it released have VALID clear, later ones are
 * still VALID. Returns -1 if none. */
static int idma_failed_pos(const fx1_isp_dev *dev, uint32_t valid) {
    int found = -1;
    unsigned k;
    for (k = 0; k < dev->in_flight; ++k) {
        if (!(valid & (1u << dev->queue[k].input))) {
            found = (int)k;
        }
    }
    return found;
}

/* A frame whose output failed is lost once the IDMA has retired its input
 * without an error on it; it is then removed and the outputs of the later
 * frames move up one buffer, as the ODMA kept its rotation position (DEC-19).
 * VALID is read before DMA_ERR: an IDMA error releases VALID and flags
 * IDMA_AXI together, so an error on the frame cannot be missed. */
static unsigned settle_losses(fx1_isp_dev *dev) {
    unsigned k = 0, n = 0;
    while (k < dev->in_flight) {
        const fx1_isp_frame f = dev->queue[k];
        if (!f.odma_failed) {
            ++k;
            continue;
        }
        const uint32_t valid = RD(dev, FX1_ISP_IDMA_BUF_VALID_OFFSET);
        if (valid & (1u << f.input)) {
            ++k;  /* the IDMA is still reading it */
            continue;
        }
        if ((RD(dev, FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT) && idma_failed_pos(dev, valid) == (int)k) {
            ++k;  /* the IDMA failed on this frame: recover retries it */
            continue;
        }
        queue_remove(dev, k);
        dev->out_next = (dev->out_next + FX1_ISP_NUM_BUFFERS - 1u) % FX1_ISP_NUM_BUFFERS;
        if (dev->lost_count < 4u) {
            dev->lost[dev->lost_count++] = f;
        }
        ++n;
    }
    return n;
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
    fx1_isp_soft_reset(dev);
    return FX1_ISP_OK;
}

int fx1_isp_stop(fx1_isp_dev *dev) {
    if (!streaming(dev)) {
        return FX1_ISP_ESTATE;
    }
    const int dropped = (int)dev->in_flight;
    /* Disable before resetting: if DMA_EN survives into the 32-cycle window,
     * IDMA sees no VALID buffer afterwards and reasserts UNDERRUN. */
    WR(dev, FX1_ISP_DMA_CTRL_OFFSET, dev->dma_ctrl & ~(FX1_ISP_DMA_CTRL_IDMA_EN_MASK | FX1_ISP_DMA_CTRL_ODMA_EN_MASK));
    fx1_isp_soft_reset(dev);  /* frames in flight are discarded (DEC-14) */
    WR(dev, FX1_ISP_COMMON_CTRL_OFFSET, 0);  /* CONFIG state: pipeline disabled */
    dev->started = 0;
    dev->state = FX1_ISP_STATE_CONFIG;
    return dropped;
}

void fx1_isp_soft_reset(fx1_isp_dev *dev) {
    if (!initialised(dev)) {
        return;
    }
    /* COMMON_CTRL.soft_rst is W1SC; isp_en is RW and written back unchanged. */
    const uint32_t ctrl = RD(dev, FX1_ISP_COMMON_CTRL_OFFSET) & FX1_ISP_COMMON_CTRL_ISP_EN_MASK;
    WR(dev, FX1_ISP_COMMON_CTRL_OFFSET, ctrl | FX1_ISP_COMMON_CTRL_SOFT_RST_MASK);
    delay(dev, SOFT_RESET_CYCLES + 8u); /* W1S/W1C writes are ignored during the window */
    WR(dev, FX1_ISP_COMMON_IRQ_STATUS_OFFSET, 0xFFFFFFFFu);
    WR(dev, FX1_ISP_DMA_IRQ_STAT_OFFSET, 0xFFFFFFFFu);
    WR(dev, FX1_ISP_DMA_ERR_OFFSET, 0xFFFFFFFFu);
    restart_rotation(dev);
    stats_untrust(dev, 0);
}

int fx1_isp_set_geometry(fx1_isp_dev *dev, uint32_t width, uint32_t height, uint32_t bayer) {
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (width < 2u || width > MAX_WIDTH || (width & 1u) || height < 2u || height > MAX_HEIGHT || (height & 1u) ||
        bayer > 3u) {
        return FX1_ISP_EINVAL;
    }
    if ((RD(dev, FX1_ISP_COMMON_STATUS_OFFSET) & FX1_ISP_COMMON_STATUS_BUSY_MASK) || dev->in_flight) {
        return FX1_ISP_EBUSY; /* HAS §9.4.4: geometry changes with the pipeline idle */
    }
    WR(dev, FX1_ISP_COMMON_FRAME_WIDTH_OFFSET, width);
    WR(dev, FX1_ISP_COMMON_FRAME_HEIGHT_OFFSET, height);
    WR(dev, FX1_ISP_COMMON_BAYER_OFFSET, bayer);
    return FX1_ISP_OK;
}

void fx1_isp_apply(fx1_isp_dev *dev, const fx1_isp_reg_write *writes, size_t n) {
    size_t i;
    if (!initialised(dev) || !writes) {
        return;
    }
    for (i = 0; i < n; ++i) {
        WR(dev, writes[i].offset, writes[i].value);
    }
}

void fx1_isp_load_gamma(fx1_isp_dev *dev, const uint16_t lut[256]) {
    unsigned i;
    if (!initialised(dev) || !lut) {
        return;
    }
    WR(dev, FX1_ISP_GAMMA_LUT_ADDR_OFFSET, 0);  /* the address auto-increments per DATA write */
    for (i = 0; i < 256u; ++i) {
        WR(dev, FX1_ISP_GAMMA_LUT_DATA_OFFSET, lut[i]);
    }
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

void fx1_isp_load_gtm_lut(fx1_isp_dev *dev, const uint16_t lut[65]) {
    unsigned i;
    if (!initialised(dev) || !lut) {
        return;
    }
    const uint32_t ctrl = RD(dev, FX1_ISP_GTM_CTRL_OFFSET);
    WR(dev, FX1_ISP_GTM_CTRL_OFFSET, ctrl | FX1_ISP_GTM_CTRL_MANUAL_MASK);  /* writes accepted in manual mode */
    WR(dev, FX1_ISP_GTM_LUT_ADDR_OFFSET, 0);
    for (i = 0; i < 65u; ++i) {
        WR(dev, FX1_ISP_GTM_LUT_DATA_OFFSET, lut[i]);
    }
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
    if (err) {  /* e.g. ACTIVE_LOAD_REJECT: the target is the active, valid profile */
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
    /* Close the gate, write the set, then open it: the set is copied as a
     * whole (DEC-24), at once if idle or at the next SOF (DEC-26). The gate
     * stays open until the next update; hardware never clears it. */
    WR(dev, FX1_ISP_CCM_CTRL_OFFSET, en);
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
    const uint32_t out_width = RD(dev, FX1_ISP_RESIZER_OUT_W_OFFSET);
    const uint32_t out_height = RD(dev, FX1_ISP_RESIZER_OUT_H_OFFSET);
    if (b->y_stride < out_width || b->uv_stride < out_width) {
        WR(dev, FX1_ISP_COMMON_CTRL_OFFSET, old_isp_en);  /* failed start has no lasting enable */
        return FX1_ISP_EINVAL;
    }
    dev->out_width = out_width;
    dev->out_height = out_height;
    WR(dev, FX1_ISP_COMMON_IRQ_EN_OFFSET, FX1_ISP_COMMON_IRQ_EN_FRAME_DONE_EN_MASK | FX1_ISP_COMMON_IRQ_EN_ERROR_EN_MASK |
                                              FX1_ISP_COMMON_IRQ_EN_STATS_READY_EN_MASK);
    /* IDMA_DONE as well: it is when a frame whose output failed becomes
     * certainly lost (settle_losses). */
    WR(dev, FX1_ISP_DMA_IRQ_EN_OFFSET, FX1_ISP_DMA_IRQ_EN_IRQ_EN_ODMA_DONE_BIT | FX1_ISP_DMA_IRQ_EN_IRQ_EN_AXI_ERROR_BIT |
                                           FX1_ISP_DMA_IRQ_EN_IRQ_EN_ODMA_OVERFLOW_BIT |
                                           FX1_ISP_DMA_IRQ_EN_IRQ_EN_IDMA_DONE_BIT);
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
    dev->state = FX1_ISP_STATE_STREAMING;
    return FX1_ISP_OK;
}

unsigned fx1_isp_next_input(const fx1_isp_dev *dev) { return dev->in_next; }

int fx1_isp_input_ready(fx1_isp_dev *dev) {
    if (!streaming(dev)) {
        return 0;
    }
    settle_losses(dev);  /* a loss decided now frees its in-flight slot */
    return dev->in_flight < FX1_ISP_NUM_BUFFERS &&
           !(RD(dev, FX1_ISP_IDMA_BUF_VALID_OFFSET) & (1u << dev->in_next));
}

int fx1_isp_queue_frame(fx1_isp_dev *dev) { return fx1_isp_queue_frame_ex(dev, NULL); }

int fx1_isp_queue_frame_ex(fx1_isp_dev *dev, uint32_t *seq) {
    if (!streaming(dev)) {
        return FX1_ISP_ESTATE;
    }
    /* An early ODMA failure can release an output slot while IDMA is still
     * reading the corresponding input. Never re-arm a VALID input. */
    if (!fx1_isp_input_ready(dev)) {
        return FX1_ISP_EBUSY;
    }
    /* Output first, so the ODMA is armed when the frame arrives (SPEC-07). */
    WR(dev, FX1_ISP_ODMA_BUF_FREE_OFFSET, 1u << dev->out_next);
    WR(dev, FX1_ISP_IDMA_BUF_VALID_OFFSET, 1u << dev->in_next);
    dev->queue[dev->in_flight].seq = dev->next_seq;
    dev->queue[dev->in_flight].input = dev->in_next;
    dev->queue[dev->in_flight].odma_failed = 0;
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
    ev->common = ev->dma = ev->dma_err = 0;
    ev->frames_done = ev->frames_lost = 0;
    if (!initialised(dev)) {
        return;
    }
    ev->common = RD(dev, FX1_ISP_COMMON_IRQ_STATUS_OFFSET);
    ev->dma = RD(dev, FX1_ISP_DMA_IRQ_STAT_OFFSET);
    ev->dma_err = RD(dev, FX1_ISP_DMA_ERR_OFFSET);
    /* W1C, per bit: a source that fires again between the read and the
     * write is set again by hardware (hardware set wins, HAS Table 7-10). */
    WR(dev, FX1_ISP_COMMON_IRQ_STATUS_OFFSET, ev->common);
    WR(dev, FX1_ISP_DMA_IRQ_STAT_OFFSET, ev->dma);
    if (ev->dma_err & FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT) {
        stats_untrust(dev, 1);  /* an input frame was aborted: zone data may be partial */
    }
    if (dev->state == FX1_ISP_STATE_STREAMING) {
        settle_losses(dev);
    }
    ev->frames_lost = dev->lost_count;
    const uint32_t done = RD(dev, FX1_ISP_ODMA_BUF_DONE_OFFSET);
    for (k = 0; k < dev->in_flight; ++k) {  /* completed buffers, in rotation order */
        if (!(done & (1u << ((dev->out_done + k) % FX1_ISP_NUM_BUFFERS)))) {
            break;
        }
        ++n;
    }
    ev->frames_done = n;
}

int fx1_isp_next_done(fx1_isp_dev *dev, unsigned *index) { return fx1_isp_next_done_ex(dev, index, NULL); }

int fx1_isp_next_done_ex(fx1_isp_dev *dev, unsigned *index, uint32_t *seq) {
    if (!streaming(dev) || !index) {
        return streaming(dev) ? FX1_ISP_EINVAL : FX1_ISP_ESTATE;
    }
    settle_losses(dev);  /* positions must be final before an output is matched to a frame */
    if (!dev->in_flight || !(RD(dev, FX1_ISP_ODMA_BUF_DONE_OFFSET) & (1u << dev->out_done))) {
        return FX1_ISP_ENODATA;
    }
    *index = dev->out_done;
    WR(dev, FX1_ISP_ODMA_BUF_DONE_OFFSET, 1u << dev->out_done);  /* returned to software */
    dev->out_done = (dev->out_done + 1u) % FX1_ISP_NUM_BUFFERS;
    const fx1_isp_frame f = queue_remove(dev, 0);
    if (seq) {
        *seq = f.seq;
    }
    return FX1_ISP_OK;
}

int fx1_isp_recover(fx1_isp_dev *dev, uint32_t dma_err) { return fx1_isp_recover_ex(dev, dma_err, NULL); }

int fx1_isp_recover_ex(fx1_isp_dev *dev, uint32_t dma_err, fx1_isp_recovery *r) {
    fx1_isp_recovery info = {0, 0, 0, 0, 0, 0, 0};
    unsigned k;
    if (!streaming(dev)) {
        return FX1_ISP_ESTATE;
    }
    /* Only errors still flagged in hardware: recover clears what it handles
     * (W1C), so a repeated call with an old interrupt snapshot is a no-op. */
    const uint32_t pending = dma_err & RD(dev, FX1_ISP_DMA_ERR_OFFSET) &
                             (FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT | FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT);
    if (pending & FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT) {
        /* The output buffer was neither completed nor left free, and the ODMA
         * kept its rotation position: free the same index again. The failed
         * buffer is the first in-flight one, in rotation order from the next
         * one to collect, whose DONE bit is clear: frames before it may be
         * DONE and not yet collected when the handler runs late (M5-R5).
         * Whether the frame is lost is decided by settle_losses. */
        const uint32_t done = RD(dev, FX1_ISP_ODMA_BUF_DONE_OFFSET);
        for (k = 0; k < dev->in_flight; ++k) {
            if (!(done & (1u << ((dev->out_done + k) % FX1_ISP_NUM_BUFFERS)))) {
                break;
            }
        }
        WR(dev, FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT | FX1_ISP_DMA_ERR_ERR_ODMA_OVERFLOW_BIT);
        if (k < dev->in_flight) {
            info.odma_output = (dev->out_done + k) % FX1_ISP_NUM_BUFFERS;
            WR(dev, FX1_ISP_ODMA_BUF_FREE_OFFSET, 1u << info.odma_output);
            dev->queue[k].odma_failed = 1;
            info.odma_failed = 1;
            info.odma_seq = dev->queue[k].seq;
        }
        info.handled |= FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT;
    }
    if (pending & FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT) {
        /* The IDMA released the input it was reading and kept its rotation
         * position: re-arm the same index (DEC-19). The aborted frame left
         * partial zone/histogram data behind (M3-A7). */
        const int pos = idma_failed_pos(dev, RD(dev, FX1_ISP_IDMA_BUF_VALID_OFFSET));
        stats_untrust(dev, 1);
        if (pos >= 0) {
            fx1_isp_frame *f = &dev->queue[pos];
            f->odma_failed = 0;  /* retried from its input: its output will be produced */
            dev->in_failed = f->input;
            info.idma_rearmed = 1;
            info.idma_input = f->input;
            info.idma_seq = f->seq;
            /* Losses before it are settled while it is still the failed one. */
            settle_losses(dev);
            WR(dev, FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT);
            WR(dev, FX1_ISP_IDMA_BUF_VALID_OFFSET, 1u << info.idma_input);
        } else {
            WR(dev, FX1_ISP_DMA_ERR_OFFSET, FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT);
        }
        info.handled |= FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT;
    }
    settle_losses(dev);
    if (r) {
        *r = info;
    }
    return info.handled ? FX1_ISP_OK : FX1_ISP_ENODATA;
}

int fx1_isp_next_lost(fx1_isp_dev *dev, uint32_t *seq, unsigned *input) {
    unsigned i;
    if (!streaming(dev)) {
        return FX1_ISP_ESTATE;
    }
    settle_losses(dev);
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

int fx1_isp_read_aec_global(fx1_isp_dev *dev, fx1_isp_aec_global *o) {
    int t;
    unsigned c;
    if (!initialised(dev) || !o) {
        return initialised(dev) ? FX1_ISP_EINVAL : FX1_ISP_ESTATE;
    }
    for (t = 0; t < STATS_RETRIES; ++t) {  /* read-tag-read, HAS §6.21.9 */
        const uint32_t id = RD(dev, FX1_ISP_AEC_FRAME_ID_OFFSET);
        for (c = 0; c < 4u; ++c) {
            WR(dev, FX1_ISP_AEC_CHANNEL_SEL_OFFSET, c);
            const uint32_t lo = RD(dev, FX1_ISP_AEC_GLOBAL_SUM_LO_OFFSET);
            const uint32_t hi = RD(dev, FX1_ISP_AEC_GLOBAL_SUM_HI_OFFSET);
            o->sum[c] = ((uint64_t)hi << 32) | lo;
            o->count[c] = RD(dev, FX1_ISP_AEC_GLOBAL_COUNT_OFFSET);
        }
        o->context_id = RD(dev, FX1_ISP_AEC_RESULT_CONTEXT_ID_OFFSET);
        if (RD(dev, FX1_ISP_AEC_FRAME_ID_OFFSET) == id) {
            o->frame_id = id;
            return FX1_ISP_OK;
        }
    }
    return FX1_ISP_EAGAIN;
}

int fx1_isp_read_aec_zones(fx1_isp_dev *dev, unsigned first, unsigned n, fx1_isp_aec_zone *zones, uint32_t hist[64],
                           uint32_t *frame_id) {
    unsigned i, c;
    if (!initialised(dev)) {
        return FX1_ISP_ESTATE;
    }
    if (n && !zones) {
        return FX1_ISP_EINVAL;
    }
    /* The zone and histogram memory is rewritten from the next enabled SOF
     * (DEC-30): FRAME_ID alone cannot detect that, so busy must be 0 before
     * and after the read (ALG-STAT-02). */
    const uint32_t id = RD(dev, FX1_ISP_AEC_FRAME_ID_OFFSET);
    if (RD(dev, FX1_ISP_AEC_STATUS_OFFSET) & FX1_ISP_AEC_STATUS_BUSY_MASK) {
        return FX1_ISP_EAGAIN;
    }
    /* A caller may poll statistics before servicing the error IRQ. The
     * hardware error is visible even when the driver has not marked it yet. */
    if (RD(dev, FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT) {
        stats_untrust(dev, 1);
        return FX1_ISP_ENODATA;
    }
    if (dev->stats_untrusted) {
        if (id == dev->stats_mark) {
            return FX1_ISP_ENODATA;  /* partial/cleared data of an aborted frame or a reset */
        }
        dev->stats_untrusted = 0;    /* a new frame was published since */
    }
    for (i = 0; i < n; ++i) {
        fx1_isp_aec_zone *z = &zones[i];
        WR(dev, FX1_ISP_AEC_ZONE_ADDR_OFFSET, first + i);
        const uint32_t oeue = RD(dev, FX1_ISP_AEC_ZONE_GREEN_OE_UE_OFFSET);
        const uint32_t mm = RD(dev, FX1_ISP_AEC_ZONE_GREEN_MIN_MAX_OFFSET);
        z->green_ue = oeue & 0xFFFFu;
        z->green_oe = oeue >> 16;
        z->green_min = mm & 0xFFFu;
        z->green_max = (mm >> 12) & 0xFFFu;
        for (c = 0; c < 4u; ++c) {
            WR(dev, FX1_ISP_AEC_CHANNEL_SEL_OFFSET, c);
            z->sum[c] = RD(dev, FX1_ISP_AEC_ZONE_SUM_OFFSET);
            z->count[c] = RD(dev, FX1_ISP_AEC_ZONE_COUNT_OFFSET);
        }
    }
    if (hist) {
        for (i = 0; i < 64u; ++i) {
            hist[i] = sel_read(dev, FX1_ISP_AEC_HIST_ADDR_OFFSET, i, FX1_ISP_AEC_HIST_DATA_OFFSET);
        }
    }
    if (RD(dev, FX1_ISP_DMA_ERR_OFFSET) & FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT) {
        stats_untrust(dev, 1);
        return FX1_ISP_ENODATA;
    }
    if ((RD(dev, FX1_ISP_AEC_STATUS_OFFSET) & FX1_ISP_AEC_STATUS_BUSY_MASK) || RD(dev, FX1_ISP_AEC_FRAME_ID_OFFSET) != id) {
        return FX1_ISP_EAGAIN;
    }
    if (frame_id) {
        *frame_id = id;
    }
    return FX1_ISP_OK;
}

int fx1_isp_read_awb_global(fx1_isp_dev *dev, fx1_isp_awb_global *o) {
    int t;
    if (!initialised(dev) || !o) {
        return initialised(dev) ? FX1_ISP_EINVAL : FX1_ISP_ESTATE;
    }
    for (t = 0; t < STATS_RETRIES; ++t) {
        const uint32_t id = RD(dev, FX1_ISP_AWB_FRAME_ID_OFFSET);
        o->sum[0] = ((uint64_t)RD(dev, FX1_ISP_AWB_GLOBAL_SUM_R_H_OFFSET) << 32) | RD(dev, FX1_ISP_AWB_GLOBAL_SUM_R_L_OFFSET);
        o->sum[1] = ((uint64_t)RD(dev, FX1_ISP_AWB_GLOBAL_SUM_G_H_OFFSET) << 32) | RD(dev, FX1_ISP_AWB_GLOBAL_SUM_G_L_OFFSET);
        o->sum[2] = ((uint64_t)RD(dev, FX1_ISP_AWB_GLOBAL_SUM_B_H_OFFSET) << 32) | RD(dev, FX1_ISP_AWB_GLOBAL_SUM_B_L_OFFSET);
        o->count = RD(dev, FX1_ISP_AWB_GLOBAL_COUNT_OFFSET);
        o->context_id = RD(dev, FX1_ISP_AWB_RESULT_CONTEXT_ID_OFFSET);
        if (RD(dev, FX1_ISP_AWB_FRAME_ID_OFFSET) == id) {
            o->frame_id = id;
            return FX1_ISP_OK;
        }
    }
    return FX1_ISP_EAGAIN;
}

int fx1_isp_read_af(fx1_isp_dev *dev, fx1_isp_af_scores *o) {
    int t;
    unsigned z;
    if (!initialised(dev) || !o) {
        return initialised(dev) ? FX1_ISP_EINVAL : FX1_ISP_ESTATE;
    }
    for (t = 0; t < STATS_RETRIES; ++t) {
        const uint32_t id = RD(dev, FX1_ISP_AF_FRAME_ID_OFFSET);
        for (z = 0; z < 16u; ++z) {
            o->fv[z] = sel_read(dev, FX1_ISP_AF_STAT_ADDR_OFFSET, z, FX1_ISP_AF_STAT_DATA_OFFSET);
        }
        o->score_valid = (RD(dev, FX1_ISP_AF_STATUS_OFFSET) & FX1_ISP_AF_STATUS_SCORE_VALID_MASK) != 0;
        o->context_id = RD(dev, FX1_ISP_AF_RESULT_CONTEXT_ID_OFFSET);
        if (RD(dev, FX1_ISP_AF_FRAME_ID_OFFSET) == id) {
            o->frame_id = id;
            return FX1_ISP_OK;
        }
    }
    return FX1_ISP_EAGAIN;
}
