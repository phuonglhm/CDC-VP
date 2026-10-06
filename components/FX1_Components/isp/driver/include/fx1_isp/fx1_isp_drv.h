/* SPDX-License-Identifier: Apache-2.0 */
/*
 * FX1 ISP reference driver (bare-metal / RTOS style, DEC-02).
 *
 * A small, hardware-independent C99 driver that implements the programming
 * sequences of docs/ISP_PROGRAMMING_GUIDE.md on top of the generated register
 * header fx1_isp_csr.h. Register access, delays and memory are supplied by
 * the integrator through fx1_isp_hal, so the same code runs on a CPU, an RTOS
 * or the SystemC model (tests/integration/test_driver_tlm.cpp runs it there).
 *
 * The driver owns no memory and does no allocation. It is not thread-safe:
 * the integrator serialises calls, and calls fx1_isp_irq() from the
 * interrupt handler (or a deferred handler) of the ISP's `o_irq` line.
 *
 * This is a reference for driver writers. It is NOT a product driver: it has
 * no Linux/V4L2 binding and no power management. Error handling follows the
 * VP's documented decisions (DEC-14, DEC-18, DEC-19), which are not
 * confirmed by the IP owner (DEC-05).
 */
#ifndef FX1_ISP_DRV_H
#define FX1_ISP_DRV_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Return codes ---------------------------------------------------------- */
#define FX1_ISP_OK 0
#define FX1_ISP_EINVAL (-1)  /* argument outside the register contract */
#define FX1_ISP_EBUSY (-2)   /* the operation needs an idle pipeline / free slot */
#define FX1_ISP_EAGAIN (-3)  /* results changed during the read: retry */
#define FX1_ISP_EIO (-4)     /* the hardware reported an error (see LSC_ERROR / DMA_ERR) */
#define FX1_ISP_ENODATA (-5) /* nothing to report yet */
#define FX1_ISP_ESTATE (-6)  /* call not allowed in the driver's current state */

#define FX1_ISP_NUM_BUFFERS 4u /* PARA_DMA_NUM_BUFFERS of the CSR map */

/* ---- Integration layer ------------------------------------------------------- */
typedef struct fx1_isp_hal {
    /* 32-bit register access at `offset` from the ISP base (FX1_ISP_*_OFFSET). */
    uint32_t (*read32)(void *ctx, uint32_t offset);
    void (*write32)(void *ctx, uint32_t offset, uint32_t value);
    /* Required: busy-wait or sleep for at least `cycles` ISP core-clock cycles. */
    void (*delay_cycles)(void *ctx, uint32_t cycles);
    void *ctx;
} fx1_isp_hal;

/* One CSR write of a configuration profile. */
typedef struct fx1_isp_reg_write {
    uint32_t offset;
    uint32_t value;
} fx1_isp_reg_write;

/* Bus addresses of the four input and four output buffers (64-bit registers;
 * the IP drives PARA_AXI_ADDR_WIDTH = 40 address bits). */
typedef struct fx1_isp_buffers {
    uint64_t in[FX1_ISP_NUM_BUFFERS];
    uint32_t in_stride;          /* bytes per input line, >= 2 * width, multiple of the AXI beat */
    uint64_t y[FX1_ISP_NUM_BUFFERS];
    uint64_t uv[FX1_ISP_NUM_BUFFERS];
    uint32_t y_stride, uv_stride; /* >= output width, multiple of the AXI beat */
    uint32_t max_burst_beats;     /* 1..256 (DMA_CTRL.max_burst_m1 + 1) */
} fx1_isp_buffers;

/* Driver states. Calls outside their state return FX1_ISP_ESTATE.
 *   UNINIT    -> fx1_isp_init                  -> CONFIG
 *   CONFIG    -> fx1_isp_start                 -> STREAMING
 *   STREAMING -> fx1_isp_stop                  -> CONFIG (frames in flight discarded)
 *   any initialised state: fx1_isp_soft_reset keeps the state, drops the frames. */
#define FX1_ISP_STATE_UNINIT 0
#define FX1_ISP_STATE_CONFIG 1
#define FX1_ISP_STATE_STREAMING 2

