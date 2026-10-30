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
 * VP's documented decisions (DEC-14, DEC-18, V107-D62..D68), which are not
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
#define FX1_ISP_ENOTSUP (-7) /* the IP lacks a required capability (TUNE_CAPS) */

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
    unsigned output;   /* CH0 output buffer index it was queued to */
} fx1_isp_frame;

/* Driver state: the software view of the buffer rotation (HAS §6.25.10).
 * Zero-initialise before fx1_isp_init; the fields are private to the driver. */
/* AWB result held by software (MAS §6.22.4): identity and captured grid. */
typedef struct fx1_isp_awb_lease {
    uint32_t frame_id, context_id;
    unsigned nx, ny;      /* zone grid captured with the result */
    unsigned bank;        /* AWB_RESULT_STATUS.read_bank */
} fx1_isp_awb_lease;

typedef struct fx1_isp_awb_global {
    uint64_t sum[3];   /* R, G, B (35-bit) */
    uint32_t count;
    uint32_t frame_id, context_id;
} fx1_isp_awb_global;

/* AEC or AF result held by software (MAS v1.07 §9.8, §9.9.9): identity and
 * the geometry captured with the measured frame. */
typedef struct fx1_isp_stat_lease {
    uint32_t frame_id, context_id, profile_id;
    unsigned width, height;          /* active image of the measured frame */
    /* AEC only: */
    unsigned source;                 /* 0 post-BPC, 1 post-DG (TUNE_AEC_SOURCE) */
    unsigned native_depth;           /* 0 RAW12, 1 RAW10 input */
    unsigned nx, ny, zone_w, zone_h;
} fx1_isp_stat_lease;

/* AEC global result (MAS v1.07 Table 9-193): RAW12 Bayer samples. */
typedef struct fx1_isp_aec_global {
    uint64_t sum, count;             /* clip-qualified sum and count */
    uint64_t oe, ue;                 /* samples > TH_OE, < TH_UE */
    uint32_t min, max;               /* extrema of every sample; empty 4095 / 0 */
    uint64_t weighted_total, weighted_sum;
    int overflow;
    uint32_t frame_id, context_id;
} fx1_isp_aec_global;

typedef struct fx1_isp_af_scores {
    uint32_t fv[16];   /* low 32 bits of the focus sums, windows 0..15 */
    int score_valid;
    uint32_t frame_id, context_id;
} fx1_isp_af_scores;

typedef struct fx1_isp_dev {
    fx1_isp_hal hal;
    uint32_t magic;       /* set by fx1_isp_init */
    int state;            /* FX1_ISP_STATE_* */
    unsigned in_next;     /* next input buffer to hand over (IDMA_BUF_VALID) */
    unsigned out_next;    /* next output buffer to hand over (ODMA_BUF_FREE) */
    unsigned rotation_gen; /* incremented whenever the rotation restarts */
    unsigned next_gen;     /* rotation_gen when fx1_isp_next_input() was last read */
    int next_read;         /* fx1_isp_next_input() read since the last queue */
    unsigned in_flight;   /* frames handed over and not yet collected */
    fx1_isp_frame queue[4]; /* in-flight frames, oldest first (queue[0..in_flight-1]) */
    fx1_isp_frame lost[4];  /* frames known lost, not yet fetched with fx1_isp_next_lost */
    unsigned lost_count;
    uint32_t rec_seq;     /* COMMON_RECOVERY_SEQUENCE the queue was last settled against */
    unsigned outputs;     /* OUTPUT_ENABLE channels latched at fx1_isp_start */
    uint32_t reset_pending; /* soft-reset register of an accepted, unfinished reset of ours (0: none) */
    uint32_t reset_completed; /* that reset, completed by a settling call: its retry returns OK */
    uint32_t pending_ack; /* buffers whose DONE could not be acknowledged yet (bit k) */
    uint32_t tune_error_prior; /* TUNE_ERROR bits a profile command found set and acknowledged */
    unsigned ch_ready;    /* channels whose buffers fx1_isp_set_channel_buffers programmed */
    unsigned rec_lost;    /* frames that recovery aborted; rec_first the oldest of them */
    fx1_isp_frame rec_first;
    uint32_t next_seq;
    uint32_t out_width, out_height;
    uint32_t dma_ctrl;    /* DMA_CTRL value written at start */
    int started;
    /* AEC and AF ownership (MAS v1.07 §9.8): the leases taken by
     * fx1_isp_aec_acquire / fx1_isp_af_acquire, and the newest results the
     * compatibility readers returned. A soft reset drops them. */
    int aec_leased, af_leased;
    fx1_isp_stat_lease aec_lease, af_lease;
    int aec_last_valid, af_last_valid;
    fx1_isp_aec_global aec_last;
    fx1_isp_af_scores af_last;
    /* AWB ownership (MAS §6.22.4): the lease taken by fx1_isp_awb_acquire,
     * and the newest result read by fx1_isp_read_awb_global. Both are
     * dropped by a soft reset. */
    int awb_leased;
    fx1_isp_awb_lease awb_lease;
    int awb_last_valid;
    fx1_isp_awb_global awb_last;
    /* EE profiles a frame may use: every profile selected since EE was last
     * seen idle (bit p = profile p). */
    unsigned ee_sel, ee_in_use;
    /* GTM manual load (MAS §9.2): step 1 done, EN to restore, and the
     * IDMA_FRAME_COUNT that proves a frame queued after it was taken. */
    int gtm_manual, gtm_en, gtm_wait;
    uint32_t gtm_target;
} fx1_isp_dev;

