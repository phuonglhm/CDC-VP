/* FX1 MIPI CSI-2 RX reference driver. See fx1_csi2_drv.h. */
#include "fx1_csi2/fx1_csi2_drv.h"

#include "fx1_csi2/fx1_csi2_regs.h"

#define FX1_CSI2_MAGIC 0x43534932u /* "CSI2" */
#define FIELD(v, reg, f) (((v) & FX1_CSI2_##reg##_##f##_MASK) >> FX1_CSI2_##reg##_##f##_SHIFT)

static uint32_t rd(fx1_csi2_dev *d, uint32_t off) { return d->hal.read32(d->hal.ctx, off); }
static void wr(fx1_csi2_dev *d, uint32_t off, uint32_t v) { d->hal.write32(d->hal.ctx, off, v); }
static int valid(const fx1_csi2_dev *d) { return d && d->magic == FX1_CSI2_MAGIC; }

/* Polls (reg & mask) == want every poll_us, spending the caller's budget:
 * *budget_us is the time left for the whole public call, decreased by every
 * delay requested here, so successive waits of one call never exceed its
 * timeout together (review M4b F1). Each step is min(poll_us, budget left),
 * nothing can wrap; poll_us = 0 is taken as 1 us. */
static int wait_for(fx1_csi2_dev *d, uint32_t off, uint32_t mask, uint32_t want, uint32_t *budget_us) {
    const uint32_t poll = d->poll_us ? d->poll_us : 1u;
    for (;;) {
        uint32_t step;
        if ((rd(d, off) & mask) == want) return FX1_CSI2_OK;
        if (*budget_us == 0) return FX1_CSI2_ETIMEDOUT;
        step = poll < *budget_us ? poll : *budget_us;
        d->hal.delay_us(d->hal.ctx, step);
        *budget_us -= step;
    }
}

static int wait_reset_done(fx1_csi2_dev *d, uint32_t *budget_us) {
    return wait_for(d, FX1_CSI2_CORE_STATUS, FX1_CSI2_CORE_STATUS_RESET_IN_PROGRESS_MASK, 0, budget_us);
}

int fx1_csi2_probe(fx1_csi2_dev *dev, const fx1_csi2_hal *hal) {
    uint32_t enc;
    if (!dev || !hal || !hal->read32 || !hal->write32 || !hal->delay_us) return FX1_CSI2_EINVAL;
    dev->hal = *hal;
    dev->magic = 0;
    dev->lanes = 0;
    if (!dev->poll_us) dev->poll_us = 10;
    dev->version = rd(dev, FX1_CSI2_IP_VERSION);
    /* D01: same major and minor revision. */
    if ((dev->version >> 8) != (FX1_CSI2_SUPPORTED_VERSION >> 8)) return FX1_CSI2_ENODEV;
    enc = FIELD(rd(dev, FX1_CSI2_PROTOCOL_CONFIG), PROTOCOL_CONFIG, MAX_LANES);
    dev->max_lanes = enc == 0 ? 1u : enc == 1 ? 2u : enc == 3 ? 4u : 0u;
    if (!dev->max_lanes) return FX1_CSI2_ENODEV;
    dev->magic = FX1_CSI2_MAGIC;
    return FX1_CSI2_OK;
}

int fx1_csi2_soft_reset(fx1_csi2_dev *dev, uint32_t timeout_us) {
    uint32_t cfg, budget = timeout_us;
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    cfg = rd(dev, FX1_CSI2_CORE_CONFIG);
    /* soft_reset is a level: assert then release; the drain may outlast the
     * pulse, reset_in_progress covers it (M13, M17). */
    int rc;
    wr(dev, FX1_CSI2_CORE_CONFIG, cfg | FX1_CSI2_CORE_CONFIG_SOFT_RESET_MASK);
    wr(dev, FX1_CSI2_CORE_CONFIG, cfg & ~FX1_CSI2_CORE_CONFIG_SOFT_RESET_MASK);
    rc = wait_reset_done(dev, &budget);
    if (rc) return rc;
    /* Without a byte clock Lane Management cannot have applied it (C20). */
    if (!(rd(dev, FX1_CSI2_PHY_STATUS) & FX1_CSI2_PHY_STATUS_CL_RXCLKACTIVEHS_MASK)) return FX1_CSI2_ENOCLK;
    return FX1_CSI2_OK;
}

