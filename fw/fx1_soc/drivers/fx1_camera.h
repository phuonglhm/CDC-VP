/*
 * FX1 camera manager (CSI-2 M6a): camera sensor -> CSI-2 RX DMA -> ISP -> NV12,
 * on top of the hardware drivers (fx1_csi2_drv, fx1_isp_drv, fx1_ccs_sensor,
 * fx1_i2c), which it does not change. Bare metal, one hart.
 *
 * Two paths (cfg.path, plan DIRECT_CSI_ISP D3), chosen at configure, while idle:
 *   - memory (FX1_CAMERA_PATH_MEMORY, the default and the behaviour below):
 *     CSI-2 RX DMA -> RAW sides in DDR -> ISP IDMA;
 *   - direct (FX1_CAMERA_PATH_DIRECT): the CSI-2 RX AXI4-Stream master feeds
 *     the ISP's stream ingress (VP block ISP_INGRESS, fx1_isp_ingress_drv.h,
 *     contract components/FX1_Components/mipi-csi2/docs/DIRECT_CSI_ISP_CONTRACT.md);
 *     the CSI-2 RX DMA and the ISP IDMA stay off: no RAW side, no staging, no
 *     DDR round trip. RAW10 runs native (COMMON_INPUT_FORMAT depth 10, no
 *     shift). A frame is published only when its ingress context entry is
 *     READY with outcome OK and the ODMA output carrying the entry's TAG is
 *     DONE (publish gate, contract §9.2): capture, CSI FRAME_ID, epoch, FS
 *     timestamp and ISP frame ID come from that entry. Any other outcome
 *     (CSI_DROPPED, CSI_MISSING_FE, INVALID, RESET, ISP_FAILED) is counted
 *     (stats.direct_failed[outcome]) and its output, if it completed one, is
 *     given back unread. The memory-path fields of a frame are N/A: csi_side
 *     FX1_CAMERA_NO_SIDE, zero_copy 0, isp_in 0. Output ownership, dequeue /
 *     release, stop deadline, state 3 (ESENSOR) and EBUSY are as below. While
 *     the application holds every output the ingress drops new frames at
 *     their SOF (no output credit, TREADY 1): the sensor is never paused.
 *     Stop: sensor standby, ingress back to SOURCE memory (refused while a
 *     frame is inside: retried until the deadline), ISP stop (its reset
 *     cancels the frames the ISP had not retired). A frame the ISP had
 *     already retired keeps its entry until its CSI terminal / boundary
 *     (contract §9.1 rule 8); when the sensor stopped before sending it (a
 *     capture without FE, then no FS), a CSI-2 soft reset after the ISP stop
 *     ends it (outcome RESET, never published; stats.stop_csi_resets). The
 *     entries are drained and counted in stop_cancelled, then SOURCE memory
 *     is set and read back; until then stop returns FX1_CAMERA_EBUSY with
 *     the state unchanged (P35).
 *
 * Buffer lifecycle (plan §9):
 *   - the CSI-2 DMA writes RAW16 (right-aligned) into two sides;
 *   - a side that ends SW_OWNED_ERROR is released at once, never sent to the
 *     ISP (stats.csi_err);
 *   - RAW12: zero-copy. The side's address is written into IDMA_BUF_ADDR of
 *     the ISP input slot about to be armed (the ISP takes it at arm), the
 *     frame is queued, and the side is released only when that slot's
 *     IDMA_BUF_VALID is clear and no IDMA read error is pending: a read
 *     error clears VALID too, and recovery re-arms the same input from the
 *     same address, so the side stays held until a read succeeds (or stop);
 *   - RAW10: the side is converted into the slot's staging buffer
 *     (sample << 2: ISP container bits [11:2]) and released at once;
 *   - an ISP output (NV12) completes in queue order; dequeue hands it to the
 *     application, release gives the slot back. While the application holds
 *     the slot the ISP's next output is not queued (back-pressure);
 *   - while the ISP cannot take a frame, the CSI side stays held: with both
 *     sides held the CSI-2 RX drops frames (counted by the controller).
 *
 * Interrupts: the application's trap handler calls fx1_camera_csi_isr() for
 * PLIC source FX1_IRQ_MIPI and fx1_camera_isp_isr() for FX1_IRQ_ISP. The CSI-2
 * ISR acknowledges and collects the interrupt status; the ISP ISR acknowledges
 * through the ISP driver (fx1_isp_irq), collects fatal DMA errors and returns
 * zero-copy sides the IDMA has read (rule above), so a slow consumer does not
 * hold both sides. Everything else runs in fx1_camera_poll() (from dequeue or
 * the main loop) with the ISP driver calls inside interrupt-masked sections.
 *
 * Stop: sensor standby (written and read back), CSI-2 DMA stop (abort if the
 * capture in flight does not finish), ISP stop, sides back to the CSI-2 DMA,
 * all within one deadline: every sensor transfer gets what is left of it and
 * none starts once it has passed (bound: FX1_CAMERA_STOP_SLACK_US). Frames still in the pipeline are cancelled and
 * counted (stats.stop_cancelled); frames the application already dequeued
 * stay readable until the next configure, and releasing them is a no-op.
 * If the sensor does not confirm standby (I2C error), the CSI-2 RX and the
 * ISP are still stopped, stop returns FX1_CAMERA_ESENSOR and the manager
 * stays in state 3 (stopped, sensor not confirmed): stop again retries the
 * standby only; configure is refused until it succeeds.
 */