/* One frame handed to the hardware, in queue order. */
typedef struct fx1_isp_frame {
    uint32_t seq;      /* sequence number assigned by fx1_isp_queue_frame_ex */
    unsigned input;    /* input buffer index it was queued from */
    int odma_failed;   /* its output failed; lost unless the IDMA retries it (see recover) */
} fx1_isp_frame;

/* Driver state: the software view of the buffer rotation (HAS §6.25.10).
 * Zero-initialise before fx1_isp_init; the fields are private to the driver. */
typedef struct fx1_isp_dev {
    fx1_isp_hal hal;
    uint32_t magic;       /* set by fx1_isp_init */
    int state;            /* FX1_ISP_STATE_* */
    unsigned in_next;     /* next input buffer to hand over (IDMA_BUF_VALID) */
    unsigned out_next;    /* next output buffer to hand over (ODMA_BUF_FREE) */
    unsigned out_done;    /* next output buffer expected to complete (ODMA_BUF_DONE) */
    unsigned in_flight;   /* frames handed over and not yet completed */
    unsigned in_failed;   /* input index of the frame an IDMA AXI error released */
    fx1_isp_frame queue[4]; /* in-flight frames, oldest first (queue[0..in_flight-1]) */
    fx1_isp_frame lost[4];  /* frames known lost, not yet fetched with fx1_isp_next_lost */
    unsigned lost_count;
    uint32_t next_seq;
    uint32_t out_width, out_height;
    uint32_t dma_ctrl;    /* DMA_CTRL value written at start */
    int started;
    /* Zone/histogram data may be partial after an aborted frame or a reset
     * (DEC-30, M3-A7): untrusted until AEC_FRAME_ID moves past stats_mark. */
    int stats_untrusted;
    uint32_t stats_mark;
} fx1_isp_dev;

/* Events reported by fx1_isp_irq(). */
typedef struct fx1_isp_events {
    uint32_t common;   /* COMMON_IRQ_STATUS bits that were set (now acknowledged) */
    uint32_t dma;      /* DMA_IRQ_STAT bits that were set (now acknowledged) */
    uint32_t dma_err;  /* DMA_ERR at the time of the interrupt */
    unsigned frames_done; /* output buffers that completed (fetch with fx1_isp_next_done) */
    unsigned frames_lost; /* frames known lost (fetch with fx1_isp_next_lost) */
} fx1_isp_events;

/* ---- Bring-up (HAS §9.1 order: reset, geometry, profile/LUTs, ISP_EN, DMA) --- */
int fx1_isp_init(fx1_isp_dev *dev, const fx1_isp_hal *hal);
/* Cancels the stream: soft reset, DMA engines disabled, every buffer back to
 * the caller. Returns the number of frames that were in flight (discarded), or
 * FX1_ISP_ESTATE if not streaming. Use it when a watchdog sees no progress. */
int fx1_isp_stop(fx1_isp_dev *dev);
/* Soft reset (DEC-14): configuration, LUTs and LSC profiles are kept; buffer
 * ownership, results and interrupt state are cleared; rotation returns to 0. */
void fx1_isp_soft_reset(fx1_isp_dev *dev);
/* Input geometry and Bayer order (0 RGGB, 1 GRBG, 2 GBRG, 3 BGGR, DEC-11).
 * Pipeline idle only (HAS §9.4.4). */
int fx1_isp_set_geometry(fx1_isp_dev *dev, uint32_t width, uint32_t height, uint32_t bayer);
/* Raw register writes of a profile, in order. */
void fx1_isp_apply(fx1_isp_dev *dev, const fx1_isp_reg_write *writes, size_t n);

/* ---- Tables and gated sets -------------------------------------------------- */
void fx1_isp_load_gamma(fx1_isp_dev *dev, const uint16_t lut[256]);
/* bank 0 luma / 1 activity (64 entries), 2 / 3 contrast (32 entries). */
int fx1_isp_load_ee_table(fx1_isp_dev *dev, unsigned bank, const uint16_t *table, unsigned n);
/* GTM software table (65 entries); writes are taken only while GTM is
 * disabled or manual (HAS Table 6-49), so this selects manual mode. */