int fx1_csi2_full_reset(fx1_csi2_dev *dev, uint32_t timeout_us) {
    uint32_t budget = timeout_us;
    int rc;
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    wr(dev, FX1_CSI2_CORE_CONFIG, rd(dev, FX1_CSI2_CORE_CONFIG) | FX1_CSI2_CORE_CONFIG_FULL_RESET_MASK);
    rc = wait_reset_done(dev, &budget);
    dev->lanes = 0; /* registers are back at reset: PHY disabled */
    return rc;
}

static int phy_reset(fx1_csi2_dev *dev, uint32_t *budget_us) {
    wr(dev, FX1_CSI2_PHY_CONFIG, rd(dev, FX1_CSI2_PHY_CONFIG) | FX1_CSI2_PHY_CONFIG_DPHY_RST_REQ_MASK);
    return wait_reset_done(dev, budget_us);
}

int fx1_csi2_phy_reset(fx1_csi2_dev *dev, uint32_t timeout_us) {
    uint32_t budget = timeout_us;
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    return phy_reset(dev, &budget);
}

int fx1_csi2_phy_start(fx1_csi2_dev *dev, unsigned lanes, uint32_t timeout_us) {
    uint32_t enc, mask, stop, v, i, budget = timeout_us; /* one budget for the reset and both waits */
    int rc;
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    if ((lanes != 1 && lanes != 2 && lanes != 4) || lanes > dev->max_lanes) return FX1_CSI2_EINVAL;
    enc = lanes == 1 ? 0u : lanes == 2 ? 1u : 3u;
    mask = (1u << lanes) - 1u;
    /* Disabled while configuring, so the PHY calibrates exactly the active
     * lanes when phy_enable rises (P09, P10). */
    wr(dev, FX1_CSI2_PHY_CONFIG, 0);
    rc = phy_reset(dev, &budget);
    if (rc) return rc;
    v = rd(dev, FX1_CSI2_PROTOCOL_CONFIG) & ~FX1_CSI2_PROTOCOL_CONFIG_ACTIVE_LANES_MASK;
    wr(dev, FX1_CSI2_PROTOCOL_CONFIG, v | (enc << FX1_CSI2_PROTOCOL_CONFIG_ACTIVE_LANES_SHIFT));
    wr(dev, FX1_CSI2_PHY_CONFIG, FX1_CSI2_PHY_CONFIG_PHY_ENABLE_MASK | FX1_CSI2_PHY_CONFIG_CL_ENABLE_MASK |
                                     (mask << FX1_CSI2_PHY_CONFIG_DL_ENABLE_SHIFT));
    stop = 0;
    for (i = 0; i < lanes; ++i) stop |= FX1_CSI2_LANE_STATUS_LANE0_STOPSTATE_MASK << (8u * i);
    rc = wait_for(dev, FX1_CSI2_PHY_STATUS, FX1_CSI2_PHY_STATUS_CL_STOPSTATE_MASK, FX1_CSI2_PHY_STATUS_CL_STOPSTATE_MASK,
                  &budget);
    if (!rc) rc = wait_for(dev, FX1_CSI2_LANE_STATUS, stop, stop, &budget);
    if (!rc) dev->lanes = lanes;
    return rc;
}

int fx1_csi2_phy_stop(fx1_csi2_dev *dev) {
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    wr(dev, FX1_CSI2_PHY_CONFIG, rd(dev, FX1_CSI2_PHY_CONFIG) & ~FX1_CSI2_PHY_CONFIG_PHY_ENABLE_MASK);
    dev->lanes = 0;
    return FX1_CSI2_OK;
}

int fx1_csi2_core_enable(fx1_csi2_dev *dev, int enable) {
    uint32_t v;
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    v = rd(dev, FX1_CSI2_CORE_CONFIG) & ~FX1_CSI2_CORE_CONFIG_CORE_ENABLE_MASK;
    wr(dev, FX1_CSI2_CORE_CONFIG, v | (enable ? FX1_CSI2_CORE_CONFIG_CORE_ENABLE_MASK : 0u));
    return FX1_CSI2_OK;
}

int fx1_csi2_irq_enable(fx1_csi2_dev *dev, uint32_t mask) {
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    if (mask & ~FX1_CSI2_IRQ_ENABLE_IRQ_ENABLE_MASK_MASK) return FX1_CSI2_EINVAL;
    wr(dev, FX1_CSI2_IRQ_STATUS, FX1_CSI2_IRQ_ENABLE_IRQ_ENABLE_MASK_MASK); /* MAS step 11 */
    wr(dev, FX1_CSI2_IRQ_ENABLE, mask);
    wr(dev, FX1_CSI2_GLOBAL_IRQ_ENABLE, FX1_CSI2_GLOBAL_IRQ_ENABLE_GLOBAL_ENABLE_MASK);
    return FX1_CSI2_OK;
}