#ifndef FX1_CAMERA_H
#define FX1_CAMERA_H

#include <stddef.h>
#include <stdint.h>

#include "fx1_ccs_sensor.h"
#include "fx1_csi2/fx1_csi2_drv.h"
#include "fx1_i2c.h"
#include "fx1_isp/fx1_isp_drv.h"
#include "fx1_isp/fx1_isp_ingress_drv.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FX1_CAMERA_OK        0
#define FX1_CAMERA_EINVAL   -20
#define FX1_CAMERA_ESTATE   -21
#define FX1_CAMERA_ETIMEOUT -22
#define FX1_CAMERA_EDEV     -23 /* a hardware driver failed (sensor, CSI-2, ISP) */
#define FX1_CAMERA_ESENSOR  -24 /* stop: the sensor did not confirm standby (state 3) */
#define FX1_CAMERA_EBUSY    -25 /* stop: the ISP's reset (or, direct, the ingress cleanup) has not finished; call again */

/* fx1_camera_stop returns within timeout_us + FX1_CAMERA_STOP_SLACK_US (200 µs
 * for the whole call): a sensor transfer started before the deadline ends
 * within its budget (what is left, at most the sensor's own timeout) +
 * FX1_I2C_ABORT_US (its abort; the idle wait before it is inside the budget);
 * the CSI-2 DMA abort gets FX1_CAMERA_STOP_ABORT_US after the deadline (the
 * safe drain is not skipped); the ISP reset and register accesses take the
 * rest (70 µs). */
#define FX1_CAMERA_STOP_ABORT_US 30u
#define FX1_CAMERA_STOP_SLACK_US 200u
/* Direct stop: budget of the CSI-2 soft reset that ends a capture left without
 * terminal, and of the ingress settle after it (both inside the slack). */
#define FX1_CAMERA_STOP_CSI_RESET_US 40u
#define FX1_CAMERA_STOP_SETTLE_US 40u

#define FX1_CAMERA_PATH_MEMORY 0
#define FX1_CAMERA_PATH_DIRECT 1
#define FX1_CAMERA_NO_SIDE 0xFFu      /* fx1_camera_frame.csi_side of a direct frame (no RAW side) */
#define FX1_CAMERA_OUTCOMES 7u        /* direct_failed[] index: ISP_INGRESS OUTCOME code (1..6) */

typedef struct fx1_camera_cfg {
    int path;                          /* FX1_CAMERA_PATH_MEMORY (0) or FX1_CAMERA_PATH_DIRECT */
    /* image */
    uint32_t width, height;            /* even; width a multiple of 8 (16-byte RAW16 lines) */
    uint32_t bits;                     /* 10 or 12 */
    /* source: the sensor over I2C0, or the CSI-2 RX test pattern generator */
    int tpg;                           /* 1: TPG, no sensor, one frame per fx1_camera_tpg_frame() */
    uint32_t lanes;                    /* sensor: 1, 2, 4 */
    uint16_t line_length, frame_length;/* sensor timing (CCS) */
    /* buffers (16-byte aligned); direct: csi_side / staging unused */
    uint32_t csi_side[2], csi_side_size;
    uint32_t staging[FX1_ISP_NUM_BUFFERS]; /* RAW10: ISP inputs; unused with RAW12 */
    uint32_t y[FX1_ISP_NUM_BUFFERS], uv[FX1_ISP_NUM_BUFFERS];
    uint32_t y_stride, uv_stride;      /* >= ISP output width, multiples of 16 */
    /* ISP profile (CSR writes), applied before the ISP starts */
    const fx1_isp_reg_write* profile;
    size_t profile_len;
    /* Optional: called for every valid CSI-2 side before it goes to the ISP
     * (address of the RAW16 image, its capture number and CSI FRAME_ID), e.g.
     * to keep a copy of the original RAW. Runs in fx1_camera_poll. Memory path
     * only: the direct path has no RAW in memory. */
    void (*capture_hook)(void* ctx, uint32_t raw16, uint32_t capture, uint16_t csi_frame_id);
    /* Optional: called on an ISP DMA error (fatal DMA_ERR bits) before the
     * manager settles the ISP's automatic recovery (fx1_isp_recover_ex: every
     * frame in flight is lost, the rotations restart at buffer 0), so the
     * application can repair the cause, e.g. fx1_camera_set_output. */
    void (*isp_error_hook)(void* ctx, uint32_t dma_err);
    void* hook_ctx;
} fx1_camera_cfg;

