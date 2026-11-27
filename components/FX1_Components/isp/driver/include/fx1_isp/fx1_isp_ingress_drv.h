/* SPDX-License-Identifier: Apache-2.0 */
/*
 * FX1 ISP_INGRESS reference driver (C99): the VP-only pixel-source select and
 * per-capture context of the direct CSI-2 -> ISP path (registers
 * fx1_isp/fx1_isp_ingress_regs.h; not part of the FTEL ISP v1.07).
 *
 * Same integration layer as the ISP driver (fx1_isp_hal), with offsets
 * relative to the ISP_INGRESS base. The driver owns no memory, does not
 * allocate and is not thread-safe.
 *
 * Programming order for direct (contract DIRECT_CSI_ISP_CONTRACT.md §6):
 * ISP idle and drained, DMA_CTRL.IDMA_EN = 0, then
 * fx1_isp_ingress_set_source(dev, FX1_ISP_INGRESS_SOURCE_DIRECT); back to
 * memory only when the ingress is idle again. A direct NV12 output is
 * published only when its ODMA completion (BUF_TAG T) and the context entry
 * with TAG T and outcome OK are both present (P26).
 */
#ifndef FX1_ISP_INGRESS_DRV_H
#define FX1_ISP_INGRESS_DRV_H

#include <stdint.h>

#include "fx1_isp/fx1_isp_drv.h"
#include "fx1_isp/fx1_isp_ingress_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FX1_ISP_INGRESS_SOURCE_MEMORY 0
#define FX1_ISP_INGRESS_SOURCE_DIRECT 1

typedef struct fx1_isp_ingress {
    fx1_isp_hal hal; /* offsets relative to the ISP_INGRESS base */
} fx1_isp_ingress;

/* One READY context entry (CTX_* registers). */
typedef struct fx1_isp_ingress_ctx {
    uint32_t tag;      /* = ODMA BUF_TAG of the frame */
    uint64_t seq;      /* 64-bit CSI capture sequence */
    uint16_t frame_id; /* CSI FRAME_ID (= ODMA BUF_TS) */
    uint16_t epoch;    /* CSI epoch at the capture start */
    uint64_t ts_ns;    /* FS timestamp */
    uint32_t outcome;  /* FX1_ISP_INGRESS_OUTCOME_* */
    uint32_t cause;    /* FX1_ISP_INGRESS_CAUSE_* when outcome is INVALID */
    uint32_t lines;
    uint32_t samples;
    uint32_t isp_frame; /* ISP frame ID (statistics, BUFn_FRAME); 0: never reached the ISP */
} fx1_isp_ingress_ctx;

typedef struct fx1_isp_ingress_counters {
    uint32_t beats_accepted, frames_admitted, drop_no_credit, drop_no_context, drop_no_stream, drop_config,
        frames_invalid, captures_untracked, beats_discarded, stall_cycles;
    uint32_t cause[FX1_ISP_INGRESS_NUM_CAUSES]; /* index cause - 1 */
} fx1_isp_ingress_counters;

/* FX1_ISP_ENOTSUP when the block's ID is not 'ISPI'. */
int fx1_isp_ingress_init(fx1_isp_ingress *dev, const fx1_isp_hal *hal);
/* 1 when a CSI stream is bound (CAPABILITY.BOUND). */
int fx1_isp_ingress_bound(const fx1_isp_ingress *dev);
/* FX1_ISP_EBUSY: refused (not idle and drained, or IDMA_EN = 1 for direct;
 * the sticky SWITCH_REFUSED is cleared); FX1_ISP_ENOTSUP: direct selected with
 * no bound stream (NOT_BOUND, cleared; the source is direct and admits nothing). */
int fx1_isp_ingress_set_source(fx1_isp_ingress *dev, int source);
int fx1_isp_ingress_source(const fx1_isp_ingress *dev);
uint32_t fx1_isp_ingress_status(const fx1_isp_ingress *dev);
/* Clears the given sticky STATUS bits (W1C). */
void fx1_isp_ingress_clear_status(fx1_isp_ingress *dev, uint32_t mask);
/* The head READY entry; FX1_ISP_ENODATA when none. pop also frees it. */
int fx1_isp_ingress_ctx_peek(const fx1_isp_ingress *dev, fx1_isp_ingress_ctx *ctx);
int fx1_isp_ingress_ctx_pop(fx1_isp_ingress *dev, fx1_isp_ingress_ctx *ctx);
/* READY and used context entries. */
unsigned fx1_isp_ingress_ctx_ready(const fx1_isp_ingress *dev);
unsigned fx1_isp_ingress_ctx_used(const fx1_isp_ingress *dev);
void fx1_isp_ingress_read_counters(const fx1_isp_ingress *dev, fx1_isp_ingress_counters *c);

#ifdef __cplusplus
}
#endif

#endif /* FX1_ISP_INGRESS_DRV_H */