/* Events reported by fx1_isp_irq(). */
typedef struct fx1_isp_events {
    uint32_t common;   /* COMMON_IRQ_STATUS bits that were set (now acknowledged) */
    uint32_t dma;      /* DMA_IRQ_STAT bits that were set (now acknowledged) */
    uint32_t dma_err;  /* DMA_ERR at the time of the interrupt */
    unsigned frames_done; /* output buffers that completed (fetch with fx1_isp_next_done) */
    unsigned frames_lost; /* frames known lost (fetch with fx1_isp_next_lost) */
    uint32_t output;   /* OUTPUT_IRQ_STATUS bits that were set (now acknowledged) */
} fx1_isp_events;

/* ---- Bring-up (HAS §9.1 order: reset, geometry, profile/LUTs, ISP_EN, DMA) --- */
/* Includes a soft reset: FX1_ISP_EBUSY if it has not finished (the device is
 * initialised; complete it with fx1_isp_soft_reset_ex). */
int fx1_isp_init(fx1_isp_dev *dev, const fx1_isp_hal *hal);
/* Cancels the stream: soft reset, DMA engines disabled, every buffer back to
 * the caller. Returns the number of frames that were in flight (discarded),
 * FX1_ISP_ESTATE if not streaming, or FX1_ISP_EBUSY if the soft reset has not
 * finished (still STREAMING, nothing returned yet: call again). Use it when a
 * watchdog sees no progress. */
int fx1_isp_stop(fx1_isp_dev *dev);
/* Soft reset (DEC-14): configuration, LUTs and LSC profiles are kept; buffer
 * ownership, results and interrupt state are cleared; rotation returns to 0.
 * Requested as a common-scope recovery (MAS v1.07 §9.9.8): the driver waits,
 * bounded, for a running recovery and for its own to finish (BUSY = 0). */
void fx1_isp_soft_reset(fx1_isp_dev *dev);
/* As fx1_isp_soft_reset, with its result: FX1_ISP_OK once the reset has
 * finished; FX1_ISP_EBUSY when it outlasted the driver's bounded wait (MAS
 * v1.07 §9.9.8: the drain lasts as long as the transactions already
 * accepted). The driver's queue and leases are then unchanged and the reset
 * stays pending: call again (it does not request a second reset), or any
 * call that settles the queue completes it. fx1_isp_init and fx1_isp_stop
 * return FX1_ISP_EBUSY in the same case (stop: still STREAMING). */
int fx1_isp_soft_reset_ex(fx1_isp_dev *dev);
/* DMA-only soft reset (DMA_CTRL.soft_reset, MAS Table 183) while streaming:
 * the frames in flight are discarded (returned: their count), buffer
 * ownership and both rotations return to 0, DMA_IRQ_STAT/DMA_ERR are cleared.
 * The hardware keeps the DONE bits of completed outputs (MAS v1.07 §9.9.8);
 * the driver discards those frames too and clears their DONE bits.
 * Configuration, the pipeline state, published statistics, COMMON_IRQ_STATUS
 * and the statistics leases are kept (a measurement in flight is discarded);
 * the stream stays enabled, so the caller re-queues its frames from buffer 0.
 * A pending GTM hand-over then waits for one frame taken after the reset.
 * FX1_ISP_ESTATE if not streaming; FX1_ISP_EBUSY if the reset has not
 * finished in the driver's bounded wait (queue unchanged, reset pending:
 * call again; see fx1_isp_soft_reset_ex). */
int fx1_isp_dma_reset(fx1_isp_dev *dev);
/* Input geometry and Bayer order (0 RGGB, 1 GRBG, 2 GBRG, 3 BGGR, DEC-11).
 * width/height are the even external dimensions before the Bayer phase trim,
 * up to 3842x2162; the active image ((W - SH) & ~1, (H - SV) & ~1) must be
 * 32x32..3840x2160 (MAS v1.07 §6.6.3.4-6.6.3.5: software validates it, the
 * DMA does not). FX1_ISP_EINVAL otherwise. Pipeline idle only. */
int fx1_isp_set_geometry(fx1_isp_dev *dev, uint32_t width, uint32_t height, uint32_t bayer);
/* COMMON_INPUT_FORMAT.DEPTH of the right-aligned input containers:
 * FX1_ISP_INPUT_RAW12 (reset) or FX1_ISP_INPUT_RAW10 (the ISP shifts left two
 * once). Do not also normalize RAW10 data in software. Pipeline idle only. */