int fx1_csi2_irq_disable(fx1_csi2_dev *dev) {
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    wr(dev, FX1_CSI2_GLOBAL_IRQ_ENABLE, 0);
    return FX1_CSI2_OK;
}

uint32_t fx1_csi2_irq_handle(fx1_csi2_dev *dev) {
    uint32_t pending;
    if (!valid(dev)) return 0;
    pending = rd(dev, FX1_CSI2_IRQ_STATUS) & rd(dev, FX1_CSI2_IRQ_ENABLE);
    if (pending) wr(dev, FX1_CSI2_IRQ_STATUS, pending);
    return pending;
}

int fx1_csi2_get_status(fx1_csi2_dev *dev, fx1_csi2_status *st) {
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    if (!st) return FX1_CSI2_EINVAL;
    st->core_status = rd(dev, FX1_CSI2_CORE_STATUS);
    st->phy_status = rd(dev, FX1_CSI2_PHY_STATUS);
    st->lane_status = rd(dev, FX1_CSI2_LANE_STATUS);
    st->buffer_status = rd(dev, FX1_CSI2_BUFFER_STATUS);
    st->lane_align_status = rd(dev, FX1_CSI2_LANE_ALIGN_STATUS);
    st->packet_count = rd(dev, FX1_CSI2_PACKET_COUNT);
    st->frame_count = rd(dev, FX1_CSI2_FRAME_COUNT);
    st->frame_drop_count = rd(dev, FX1_CSI2_FRAME_DROP_COUNT);
    st->crc_error_count = rd(dev, FX1_CSI2_CRC_ERROR_COUNT);
    st->ecc_error_count = rd(dev, FX1_CSI2_ECC_ERROR_COUNT);
    st->lane_abort_count = rd(dev, FX1_CSI2_LANE_ABORT_COUNT);
    st->irq_status = rd(dev, FX1_CSI2_IRQ_STATUS);
    return FX1_CSI2_OK;
}

int fx1_csi2_read_timestamp(fx1_csi2_dev *dev, uint64_t *ns) {
    uint32_t lo;
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    if (!ns) return FX1_CSI2_EINVAL;
    lo = rd(dev, FX1_CSI2_TIMESTAMP_LO); /* snapshots the upper half */
    *ns = ((uint64_t)rd(dev, FX1_CSI2_TIMESTAMP_HI) << 32) | lo;
    return FX1_CSI2_OK;
}

int fx1_csi2_gsp_pop(fx1_csi2_dev *dev, uint8_t *vc, uint8_t *dt, uint16_t *data) {
    uint32_t v;
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    if (!(rd(dev, FX1_CSI2_CORE_STATUS) & FX1_CSI2_CORE_STATUS_SP_FIFO_NOT_EMPTY_MASK)) return FX1_CSI2_ENODATA;
    v = rd(dev, FX1_CSI2_GENERIC_SHORT_PACKET); /* the read pops (D12) */
    if (vc) *vc = (uint8_t)FIELD(v, GENERIC_SHORT_PACKET, VIRTUAL_CHANNEL);
    if (dt) *dt = (uint8_t)FIELD(v, GENERIC_SHORT_PACKET, DATA_TYPE);
    if (data) *data = (uint16_t)FIELD(v, GENERIC_SHORT_PACKET, DATA);
    return FX1_CSI2_OK;
}

/* ---- RAW DMA (M4b) ----------------------------------------------------------- */

static int dma_idle(fx1_csi2_dev *d) {
    return !(rd(d, FX1_CSI2_DMA_CONTROL) & FX1_CSI2_DMA_CONTROL_DMA_ENABLE_MASK) &&
           !(rd(d, FX1_CSI2_DMA_STATUS) & FX1_CSI2_DMA_STATUS_DMA_BUSY_MASK);
}

