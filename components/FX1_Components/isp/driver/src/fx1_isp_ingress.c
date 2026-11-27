/* SPDX-License-Identifier: Apache-2.0 */
#include "fx1_isp/fx1_isp_ingress_drv.h"

#define RD(dev, off) ((dev)->hal.read32((dev)->hal.ctx, (off)))
#define WR(dev, off, v) ((dev)->hal.write32((dev)->hal.ctx, (off), (v)))

int fx1_isp_ingress_init(fx1_isp_ingress *dev, const fx1_isp_hal *hal) {
    if (!dev || !hal || !hal->read32 || !hal->write32) {
        return FX1_ISP_EINVAL;
    }
    dev->hal = *hal;
    return RD(dev, FX1_ISP_INGRESS_ID_OFFSET) == FX1_ISP_INGRESS_ID_VALUE ? FX1_ISP_OK : FX1_ISP_ENOTSUP;
}

int fx1_isp_ingress_bound(const fx1_isp_ingress *dev) {
    return (RD(dev, FX1_ISP_INGRESS_CAPABILITY_OFFSET) & FX1_ISP_INGRESS_CAPABILITY_BOUND_MASK) != 0u;
}

int fx1_isp_ingress_source(const fx1_isp_ingress *dev) {
    return (RD(dev, FX1_ISP_INGRESS_CTRL_OFFSET) & FX1_ISP_INGRESS_CTRL_SOURCE_MASK) ? FX1_ISP_INGRESS_SOURCE_DIRECT
                                                                                     : FX1_ISP_INGRESS_SOURCE_MEMORY;
}

uint32_t fx1_isp_ingress_status(const fx1_isp_ingress *dev) { return RD(dev, FX1_ISP_INGRESS_STATUS_OFFSET); }

void fx1_isp_ingress_clear_status(fx1_isp_ingress *dev, uint32_t mask) {
    WR(dev, FX1_ISP_INGRESS_STATUS_OFFSET, mask & FX1_ISP_INGRESS_STATUS_STICKY_MASK);
}

int fx1_isp_ingress_set_source(fx1_isp_ingress *dev, int source) {
    if (source != FX1_ISP_INGRESS_SOURCE_MEMORY && source != FX1_ISP_INGRESS_SOURCE_DIRECT) {
        return FX1_ISP_EINVAL;
    }
    const uint32_t sticky = FX1_ISP_INGRESS_STATUS_SWITCH_REFUSED_MASK | FX1_ISP_INGRESS_STATUS_NOT_BOUND_MASK;
    WR(dev, FX1_ISP_INGRESS_STATUS_OFFSET, sticky); /* only this call's outcome below */
    const uint32_t ctrl = RD(dev, FX1_ISP_INGRESS_CTRL_OFFSET);
    WR(dev, FX1_ISP_INGRESS_CTRL_OFFSET,
       (ctrl & ~FX1_ISP_INGRESS_CTRL_SOURCE_MASK) | (source ? FX1_ISP_INGRESS_CTRL_SOURCE_MASK : 0u));
    const uint32_t st = RD(dev, FX1_ISP_INGRESS_STATUS_OFFSET);
    WR(dev, FX1_ISP_INGRESS_STATUS_OFFSET, st & sticky);
    if (st & FX1_ISP_INGRESS_STATUS_SWITCH_REFUSED_MASK) {
        return FX1_ISP_EBUSY;
    }
    if (fx1_isp_ingress_source(dev) != source) {
        return FX1_ISP_EBUSY;
    }
    return (st & FX1_ISP_INGRESS_STATUS_NOT_BOUND_MASK) ? FX1_ISP_ENOTSUP : FX1_ISP_OK;
}

unsigned fx1_isp_ingress_ctx_ready(const fx1_isp_ingress *dev) {
    return (unsigned)(RD(dev, FX1_ISP_INGRESS_CTX_COUNT_OFFSET) & FX1_ISP_INGRESS_CTX_COUNT_READY_MASK);
}

unsigned fx1_isp_ingress_ctx_used(const fx1_isp_ingress *dev) {
    return (unsigned)((RD(dev, FX1_ISP_INGRESS_CTX_COUNT_OFFSET) & FX1_ISP_INGRESS_CTX_COUNT_USED_MASK) >>
                      FX1_ISP_INGRESS_CTX_COUNT_USED_SHIFT);
}