#define FX1_ISP_INPUT_RAW12 0u
#define FX1_ISP_INPUT_RAW10 1u
int fx1_isp_set_input_format(fx1_isp_dev *dev, uint32_t depth);

/* Output channel geometry (MAS v1.07 §6.20.1.1, §8.3): channel 0/1 NV12,
 * channel 2 RGB888 (from V107-8 the integrated ODMA writes CH1/CH2). The
 * crop is a half-open, even rectangle in active-image pixels (enable = 0:
 * the full active image); `scale` is the RESIZER_CTRL.SCALE preset (0 FullRes)
 * and `enable` = 0 bypasses the scaler. Both are IDLE-class writes (EBUSY
 * while streaming). They return FX1_ISP_OK when the settled channel geometry
 * is VALID and FX1_ISP_EINVAL otherwise (the reason: fx1_isp_geometry_status).
 * The crop is disabled while its words change, so no intermediate setup is
 * invalid (TUNE_ERROR.INVALID is sticky). */
int fx1_isp_set_crop(fx1_isp_dev *dev, unsigned ch, int enable, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
int fx1_isp_set_resize(fx1_isp_dev *dev, unsigned ch, int enable, uint32_t scale);
/* CHc_GEOMETRY_STATUS once the derivation has settled (BUSY clear, within two
 * derivations): VALID bit 0, ERROR bits 7:4 (CSR v1.07 priority code).
 * FX1_ISP_EAGAIN if it is still deriving after that. */
int fx1_isp_geometry_status(fx1_isp_dev *dev, unsigned ch, uint32_t *status);
/* Pre-submission check of an input frame (MAS v1.07 §6.6.3.2: firmware
 * validates the container's unused upper bits; the ISP has no detector).
 * Returns the number of samples with a bit set above the native depth
 * (bits [15:12] for RAW12, [15:10] for RAW10); 0 means the frame may be
 * queued. A RAW10 frame already normalized to RAW12 fails this check. */
size_t fx1_isp_raw_upper_bits(const uint16_t *samples, size_t count, uint32_t depth);
/* Register writes of a profile, in order. In legacy mode a profile that
 * loads the Gamma curve, the GTM manual curve or an EE resident is applied
 * with that block disabled first (its own writes enable it again), so it
 * also works over an earlier configuration; FX1_ISP_EIO when a Gamma or EE
 * confirmation it pulses was refused (e.g. the pipeline was not idle). In
 * profile mode the writes are replayed as is (they land in the selected
 * bank). Callers that ignore the result keep the old behaviour. */
int fx1_isp_apply(fx1_isp_dev *dev, const fx1_isp_reg_write *writes, size_t n);

/* ---- Discovery (CSR v1.07 TUNE_ID / TUNE_REV / TUNE_CAPS) --------------------- */
typedef struct fx1_isp_identity {
    uint32_t magic;     /* TUNE_ID */
    uint32_t revision;  /* TUNE_REV */
    uint32_t caps;      /* TUNE_CAPS: FX1_ISP_TUNE_CAPS_*_MASK */
} fx1_isp_identity;
int fx1_isp_discover(fx1_isp_dev *dev, fx1_isp_identity *out);
/* FX1_ISP_OK if every bit of `caps` is set in TUNE_CAPS, else FX1_ISP_ENOTSUP:
 * firmware checks the capabilities it relies on before using them. */
int fx1_isp_require_caps(fx1_isp_dev *dev, uint32_t caps);

/* ---- Additional output channels (MAS v1.07 §6.25.3.8) ------------------------ *
 * CH1 is NV12 (Y and UV planes), CH2 one packed RGB888 plane (`uv` unused,
 * y_stride >= 3 x width). Programmed in CONFIG state; fx1_isp_start then
 * latches OUTPUT_ENABLE and requires buffers for every enabled channel. Each
 * queued frame arms the same buffer index on every enabled channel, and a
 * frame is complete when all of them published DONE for it. */
typedef struct fx1_isp_channel_buffers {
    uint64_t y[FX1_ISP_NUM_BUFFERS];
    uint64_t uv[FX1_ISP_NUM_BUFFERS];
    uint32_t y_stride, uv_stride;
} fx1_isp_channel_buffers;
int fx1_isp_set_channel_buffers(fx1_isp_dev *dev, unsigned ch, const fx1_isp_channel_buffers *b);

/* ---- Profile transaction (MAS v1.07 §9.7-9.9) ------------------------------- *
 * Profile mode (TUNE_MODE) changes only while idle; set the input geometry
 * and format first (input-domain words are rejected in profile mode). A
 * profile is built in a bank: begin (TUNE_BANK, INIT copies the current
 * configuration), write it with fx1_isp_apply (writes land in the selected
 * bank) and the table loaders, confirm bank payloads, seal with a profile
 * ID, then apply (from the next frame, or EXACT at ingress frame `frame`).
 * Each command runs in a clean TUNE_ERROR window: bits left by earlier
 * accesses are acknowledged first and kept in dev->tune_error_prior. It
 * returns FX1_ISP_EIO when it raised TUNE_ERROR bits (returned in *err, NULL
 * allowed, and acknowledged), FX1_ISP_EBUSY if the window could not be
 * cleared (*err: the bits that stayed set). */
int fx1_isp_profile_mode(fx1_isp_dev *dev, int enable);
int fx1_isp_profile_begin(fx1_isp_dev *dev, unsigned bank, uint32_t *err);
/* TUNE_TABLE_CMD with FX1_ISP_TUNE_TABLE_CMD_*_CONFIRM_MASK bits; EIO unless
 * TUNE_TABLE_STATUS then shows every requested table valid. */
int fx1_isp_profile_confirm_tables(fx1_isp_dev *dev, unsigned bank, uint32_t mask, uint32_t *err);
/* INIT clears the bank's payload validity: a profile that enables Gamma loads
 * its curve into the bank after INIT (banked port TUNE_GAMMA_PORT/DATA,
 * verified, then GAMMA_CONFIRM). The legacy loader and LUT_LOAD_CONFIRM are
 * rejected with MODE in profile mode. */
int fx1_isp_profile_load_gamma(fx1_isp_dev *dev, unsigned bank, const uint16_t lut[256], uint32_t *err);
int fx1_isp_profile_seal(fx1_isp_dev *dev, unsigned bank, uint32_t profile_id, uint32_t *err);
/* The default profile-mode bring-up of a legacy profile: `writes` were
 * applied (fx1_isp_apply) in legacy mode, after the input geometry and
 * format. Enters profile mode, INIT copies that configuration into `bank`,
 * the payloads INIT cleared are loaded again (the confirmed legacy Gamma
 * curve, read back through GAMMA_LUT_RDATA, through the banked port; the
 * GTM manual curve and TUNE payload writes replayed in order through their
 * own ports), then SEAL with `profile_id` and APPLY from the next
 * frame. CONFIG state only. Any failure is returned with its TUNE_ERROR
 * bits (no silent fallback to legacy mode); leave profile mode with
 * fx1_isp_profile_mode(dev, 0). */
int fx1_isp_profile_from_legacy(fx1_isp_dev *dev, unsigned bank, const fx1_isp_reg_write *writes, size_t n,
                                uint32_t profile_id, uint32_t *err);
int fx1_isp_profile_apply(fx1_isp_dev *dev, unsigned bank, int exact, uint32_t frame, uint32_t *err);
int fx1_isp_profile_cancel(fx1_isp_dev *dev, uint32_t *err);
int fx1_isp_profile_unseal(fx1_isp_dev *dev, unsigned bank, uint32_t *err);
typedef struct fx1_isp_profile_status {
    uint32_t status;        /* TUNE_STATUS */
    uint32_t active_id;     /* TUNE_ACTIVE_ID */
    uint32_t active_frame;  /* TUNE_ACTIVE_FRAME: first ingress frame of the active profile */
    uint32_t next_frame;    /* TUNE_NEXT_FRAME: ingress ID of the next admitted frame */
} fx1_isp_profile_status;
int fx1_isp_profile_state(fx1_isp_dev *dev, fx1_isp_profile_status *out);

/* ---- Tables and gated sets -------------------------------------------------- */
/* Gamma curve (MAS v1.07 §6.14.4): writes the 256 entries through the legacy
 * port, verifies them through GAMMA_LUT_RDATA (FX1_ISP_EIO on a mismatch) and
 * confirms them (LUT_LOAD_CONFIRM). The curve applies only once confirmed,
 * which needs GAMMA_CTRL.EN = 0 and an idle pipeline: FX1_ISP_EBUSY if Gamma
 * is enabled (nothing is written), FX1_ISP_EAGAIN if the curve is written but
 * the pipeline was not idle (call again when idle). Legacy mode only. */
int fx1_isp_load_gamma(fx1_isp_dev *dev, const uint16_t lut[256]);
/* bank 0 luma / 1 activity (64 entries), 2 / 3 contrast (32 entries), into
 * the profile EE_CTRL.profile_wr_sel names (profile 0 after reset). */
int fx1_isp_load_ee_table(fx1_isp_dev *dev, unsigned bank, const uint16_t *table, unsigned n);
/* GTM software table (65 entries), following MAS §9.2:
 *   1. fx1_isp_gtm_manual_begin clears EN and sets MANUAL (frames now pass GTM
 *      through, and no new automatic curve is built);
 *   2. one frame must then run, so that no automatic curve built before
 *      step 1 is still waiting for its SOF: fx1_isp_load_gtm_lut returns
 *      FX1_ISP_EAGAIN until the pipeline has taken a frame queued after
 *      step 1 (keep streaming, or run one frame; its output may fail). Only
 *      when no frame has been taken since i_rst_n (IDMA_FRAME_COUNT = 0, kept
 *      by soft reset and fx1_isp_init) and none is queued can no curve be
 *      pending, and the load proceeds at once;
 *   3-5. it writes the 65 points, verifies them through GTM_LUT_RDATA
 *      (FX1_ISP_EIO on a mismatch) and restores EN; the curve applies from the
 *      next SOF and stays while MANUAL is set.
 * fx1_isp_load_gtm_lut performs step 1 itself if it was not done. */
int fx1_isp_gtm_manual_begin(fx1_isp_dev *dev);
int fx1_isp_load_gtm_lut(fx1_isp_dev *dev, const uint16_t lut[65]);

/* EE resident profiles (MAS §6.18.4.1, §9.2). Each of the three profiles owns
 * the scalars and the four tables below. */
typedef struct fx1_isp_ee_profile {
    uint32_t alpha, beta, clamp, feature_en;         /* EE_ALPHA .. EE_FEATURE_EN */
    uint32_t radial_center, r2_th[3], radial_gain_01, radial_gain_23;
    uint16_t luma[64], activity[64], contrast_pos[32], contrast_neg[32];
} fx1_isp_ee_profile;
/* Loads profile 0..2 with full 32-bit writes and verifies its tables through
 * EE_LUT_RDATA (FX1_ISP_EIO on a mismatch). While EE is busy, every profile
 * selected since EE was last seen idle may still be used by a frame in
 * flight, and is refused with FX1_ISP_EBUSY; the selector written last is no
 * proof of which profile those frames use.
 * MAS v1.07 §9.9.1: any scalar or table write clears the resident's
 * confirmation, and an unconfirmed resident passes frames through. In legacy
 * mode the confirmation needs EE disabled and the pipeline idle: the load
 * then confirms and returns FX1_ISP_OK; otherwise the resident is written but
 * unconfirmed and FX1_ISP_EAGAIN is returned (fx1_isp_ee_confirm later). */
int fx1_isp_ee_load_profile(fx1_isp_dev *dev, unsigned profile, const fx1_isp_ee_profile *p);
/* PROFILE_LOAD_CONFIRM of resident 0..2: FX1_ISP_OK once EE_STATUS reports it
 * valid; FX1_ISP_EAGAIN while EE is enabled or the pipeline busy; FX1_ISP_EIO
 * if the resident is incomplete (TUNE_ERROR.NOT_READY, cleared here). */
int fx1_isp_ee_confirm(fx1_isp_dev *dev, unsigned profile);
/* Selects profile 0..2 and the enable; both take effect at the next SOF. */
int fx1_isp_ee_select(fx1_isp_dev *dev, unsigned profile, int enable);
/* Loads 4 * nx * ny UQ3.18 coefficients (R, Gr, Gb, B per node, x then y)
 * into a non-active LSC profile and validates it; returns FX1_ISP_EIO with
 * *lsc_error set if the loader rejected it. */
int fx1_isp_lsc_load(fx1_isp_dev *dev, unsigned profile, unsigned nx, unsigned ny, const uint32_t *coef,
                     uint32_t *lsc_error);
/* Selects a valid profile; it becomes active at the next frame start if it is
 * still valid then. A load into it begun before that SOF keeps the previous
 * profile active and raises LSC_ERROR.PROFILE_SEL_REJECT. */
int fx1_isp_lsc_select(fx1_isp_dev *dev, unsigned profile);
/* CCM coefficients (Q3.9, row major) and offsets. The block captures them at
 * every accepted SOF; CCM_CTRL.updated has no effect (CSR/MAS 07/10). The set
 * is not protected as a whole: call it while no frame starts (idle, or
 * between frames), or a frame may use a mix of the old and new values. */
void fx1_isp_ccm_set(fx1_isp_dev *dev, const int16_t m[9], const int16_t ofs[3], int enable);

/* AEC: shadow configuration plus commit (CSR #296); takes effect at the next SOF. */
typedef struct fx1_isp_aec_cfg {
    unsigned nx, ny;            /* 1..32, 1..24; (nx-1)*zone_w < W <= nx*zone_w, the same for H */
    unsigned zone_w, zone_h;    /* pixels */
    unsigned clip_min, clip_max; /* RAW12, inclusive, min <= max */
    unsigned th_ue, th_oe;      /* RAW12, strict, ue < oe */
    unsigned source;            /* 0 post-BPC, 1 post-DG; joins the commit (MAS v1.07 Table 9-195) */
    uint32_t context_id;
} fx1_isp_aec_cfg;
void fx1_isp_aec_configure(fx1_isp_dev *dev, const fx1_isp_aec_cfg *cfg, int enable);

/* ---- Streaming -------------------------------------------------------------- */
/* Enables the pipeline, the interrupt sources and the DMA engines. Call after
 * the profile and only in CONFIG state; use fx1_isp_stop before restarting a
 * drained stream so the hardware and driver buffer rotation both reset.
 * Returns FX1_ISP_EINVAL when the CH0 geometry is not VALID (MAS v1.07 §8.3:
 * no frame is admitted). The output geometry (RESIZER_OUT_W/H) is valid from here. */
int fx1_isp_start(fx1_isp_dev *dev, const fx1_isp_buffers *bufs);
/* Hands the next input and output buffers to the hardware (one frame).
 * First check fx1_isp_input_ready(), then fill the input at fx1_isp_next_input().
 * Returns FX1_ISP_EBUSY if all four frames are in flight, the hardware still
 * owns the next input or output, or a recovery is running; FX1_ISP_EAGAIN if
 * a recovery finished after fx1_isp_next_input() was read and restarted the
 * rotation (MAS v1.07 §9.9.8): fill the new fx1_isp_next_input() and retry.
 * Any driver call may settle that recovery (fx1_isp_input_ready() included),
 * so the check covers everything since the read, not only this call (V107-11
 * platform CI: a pair filled, then the second queued into slot 0 unfilled). */
int fx1_isp_queue_frame(fx1_isp_dev *dev);
/* As fx1_isp_queue_frame, and returns the frame's sequence number in *seq. */
int fx1_isp_queue_frame_ex(fx1_isp_dev *dev, uint32_t *seq);
/* As fx1_isp_queue_frame_ex, and gives the input buffer a software tag and a
 * capture timestamp in the FRAME_COUNTER time base (IDMA_BUF_TAG/TS, MAS
 * Table 160). The hardware carries them to the output buffer the frame
 * completes in; fx1_isp_next_done_meta returns them. The other queue calls
 * leave the input buffer's tag and timestamp registers untouched. */
int fx1_isp_queue_frame_meta(fx1_isp_dev *dev, uint32_t tag, uint32_t ts, uint32_t *seq);
/* The input to fill for the next queue; remembers the rotation it belongs to. */
unsigned fx1_isp_next_input(fx1_isp_dev *dev);
/* Non-zero when the next input may be filled and queued. Serialise this
 * check, filling and queue_frame against other driver calls. */
int fx1_isp_input_ready(fx1_isp_dev *dev);
/* Interrupt handler: reads and acknowledges the three status groups. */
void fx1_isp_irq(fx1_isp_dev *dev, fx1_isp_events *ev);
/* Pops the next completed output buffer (index into fx1_isp_buffers.y/uv and
 * fx1_isp_channel_buffers): FX1_ISP_OK and *index once every enabled channel
 * published DONE for the oldest frame, or FX1_ISP_ENODATA (also while a
 * recovery runs). FX1_ISP_EAGAIN: a recovery started during the collection
 * and dropped the acknowledgement; the frame stays queued, call again later. The buffer may be reused or
 * queued again after the caller has consumed it. */
int fx1_isp_next_done(fx1_isp_dev *dev, unsigned *index);
/* As fx1_isp_next_done, and returns the completed frame's sequence number. */
int fx1_isp_next_done_ex(fx1_isp_dev *dev, unsigned *index, uint32_t *seq);
/* Metadata published with a completed output buffer (MAS Table 160). All
 * times are FRAME_COUNTER values (core clocks, modulo 2^32): eof_ts - sof_ts
 * is the output span of the frame, eof_ts - ts its end-to-end latency. */
typedef struct fx1_isp_frame_meta {
    uint32_t tag;     /* the input buffer's IDMA_BUF_TAG when the frame started */
    uint32_t ts;      /* the input buffer's IDMA_BUF_TS when the frame started */
    uint32_t sof_ts;  /* output start of frame */
    uint32_t eof_ts;  /* last line of the frame accepted by the ODMA */
} fx1_isp_frame_meta;
/* As fx1_isp_next_done_ex, and returns the buffer's metadata in *meta (NULL
 * allowed). */
int fx1_isp_next_done_meta(fx1_isp_dev *dev, unsigned *index, uint32_t *seq, fx1_isp_frame_meta *meta);
/* Metadata of channel `ch`'s buffer `index` (MAS v1.07 Table 6-159): valid
 * once fx1_isp_next_done returned that index and until it is queued again.
 * *frame: the ingress frame ID (TUNE_CHc_BUFn_FRAME), *profile: the profile
 * ID of the frame (TUNE_CHc_BUFn_PROFILE; 0 in legacy mode); NULL allowed. */
int fx1_isp_output_meta(fx1_isp_dev *dev, unsigned ch, unsigned index, fx1_isp_frame_meta *meta, uint32_t *frame,
                        uint32_t *profile);
/* FRAME_COUNTER: the free-running 32-bit core-clock time base (0 before init). */
uint32_t fx1_isp_frame_counter(fx1_isp_dev *dev);
/* Error recovery (MAS v1.07 §9.9.8; DEC-19 superseded). An IDMA or ODMA AXI
 * error starts an automatic DMA-scope recovery in hardware: every admitted,
 * unretired frame is aborted, every IDMA_BUF_VALID / *_ODMA_BUF_FREE bit is
 * cleared and both rotations restart at buffer 0. Completed outputs (DONE)
 * and their metadata are kept. The driver settles its queue against
 * COMMON_RECOVERY_SEQUENCE once the recovery has finished (BUSY = 0): frames
 * whose output is DONE stay collectable, every other frame in flight is lost
 * (fx1_isp_next_lost) and the caller resubmits it. Settling happens in every
 * call that looks at the queue; fx1_isp_recover waits (bounded) for the
 * recovery to finish, settles the queue and acknowledges the error bits
 * (W1C). FX1_ISP_EBUSY if the recovery is still running (bits kept: call
 * again). */
int fx1_isp_recover(fx1_isp_dev *dev, uint32_t dma_err);
/* What fx1_isp_recover_ex did. Only error bits that are still set in DMA_ERR
 * are handled, so a second call with the same (stale) snapshot does nothing
 * and returns FX1_ISP_ENODATA (unless the hardware flagged the error again). */
typedef struct fx1_isp_recovery {
    uint32_t handled;          /* DMA_ERR bits acted on */
    int idma_rearmed;          /* always 0: no in-place retry since v1.07 (kept for source compatibility) */
    unsigned idma_input;
    uint32_t idma_seq;
    int odma_failed;           /* AXI_WR handled and at least one frame was aborted */
    unsigned odma_output;      /* output buffer of the oldest aborted frame */
    uint32_t odma_seq;         /* its sequence number */
    unsigned aborted;          /* frames the recovery lost (all queued, not DONE) */
    uint32_t sequence;         /* COMMON_RECOVERY_SEQUENCE settled against */
} fx1_isp_recovery;
int fx1_isp_recover_ex(fx1_isp_dev *dev, uint32_t dma_err, fx1_isp_recovery *r);
/* Pops the next frame known lost (its input index and sequence number), to be
 * resubmitted by the caller: FX1_ISP_OK, or FX1_ISP_ENODATA. */
int fx1_isp_next_lost(fx1_isp_dev *dev, uint32_t *seq, unsigned *input);
/* The report of the last completed recovery (CSR 0x8C00..0x8C2C): read with
 * BUSY = 0 and the same SEQUENCE before and after the records. Returns
 * FX1_ISP_OK, FX1_ISP_ENODATA (no recovery since i_rst_n) or FX1_ISP_EBUSY
 * (a recovery is still running). */
#define FX1_ISP_ABORT_CAUSE_READ 1u     /* AXI read error */
#define FX1_ISP_ABORT_CAUSE_WRITE 2u    /* AXI write error */
#define FX1_ISP_ABORT_CAUSE_DMA 3u      /* DMA_CTRL.soft_reset */
#define FX1_ISP_ABORT_CAUSE_COMMON 4u   /* COMMON_CTRL.soft_rst */
#define FX1_ISP_ABORT_CAUSE_MALFORMED 5u
typedef struct fx1_isp_abort_record {
    uint32_t frame;            /* ingress frame ID (TUNE_CHc_BUFn_FRAME) */
    unsigned failed_outputs;   /* bit c: enabled channel c had no DONE */
    unsigned cause;            /* FX1_ISP_ABORT_CAUSE_* */
} fx1_isp_abort_record;
typedef struct fx1_isp_abort_report {
    uint32_t sequence;         /* COMMON_RECOVERY_SEQUENCE */
    unsigned count;            /* records valid in rec[] (0..4) */
    fx1_isp_abort_record rec[4];
} fx1_isp_abort_report;
int fx1_isp_recovery_report(fx1_isp_dev *dev, fx1_isp_abort_report *out);

/* Progress snapshot for a caller's watchdog (programming guide §8.1). */
typedef struct fx1_isp_progress {
    uint32_t idma_frames, odma_frames;  /* IDMA/ODMA_FRAME_COUNT */
    unsigned in_flight;
    uint32_t dma_err;                   /* DMA_ERR */
    int pipeline_busy;                  /* COMMON_STATUS.busy */
    int isp_enabled;                    /* COMMON_CTRL.isp_en */
} fx1_isp_progress;
int fx1_isp_get_progress(fx1_isp_dev *dev, fx1_isp_progress *p);

/* ---- Statistics (MAS v1.07 §6.21-6.23, §9.8) -----------------------------------
 * AEC and AF results live in two banks per engine owned through
 * acquire/release, like AWB. Explicit use:
 *   fx1_isp_aec_acquire / fx1_isp_af_acquire  take the oldest completed result:
 *                        FX1_ISP_ENODATA if none is ready, FX1_ISP_EBUSY if one
 *                        is already held;
 *   readers              read the held result (immutable until release);
 *   fx1_isp_aec_release / fx1_isp_af_release  return it. While two results
 *                        are held or ready, enabled frames are skipped and
 *                        counted (fx1_isp_aec_drops / fx1_isp_af_drops).
 * A reader that finds the lease gone (a soft reset cleared it) drops it and
 * returns FX1_ISP_ENODATA. The AEC results are RAW12 Bayer statistics of the
 * selected tap; there are no per-CFA results in v1.07. */
int fx1_isp_aec_acquire(fx1_isp_dev *dev, fx1_isp_stat_lease *out);
/* One logical 64-bit result (MAS Table 9-193); FX1_ISP_EINVAL for an index
 * outside the table. */
int fx1_isp_aec_result(fx1_isp_dev *dev, uint32_t index, uint64_t *value);
int fx1_isp_aec_read_global(fx1_isp_dev *dev, fx1_isp_aec_global *out);
typedef struct fx1_isp_aec_zone {
    uint64_t sum, count, oe, ue;
    uint32_t min, max;             /* empty zone 4095 / 0 */
} fx1_isp_aec_zone;
/* Zones first..first+n-1 of the captured grid (FX1_ISP_EINVAL beyond it). */
int fx1_isp_aec_read_zones(fx1_isp_dev *dev, unsigned first, unsigned n, fx1_isp_aec_zone *zones);
/* The 64-bin histogram of every sample and, if `weighted` is not NULL, the
 * weighted histogram (zero unless enabled in profile mode). */
int fx1_isp_aec_read_hist(fx1_isp_dev *dev, uint64_t hist[64], uint64_t *weighted);
int fx1_isp_aec_release(fx1_isp_dev *dev);
/* Skipped measurements (no FREE bank or an illegal set); clear != 0 clears. */
int fx1_isp_aec_drops(fx1_isp_dev *dev, uint32_t *drops, int clear);
/* Compatibility reader: the newest AEC global result. It acquires, reads
 * and releases every ready result and keeps the newest; with none ready it
 * returns the one it read last. FX1_ISP_ENODATA before any result (or after
 * a soft reset); FX1_ISP_EBUSY while the caller holds a lease. */
int fx1_isp_read_aec_global(fx1_isp_dev *dev, fx1_isp_aec_global *out);

/* AWB results live in two banks owned through acquire/release (MAS §6.22.4).
 * Explicit use:
 *   fx1_isp_awb_acquire  takes the oldest completed result: FX1_ISP_ENODATA
 *                        if none is ready, FX1_ISP_EBUSY if one is already held;
 *   fx1_isp_awb_read_global / fx1_isp_awb_read_zones read the held result
 *                        (stable until release; zones of the captured grid);
 *   fx1_isp_awb_release  returns it to the hardware; on return the bank can
 *                        take the next frame. Release after the last
 *                        read: while two results are held or ready, enabled
 *                        frames are skipped and counted in AWB_DROP_COUNT.
 * A read that finds the lease gone (a soft reset cleared it) drops it, and
 * the compatibility reader's cached result, and returns FX1_ISP_ENODATA.
 * Limitation: a reset issued behind the driver while it holds no lease is not
 * visible to it; whoever resets the ISP must call fx1_isp_soft_reset or
 * fx1_isp_init so that the driver forgets the results. */
int fx1_isp_awb_acquire(fx1_isp_dev *dev, fx1_isp_awb_lease *out);
int fx1_isp_awb_read_global(fx1_isp_dev *dev, fx1_isp_awb_global *out);
typedef struct fx1_isp_awb_zone {
    uint64_t sum[3];   /* R, G, B (35-bit) */
    uint32_t count;
} fx1_isp_awb_zone;
/* Zones first..first+n-1 of the captured grid (FX1_ISP_EINVAL beyond it). */
int fx1_isp_awb_read_zones(fx1_isp_dev *dev, unsigned first, unsigned n, fx1_isp_awb_zone *zones);
int fx1_isp_awb_release(fx1_isp_dev *dev);
/* Loss reporting: OVERRUN and DROP_COUNT (enabled measurements skipped for
 * lack of a free bank or an illegal grid). clear != 0 acknowledges them
 * (CLEAR_REPORT); data and ownership are not affected. */
typedef struct fx1_isp_awb_status {
    unsigned ready_count;
    int leased, write_busy, overrun;
    uint32_t drop_count;
} fx1_isp_awb_status;
int fx1_isp_awb_report(fx1_isp_dev *dev, fx1_isp_awb_status *out, int clear);
/* Compatibility reader (API of the previous CSR revision): the newest AWB
 * global result. It acquires, reads and releases every ready result and keeps
 * the newest; with none ready it returns the one it read last.
 * FX1_ISP_ENODATA before any result (or after a soft reset); FX1_ISP_EBUSY
 * while the caller holds a lease. */
int fx1_isp_read_awb_global(fx1_isp_dev *dev, fx1_isp_awb_global *out);

/* AF results (MAS v1.07 §6.23.3.7): per window the focus, luma and count
 * sums and the effective rectangle [x0, x1) x [y0, y1). */
typedef struct fx1_isp_af_window {
    uint64_t focus, luma, count;
    unsigned x0, y0, x1, y1;
} fx1_isp_af_window;
int fx1_isp_af_acquire(fx1_isp_dev *dev, fx1_isp_stat_lease *out);
int fx1_isp_af_read(fx1_isp_dev *dev, fx1_isp_af_window win[16], uint32_t *mask, unsigned *mode);
int fx1_isp_af_release(fx1_isp_dev *dev);
int fx1_isp_af_drops(fx1_isp_dev *dev, uint32_t *drops, int clear);
/* Compatibility reader (API of the previous CSR revision): the newest AF
 * scores, by the same acquire/read/release sweep as fx1_isp_read_aec_global. */
int fx1_isp_read_af(fx1_isp_dev *dev, fx1_isp_af_scores *out);

#ifdef __cplusplus
}
#endif

#endif /* FX1_ISP_DRV_H */