int fx1_csi2_dma_config(fx1_csi2_dev *dev, const fx1_csi2_dma_cfg *c) {
    uint32_t stride, i, ctl;
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    if (!c) return FX1_CSI2_EINVAL;
    stride = (uint32_t)c->bytes_per_line + c->line_padding;
    if (c->bytes_per_line == 0 || (stride & 7u) || c->size < c->bytes_per_line) return FX1_CSI2_EINVAL;
    for (i = 0; i < 2; ++i)
        if ((c->base[i] & 7u) || (uint64_t)c->base[i] + c->size > 0x100000000ull) return FX1_CSI2_EINVAL;
    if (!dma_idle(dev)) return FX1_CSI2_ESTATE;
    wr(dev, FX1_CSI2_DMA_BUFFER0_ADDR, c->base[0]);
    wr(dev, FX1_CSI2_DMA_BUFFER1_ADDR, c->base[1]);
    wr(dev, FX1_CSI2_DMA_BUFFER_SIZE, c->size);
    wr(dev, FX1_CSI2_LINE_PITCH, ((uint32_t)c->bytes_per_line << FX1_CSI2_LINE_PITCH_BYTES_PER_LINE_SHIFT) |
                                     ((uint32_t)c->line_padding << FX1_CSI2_LINE_PITCH_LINE_PADDING_SHIFT));
    wr(dev, FX1_CSI2_DMA_ATTR,
       (((uint32_t)c->qos << FX1_CSI2_DMA_ATTR_AWQOS_SHIFT) & FX1_CSI2_DMA_ATTR_AWQOS_MASK) |
           (((uint32_t)c->max_burst_len << FX1_CSI2_DMA_ATTR_MAX_BURST_LEN_SHIFT) & FX1_CSI2_DMA_ATTR_MAX_BURST_LEN_MASK) |
           (((uint32_t)c->prot << FX1_CSI2_DMA_ATTR_AWPROT_SHIFT) & FX1_CSI2_DMA_ATTR_AWPROT_MASK) |
           (((uint32_t)c->max_outstanding << FX1_CSI2_DMA_ATTR_MAX_OUTSTANDING_SHIFT) & FX1_CSI2_DMA_ATTR_MAX_OUTSTANDING_MASK));
    ctl = (c->raw16 ? FX1_CSI2_DMA_CONTROL_DMA_FORMAT_MASK : 0u) |
          (c->release_on_error ? FX1_CSI2_DMA_CONTROL_ERR_FRAME_POLICY_MASK : 0u);
    wr(dev, FX1_CSI2_DMA_CONTROL, ctl); /* enable = start = abort = 0 */
    return FX1_CSI2_OK;
}

int fx1_csi2_dma_start(fx1_csi2_dev *dev) {
    uint32_t v;
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    v = rd(dev, FX1_CSI2_DMA_CONTROL) & ~FX1_CSI2_DMA_CONTROL_DMA_ABORT_MASK;
    wr(dev, FX1_CSI2_DMA_CONTROL, v | FX1_CSI2_DMA_CONTROL_DMA_ENABLE_MASK | FX1_CSI2_DMA_CONTROL_DMA_START_MASK);
    return FX1_CSI2_OK;
}

int fx1_csi2_dma_stop(fx1_csi2_dev *dev, uint32_t timeout_us) {
    uint32_t v, budget = timeout_us;
    int rc;
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    v = rd(dev, FX1_CSI2_DMA_CONTROL) & ~FX1_CSI2_DMA_CONTROL_DMA_START_MASK;
    wr(dev, FX1_CSI2_DMA_CONTROL, v);
    rc = wait_for(dev, FX1_CSI2_DMA_STATUS, FX1_CSI2_DMA_STATUS_DMA_BUSY_MASK, 0, &budget);
    if (rc) return rc;
    wr(dev, FX1_CSI2_DMA_CONTROL, v & ~FX1_CSI2_DMA_CONTROL_DMA_ENABLE_MASK);
    return FX1_CSI2_OK;
}

int fx1_csi2_dma_abort(fx1_csi2_dev *dev, uint32_t timeout_us) {
    uint32_t v, budget = timeout_us; /* one budget for both conditions (review M4b F1) */
    int rc;
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    v = rd(dev, FX1_CSI2_DMA_CONTROL);
    wr(dev, FX1_CSI2_DMA_CONTROL, v | FX1_CSI2_DMA_CONTROL_DMA_ABORT_MASK);
    rc = wait_for(dev, FX1_CSI2_DMA_STATUS, FX1_CSI2_DMA_STATUS_DMA_BUSY_MASK, 0, &budget);
    if (!rc) rc = wait_reset_done(dev, &budget);
    if (rc) return rc; /* abort left pending: the drain is not over */
    wr(dev, FX1_CSI2_DMA_CONTROL, v & ~(FX1_CSI2_DMA_CONTROL_DMA_ABORT_MASK | FX1_CSI2_DMA_CONTROL_DMA_ENABLE_MASK |
                                        FX1_CSI2_DMA_CONTROL_DMA_START_MASK));
    return FX1_CSI2_OK;
}