/* A completed NV12 frame and where it came from. */
typedef struct fx1_camera_frame {
    uint32_t capture;      /* memory: manager sequence of the CSI-2 capture, from 1;
                            * direct: low 32 bits of the CSI capture sequence (never 0) */
    unsigned csi_side;     /* direct: FX1_CAMERA_NO_SIDE */
    uint16_t csi_frame_id; /* FRAME_ID of the side at its claim / of the capture (direct) */
    uint32_t isp_seq;      /* ISP driver sequence (direct: the ingress TAG) */
    unsigned isp_input, isp_output; /* direct: isp_input N/A (0) */
    int zero_copy;         /* 1: the ISP read the CSI-2 side itself (direct: 0) */
    uint32_t isp_in;       /* address the ISP read (CSI side or staging; direct: 0) */
    uint32_t y, uv, width, height;
    /* direct only (0 on the memory path): the ingress context entry */
    int path;              /* FX1_CAMERA_PATH_* of the frame */
    uint64_t capture_seq;  /* 64-bit CSI capture sequence */
    uint16_t epoch;        /* CSI epoch at the capture start */
    uint64_t ts_ns;        /* FS timestamp */
    uint32_t isp_frame;    /* ISP frame ID (statistics, TUNE_CHx_BUFn_FRAME) */
} fx1_camera_frame;

typedef struct fx1_camera_stats {
    uint32_t csi_ok, csi_err;          /* sides completed valid / with error (error: released, not sent) */
    uint32_t csi_irqs, isp_irqs;
    uint32_t isp_queued, isp_done, isp_lost, isp_errors;
    uint32_t held_for_isp;             /* polls that found a valid side waiting for the ISP */
    uint32_t timeouts;
    uint32_t stop_cancelled;           /* frames in the pipeline dropped by stop (waiting, in the ISP, not dequeued) */
    uint32_t sensor_errors;            /* standby not confirmed by the sensor */
    /* direct path */
    uint32_t direct_ok;                /* context entries OK and published */
    uint32_t direct_failed[FX1_CAMERA_OUTCOMES]; /* entries not published, by OUTCOME code */
    uint32_t direct_no_output;         /* OK entry without a DONE output carrying its TAG (never expected) */
    uint32_t stop_csi_resets;          /* direct stop: CSI-2 soft resets that ended a capture left without terminal */
} fx1_camera_stats;

typedef struct fx1_camera {
    fx1_camera_cfg cfg;
    fx1_i2c_t bus;
    fx1_ccs sensor;
    fx1_csi2_dev csi;
    fx1_isp_dev isp;
    fx1_isp_buffers bufs;
    volatile uint32_t csi_events;      /* IRQ_STATUS bits collected by the ISR */
    volatile uint32_t isp_fatal;       /* fatal DMA_ERR bits collected by the ISR */
    int state;                         /* 0 idle, 1 configured, 2 running, 3 stopped with the sensor not confirmed */
    /* stop: sensor and CSI-2 DMA stopped, the ISP's reset still draining
     * (fx1_isp_stop EBUSY); the next stop retries the ISP only */
    int isp_stop_pending;
    int stop_sensor_rc;                /* the standby result of that first stop */
    /* CSI-2 sides */
    int side_state[2];                 /* 0 HW, 1 waiting for the ISP, 2 read by the ISP (zero-copy) */
    uint32_t side_capture[2];
    uint16_t side_frame_id[2];
    unsigned side_input[2];
    /* frames in the ISP, by sequence (seq % 16) */
    fx1_camera_frame rec[16];
    /* completed frames, oldest first */
    fx1_camera_frame ready[FX1_ISP_NUM_BUFFERS];
    unsigned ready_head, ready_count;
    uint32_t out_owned[FX1_ISP_NUM_BUFFERS]; /* capture held by the application in that output slot, 0 free */
    uint32_t capture_seq;
    fx1_camera_stats stats;
    /* direct path */
    fx1_isp_ingress ing;               /* valid once a direct configure has run */
    int ing_ready;
    /* direct: output slots to give to the ODMA again: set only for a slot no
     * one holds (not the application, not the ODMA, not a context entry still
     * to come); cleared by every successful arm and when a slot is published */
    uint32_t unarmed;
    /* direct stop: ISP stopped, the ingress not drained yet (a context entry
     * waiting for its CSI terminal, SOURCE not back to memory); the next stop
     * resumes the cleanup only (P35) */
    int stop_cleanup_pending;
    int stop_isp_err;                  /* the ISP stop of that stop failed (EDEV once the cleanup completes) */
    int stop_csi_reset;                /* the CSI-2 soft reset of that stop is done */
} fx1_camera;

