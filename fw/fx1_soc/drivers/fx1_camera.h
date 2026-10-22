/*
 * FX1 camera manager (CSI-2 M6a): camera sensor -> CSI-2 RX DMA -> ISP -> NV12,
 * on top of the hardware drivers (fx1_csi2_drv, fx1_isp_drv, fx1_ccs_sensor,
 * fx1_i2c), which it does not change. Bare metal, one hart.
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

#ifdef __cplusplus
extern "C" {
#endif

#define FX1_CAMERA_OK        0
#define FX1_CAMERA_EINVAL   -20
#define FX1_CAMERA_ESTATE   -21
#define FX1_CAMERA_ETIMEOUT -22
#define FX1_CAMERA_EDEV     -23 /* a hardware driver failed (sensor, CSI-2, ISP) */
#define FX1_CAMERA_ESENSOR  -24 /* stop: the sensor did not confirm standby (state 3) */

/* fx1_camera_stop returns within timeout_us + FX1_CAMERA_STOP_SLACK_US (200 µs
 * for the whole call): a sensor transfer started before the deadline ends
 * within its budget (what is left, at most the sensor's own timeout) +
 * FX1_I2C_ABORT_US (its abort; the idle wait before it is inside the budget);
 * the CSI-2 DMA abort gets FX1_CAMERA_STOP_ABORT_US after the deadline (the
 * safe drain is not skipped); the ISP reset and register accesses take the
 * rest (70 µs). */
#define FX1_CAMERA_STOP_ABORT_US 30u
#define FX1_CAMERA_STOP_SLACK_US 200u

typedef struct fx1_camera_cfg {
    /* image */
    uint32_t width, height;            /* even; width a multiple of 8 (16-byte RAW16 lines) */
    uint32_t bits;                     /* 10 or 12 */
    /* source: the sensor over I2C0, or the CSI-2 RX test pattern generator */
    int tpg;                           /* 1: TPG, no sensor, one frame per fx1_camera_tpg_frame() */
    uint32_t lanes;                    /* sensor: 1, 2, 4 */
    uint16_t line_length, frame_length;/* sensor timing (CCS) */
    /* buffers (16-byte aligned) */
    uint32_t csi_side[2], csi_side_size;
    uint32_t staging[FX1_ISP_NUM_BUFFERS]; /* RAW10: ISP inputs; unused with RAW12 */
    uint32_t y[FX1_ISP_NUM_BUFFERS], uv[FX1_ISP_NUM_BUFFERS];
    uint32_t y_stride, uv_stride;      /* >= ISP output width, multiples of 16 */
    /* ISP profile (CSR writes), applied before the ISP starts */
    const fx1_isp_reg_write* profile;
    size_t profile_len;
    /* Optional: called for every valid CSI-2 side before it goes to the ISP
     * (address of the RAW16 image, its capture number and CSI FRAME_ID), e.g.
     * to keep a copy of the original RAW. Runs in fx1_camera_poll. */
    void (*capture_hook)(void* ctx, uint32_t raw16, uint32_t capture, uint16_t csi_frame_id);
    /* Optional: called on an ISP DMA error (fatal DMA_ERR bits) before the
     * manager recovers (fx1_isp_recover_ex frees / re-arms the same buffer),
     * so the application can repair the cause, e.g. fx1_camera_set_output. */
    void (*isp_error_hook)(void* ctx, uint32_t dma_err);
    void* hook_ctx;
} fx1_camera_cfg;

/* A completed NV12 frame and where it came from. */
typedef struct fx1_camera_frame {
    uint32_t capture;      /* manager sequence of the CSI-2 capture, from 1 */
    unsigned csi_side;
    uint16_t csi_frame_id; /* FRAME_ID of the side at its claim */
    uint32_t isp_seq;      /* ISP driver sequence */
    unsigned isp_input, isp_output;
    int zero_copy;         /* 1: the ISP read the CSI-2 side itself */
    uint32_t isp_in;       /* address the ISP read (CSI side or staging) */
    uint32_t y, uv, width, height;
} fx1_camera_frame;

typedef struct fx1_camera_stats {
    uint32_t csi_ok, csi_err;          /* sides completed valid / with error (error: released, not sent) */
    uint32_t csi_irqs, isp_irqs;
    uint32_t isp_queued, isp_done, isp_lost, isp_errors;
    uint32_t held_for_isp;             /* polls that found a valid side waiting for the ISP */
    uint32_t timeouts;
    uint32_t stop_cancelled;           /* frames in the pipeline dropped by stop (waiting, in the ISP, not dequeued) */
    uint32_t sensor_errors;            /* standby not confirmed by the sensor */
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
} fx1_camera;

/* Probes the CSI-2 RX and the ISP, and the sensor unless cfg->tpg. */
int fx1_camera_probe(fx1_camera* cam, const fx1_csi2_hal* csi_hal, const fx1_isp_hal* isp_hal, int tpg);
/* Sensor mode (standby), D-PHY start, CSI-2 DMA (RAW16, both sides), CSI-2
 * interrupts, ISP geometry (RGGB), profile, buffers, ISP start. */
int fx1_camera_configure(fx1_camera* cam, const fx1_camera_cfg* cfg);
/* DMA start and sensor stream on (TPG: DMA start only). On an error the
 * manager stays configured (state 1), the CSI-2 DMA possibly started:
 * fx1_camera_stop cleans up (M6b review F1). */
int fx1_camera_start(fx1_camera* cam);
/* TPG: exactly one frame. */
int fx1_camera_tpg_frame(fx1_camera* cam);
/* Stop within timeout_us + FX1_CAMERA_STOP_SLACK_US, from state 1
 * (configured, or a start that failed half-way), 2, or 3 (see the header
 * comment): FX1_CAMERA_OK, state 0;
 * FX1_CAMERA_ESENSOR, state 3 (CSI-2 RX and ISP stopped, the sensor maybe
 * still streaming; call again to retry the standby); FX1_CAMERA_ETIMEOUT, the
 * CSI-2 DMA did not stop, state unchanged (1 or 2; call again); FX1_CAMERA_EDEV. */
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