int fx1_csi2_buffer_release(fx1_csi2_dev *dev, unsigned side) {
    fx1_csi2_buf_info info;
    int rc;
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    if (side > 1) return FX1_CSI2_EINVAL;
    rc = fx1_csi2_get_buffer(dev, side, &info);
    if (rc) return rc;
    if (info.state == FX1_CSI2_BUF_HW_OWNED) return FX1_CSI2_ESTATE;
    wr(dev, FX1_CSI2_BUFFER_RELEASE, side ? FX1_CSI2_BUFFER_RELEASE_RELEASE_BUF1_MASK : FX1_CSI2_BUFFER_RELEASE_RELEASE_BUF0_MASK);
    return FX1_CSI2_OK;
}

int fx1_csi2_get_buffer(fx1_csi2_dev *dev, unsigned side, fx1_csi2_buf_info *info) {
    uint32_t own, fid, err;
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    if (side > 1 || !info) return FX1_CSI2_EINVAL;
    own = rd(dev, FX1_CSI2_BUFFER_OWNERSHIP);
    fid = rd(dev, FX1_CSI2_FRAME_ID);
    err = rd(dev, FX1_CSI2_BUFFER_ERROR_INFO);
    info->state = (fx1_csi2_buf_state)(side ? FIELD(own, BUFFER_OWNERSHIP, BUF1_STATE) : FIELD(own, BUFFER_OWNERSHIP, BUF0_STATE));
    info->frame_id = (uint16_t)(side ? FIELD(fid, FRAME_ID, FRAME_ID_BUF1) : FIELD(fid, FRAME_ID, FRAME_ID_BUF0));
    info->last_valid_line =
        (uint16_t)(side ? FIELD(err, BUFFER_ERROR_INFO, BUF1_LAST_VALID_LINE) : FIELD(err, BUFFER_ERROR_INFO, BUF0_LAST_VALID_LINE));
    return FX1_CSI2_OK;
}

int fx1_csi2_get_dma_fault(fx1_csi2_dev *dev, fx1_csi2_dma_fault *f) {
    uint32_t t, n;
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    if (!f) return FX1_CSI2_EINVAL;
    t = rd(dev, FX1_CSI2_AXI_FAULT_TRACE);
    n = rd(dev, FX1_CSI2_AXI_ERROR_COUNT);
    f->awid = (uint8_t)FIELD(t, AXI_FAULT_TRACE, AWID);
    f->resp = (uint8_t)FIELD(t, AXI_FAULT_TRACE, RESP);
    f->addr = rd(dev, FX1_CSI2_AXI_FAULT_ADDR);
    f->slverr_count = (uint16_t)(n & 0xFFFFu);
    f->decerr_count = (uint16_t)(n >> 16);
    return FX1_CSI2_OK;
}

int fx1_csi2_stream_config(fx1_csi2_dev *dev, int enable, int raw16) {
    uint32_t cur, want;
    if (!valid(dev)) return FX1_CSI2_ESTATE;
    cur = rd(dev, FX1_CSI2_STREAM_CONFIG);
    /* workbook: stream_format 0 = RAW16, 1 = packed (C27) */
    want = (enable ? FX1_CSI2_STREAM_CONFIG_AXIS_ENABLE_MASK : 0u) | (raw16 ? 0u : FX1_CSI2_STREAM_CONFIG_STREAM_FORMAT_MASK);
    if ((cur ^ want) & FX1_CSI2_STREAM_CONFIG_STREAM_FORMAT_MASK) {
        if (cur & FX1_CSI2_STREAM_CONFIG_AXIS_ENABLE_MASK) return FX1_CSI2_ESTATE; /* nothing written */
        /* Disabled: the format is written first with the path still disabled,
         * then the enable with the format unchanged (review M4b F2). */
        wr(dev, FX1_CSI2_STREAM_CONFIG, want & ~FX1_CSI2_STREAM_CONFIG_AXIS_ENABLE_MASK);
    }
    wr(dev, FX1_CSI2_STREAM_CONFIG, want);
    return FX1_CSI2_OK;
}