int fx1_isp_ingress_ctx_peek(const fx1_isp_ingress *dev, fx1_isp_ingress_ctx *ctx) {
    if (!ctx) {
        return FX1_ISP_EINVAL;
    }
    if (fx1_isp_ingress_ctx_ready(dev) == 0u) {
        return FX1_ISP_ENODATA;
    }
    const uint32_t frame = RD(dev, FX1_ISP_INGRESS_CTX_FRAME_OFFSET);
    const uint32_t outcome = RD(dev, FX1_ISP_INGRESS_CTX_OUTCOME_OFFSET);
    ctx->tag = RD(dev, FX1_ISP_INGRESS_CTX_TAG_OFFSET);
    ctx->seq = RD(dev, FX1_ISP_INGRESS_CTX_SEQ_LO_OFFSET) | ((uint64_t)RD(dev, FX1_ISP_INGRESS_CTX_SEQ_HI_OFFSET) << 32);
    ctx->frame_id = (uint16_t)(frame & FX1_ISP_INGRESS_CTX_FRAME_FRAME_ID_MASK);
    ctx->epoch = (uint16_t)((frame & FX1_ISP_INGRESS_CTX_FRAME_EPOCH_MASK) >> FX1_ISP_INGRESS_CTX_FRAME_EPOCH_SHIFT);
    ctx->ts_ns = RD(dev, FX1_ISP_INGRESS_CTX_TS_LO_OFFSET) | ((uint64_t)RD(dev, FX1_ISP_INGRESS_CTX_TS_HI_OFFSET) << 32);
    ctx->outcome = outcome & FX1_ISP_INGRESS_CTX_OUTCOME_CODE_MASK;
    ctx->cause = (outcome & FX1_ISP_INGRESS_CTX_OUTCOME_CAUSE_MASK) >> FX1_ISP_INGRESS_CTX_OUTCOME_CAUSE_SHIFT;
    ctx->lines = RD(dev, FX1_ISP_INGRESS_CTX_LINES_OFFSET);
    ctx->samples = RD(dev, FX1_ISP_INGRESS_CTX_SAMPLES_OFFSET);
    ctx->isp_frame = RD(dev, FX1_ISP_INGRESS_CTX_ISP_FRAME_OFFSET);
    return FX1_ISP_OK;
}

int fx1_isp_ingress_ctx_pop(fx1_isp_ingress *dev, fx1_isp_ingress_ctx *ctx) {
    const int rc = fx1_isp_ingress_ctx_peek(dev, ctx);
    if (rc == FX1_ISP_OK) {
        WR(dev, FX1_ISP_INGRESS_CTX_POP_OFFSET, 1u);
    }
    return rc;
}

void fx1_isp_ingress_read_counters(const fx1_isp_ingress *dev, fx1_isp_ingress_counters *c) {
    c->beats_accepted = RD(dev, FX1_ISP_INGRESS_BEATS_ACCEPTED_OFFSET);
    c->frames_admitted = RD(dev, FX1_ISP_INGRESS_FRAMES_ADMITTED_OFFSET);
    c->drop_no_credit = RD(dev, FX1_ISP_INGRESS_DROP_NO_CREDIT_OFFSET);
    c->drop_no_context = RD(dev, FX1_ISP_INGRESS_DROP_NO_CONTEXT_OFFSET);
    c->drop_no_stream = RD(dev, FX1_ISP_INGRESS_DROP_NO_STREAM_OFFSET);
    c->drop_config = RD(dev, FX1_ISP_INGRESS_DROP_CONFIG_OFFSET);
    c->frames_invalid = RD(dev, FX1_ISP_INGRESS_FRAMES_INVALID_OFFSET);
    c->captures_untracked = RD(dev, FX1_ISP_INGRESS_CAPTURES_UNTRACKED_OFFSET);
    c->beats_discarded = RD(dev, FX1_ISP_INGRESS_BEATS_DISCARDED_OFFSET);
    c->stall_cycles = RD(dev, FX1_ISP_INGRESS_STALL_CYCLES_OFFSET);
    for (unsigned i = 0; i < FX1_ISP_INGRESS_NUM_CAUSES; ++i) {
        c->cause[i] = RD(dev, FX1_ISP_INGRESS_CAUSE_COUNT_OFFSET(i + 1u));
    }
}