void fx1_isp_load_gtm_lut(fx1_isp_dev *dev, const uint16_t lut[65]);
/* Loads 4 * nx * ny UQ3.18 coefficients (R, Gr, Gb, B per node, x then y)
 * into a non-active LSC profile and validates it; returns FX1_ISP_EIO with
 * *lsc_error set if the loader rejected it. */
int fx1_isp_lsc_load(fx1_isp_dev *dev, unsigned profile, unsigned nx, unsigned ny, const uint32_t *coef,
                     uint32_t *lsc_error);
/* Selects a valid profile; it becomes active at the next frame start. */
int fx1_isp_lsc_select(fx1_isp_dev *dev, unsigned profile);
/* CCM coefficients (Q3.9, row major) and offsets, committed through
 * CCM_CTRL.updated (DEC-24/26: immediately if idle, else at the next SOF). */
void fx1_isp_ccm_set(fx1_isp_dev *dev, const int16_t m[9], const int16_t ofs[3], int enable);

/* AEC: shadow configuration plus commit (CSR #296); takes effect at the next SOF. */
typedef struct fx1_isp_aec_cfg {
    unsigned nx, ny;            /* 1..32, 1..24 */
    unsigned zone_w, zone_h;    /* pixels */
    unsigned clip_min, clip_max;
    unsigned th_ue, th_oe;
    uint32_t context_id;
} fx1_isp_aec_cfg;
void fx1_isp_aec_configure(fx1_isp_dev *dev, const fx1_isp_aec_cfg *cfg, int enable);

/* ---- Streaming -------------------------------------------------------------- */
/* Enables the pipeline, the interrupt sources and the DMA engines. Call after
 * the profile and only in CONFIG state; use fx1_isp_stop before restarting a
 * drained stream so the hardware and driver buffer rotation both reset.
 * The output geometry (RESIZER_OUT_W/H) is valid from here. */
int fx1_isp_start(fx1_isp_dev *dev, const fx1_isp_buffers *bufs);
/* Hands the next input and output buffers to the hardware (one frame).
 * First check fx1_isp_input_ready(), then fill the input at fx1_isp_next_input().
 * Returns FX1_ISP_EBUSY if all four frames are in flight or IDMA still owns
 * the next input (possible while draining a frame after an ODMA error). */
int fx1_isp_queue_frame(fx1_isp_dev *dev);
/* As fx1_isp_queue_frame, and returns the frame's sequence number in *seq. */
int fx1_isp_queue_frame_ex(fx1_isp_dev *dev, uint32_t *seq);
unsigned fx1_isp_next_input(const fx1_isp_dev *dev);
/* Non-zero when the next input may be filled and queued. Serialise this
 * check, filling and queue_frame against other driver calls. */
int fx1_isp_input_ready(fx1_isp_dev *dev);
/* Interrupt handler: reads and acknowledges both status groups. */
void fx1_isp_irq(fx1_isp_dev *dev, fx1_isp_events *ev);
/* Pops the next completed output buffer (index into fx1_isp_buffers.y/uv):
 * FX1_ISP_OK and *index, or FX1_ISP_ENODATA. The buffer may be reused or
 * queued again after the caller has consumed it. */
int fx1_isp_next_done(fx1_isp_dev *dev, unsigned *index);
/* As fx1_isp_next_done, and returns the completed frame's sequence number. */
int fx1_isp_next_done_ex(fx1_isp_dev *dev, unsigned *index, uint32_t *seq);
/* Error recovery (DEC-19). After an IDMA AXI error the input buffer was
 * released and the rotation kept: re-arm the same index. After an ODMA AXI
 * error the output buffer was not completed and is no longer free: free the
 * same index again (the failed frame is lost; the caller resubmits it). */