/* Probes the CSI-2 RX and the ISP, and the sensor unless cfg->tpg. */
int fx1_camera_probe(fx1_camera* cam, const fx1_csi2_hal* csi_hal, const fx1_isp_hal* isp_hal, int tpg);
/* Sensor mode (standby), D-PHY start, CSI-2 DMA (RAW16, both sides), CSI-2
 * interrupts, ISP geometry (RGGB), profile, buffers, ISP start. Direct path:
 * CSI-2 stream RAW16 on (DMA off), ISP_INGRESS present and bound (EDEV
 * otherwise), ISP depth (10 / 12), profile, ODMA only (fx1_isp_start_direct),
 * SOURCE = direct, every output armed. Only from state 0: switching path or
 * source needs stop first. */
int fx1_camera_configure(fx1_camera* cam, const fx1_camera_cfg* cfg);
/* DMA start and sensor stream on (TPG: DMA start only). On an error the
 * manager stays configured (state 1), the CSI-2 DMA possibly started:
 * fx1_camera_stop cleans up (M6b review F1). */
int fx1_camera_start(fx1_camera* cam);
/* TPG: exactly one frame, state 2 only. FX1_CAMERA_ESTATE, nothing written,
 * while a stop is pending (EBUSY returned, not yet completed): only stop may
 * be called then (re-review D3 R1). */
int fx1_camera_tpg_frame(fx1_camera* cam);
/* Stop within timeout_us + FX1_CAMERA_STOP_SLACK_US, from state 1
 * (configured, or a start that failed half-way), 2, or 3 (see the header
 * comment): FX1_CAMERA_OK, state 0;
 * FX1_CAMERA_ESENSOR, state 3 (CSI-2 RX and ISP stopped, the sensor maybe
 * still streaming; call again to retry the standby); FX1_CAMERA_ETIMEOUT, the
 * CSI-2 DMA did not stop, state unchanged (1 or 2; call again);
 * FX1_CAMERA_EBUSY, the ISP's reset is still draining (fx1_isp_stop EBUSY:
 * MAS v1.07 §9.9.8 sets no bound): state unchanged, the CSI-2 sides and the
 * frames stay with the ISP; call again until it finishes (only the ISP stop
 * is retried); direct path, also: the ISP is stopped but the ingress has not
 * drained within the deadline + slack (SOURCE still direct or a context
 * entry left): state unchanged, call again (only the cleanup is retried; P35);
 * FX1_CAMERA_EDEV. State 0 / 3 is entered only with the pipeline stopped:
 * ISP stopped and, direct, SOURCE memory with an empty context table. */
int fx1_camera_stop(fx1_camera* cam, uint32_t timeout_us);
/* ISR hooks (see the header comment for what each one does). */
void fx1_camera_csi_isr(fx1_camera* cam);
void fx1_camera_isp_isr(fx1_camera* cam);
/* Moves frames: CSI-2 sides -> ISP, ISP outputs -> ready list; handles ISP
 * DMA errors (fx1_isp_recover_ex) and losses. Returns the ready count. */
unsigned fx1_camera_poll(fx1_camera* cam);
/* Oldest completed frame, polling up to timeout_us: FX1_CAMERA_ETIMEOUT if none. */
int fx1_camera_dequeue(fx1_camera* cam, fx1_camera_frame* frame, uint32_t timeout_us);
/* Gives the frame's output slot back to the ISP (after stop: no-op, OK). */
int fx1_camera_release(fx1_camera* cam, const fx1_camera_frame* frame);
/* Repairs an output slot's addresses (after an ODMA error on a slot whose
 * address was wrong); takes effect for its next use. */
void fx1_camera_set_output(fx1_camera* cam, unsigned slot, uint32_t y, uint32_t uv);

#ifdef __cplusplus
}
#endif

#endif /* FX1_CAMERA_H */