int fx1_isp_recover(fx1_isp_dev *dev, uint32_t dma_err);
/* What fx1_isp_recover_ex did. Only error bits that are still set in DMA_ERR
 * are handled, so a second call with the same (stale) snapshot does nothing
 * and returns FX1_ISP_ENODATA (unless the hardware flagged the error again).
 *
 * A frame whose output failed is lost only once the IDMA has finished
 * reading its input without error: if the IDMA fails on the same frame, the
 * frame is retried from its input and still completes. The driver therefore
 * reports a loss when it is certain, through fx1_isp_next_lost(), possibly at
 * a later fx1_isp_irq() (the IDMA_DONE interrupt is enabled for this). */
typedef struct fx1_isp_recovery {
    uint32_t handled;          /* DMA_ERR bits acted on */
    int idma_rearmed;          /* IDMA_AXI: the frame is retried from the same input */
    unsigned idma_input;
    uint32_t idma_seq;
    int odma_failed;           /* ODMA_AXI: this frame's output failed */
    unsigned odma_output;      /* output buffer freed again */
    uint32_t odma_seq;
} fx1_isp_recovery;
int fx1_isp_recover_ex(fx1_isp_dev *dev, uint32_t dma_err, fx1_isp_recovery *r);
/* Pops the next frame known lost (its input index and sequence number), to be
 * resubmitted by the caller: FX1_ISP_OK, or FX1_ISP_ENODATA. */
int fx1_isp_next_lost(fx1_isp_dev *dev, uint32_t *seq, unsigned *input);

/* Progress snapshot for a caller's watchdog (programming guide §8.1). */
typedef struct fx1_isp_progress {
    uint32_t idma_frames, odma_frames;  /* IDMA/ODMA_FRAME_COUNT */
    unsigned in_flight;
    uint32_t dma_err;                   /* DMA_ERR */
    int pipeline_busy;                  /* COMMON_STATUS.busy */
    int isp_enabled;                    /* COMMON_CTRL.isp_en */
} fx1_isp_progress;
int fx1_isp_get_progress(fx1_isp_dev *dev, fx1_isp_progress *p);

/* ---- Statistics (HAS §6.21-6.23, §9.5; protocol of HAS §6.21.9) --------------
 * Globals and AF scores are double-buffered: read-tag-read on FRAME_ID.
 * Zones and histograms are a single in-place memory (DEC-30): they are
 * coherent only while the block is idle, so the zone readers also check
 * busy and return FX1_ISP_EAGAIN when a frame is in progress. */
typedef struct fx1_isp_aec_global {
    uint64_t sum[4];   /* R, Gr, Gb, B (33-bit) */
    uint32_t count[4]; /* 21-bit */
    uint32_t frame_id, context_id;
} fx1_isp_aec_global;
int fx1_isp_read_aec_global(fx1_isp_dev *dev, fx1_isp_aec_global *out);

typedef struct fx1_isp_aec_zone {
    uint32_t sum[4], count[4];
    uint32_t green_oe, green_ue, green_min, green_max;
} fx1_isp_aec_zone;
/* FX1_ISP_EAGAIN while a frame is being measured; FX1_ISP_ENODATA after an
 * aborted frame, or a reset with a previous publication, until the next AEC
 * publication (DEC-30, M3-A7). This includes the first frame aborting while
 * AEC_FRAME_ID is still 0. A reset before any publication reads empty defaults. */
int fx1_isp_read_aec_zones(fx1_isp_dev *dev, unsigned first, unsigned n, fx1_isp_aec_zone *zones,
                           uint32_t hist[64], uint32_t *frame_id);

typedef struct fx1_isp_awb_global {
    uint64_t sum[3];   /* R, G, B (35-bit) */
    uint32_t count;
    uint32_t frame_id, context_id;
} fx1_isp_awb_global;
int fx1_isp_read_awb_global(fx1_isp_dev *dev, fx1_isp_awb_global *out);

typedef struct fx1_isp_af_scores {
    uint32_t fv[16];   /* 4x4 zones, raster order */
    int score_valid;
    uint32_t frame_id, context_id;
} fx1_isp_af_scores;
int fx1_isp_read_af(fx1_isp_dev *dev, fx1_isp_af_scores *out);

#ifdef __cplusplus
}
#endif

#endif /* FX1_ISP_DRV_H */
