/*
 * FX1 MIPI CSI-2 RX reference driver, M2 scope (C99, bare-metal / RTOS style).
 *
 * Register access, delays and timing are supplied by the integrator through
 * fx1_csi2_hal, so the same code runs on the FX1 harts and on the SystemC
 * model (tests/test_csi2_driver.cpp). The driver owns no memory and is not
 * thread-safe: one hart owns the camera; the integrator serialises calls and
 * calls fx1_csi2_irq_handle() from the handler of the CSI-2 interrupt line.
 *
 * Every wait is bounded by a caller-supplied timeout: the delays requested
 * through the HAL by one call add up to at most timeout_us, also when the call
 * waits for several conditions in turn (one budget per call). Behaviour follows the
 * register contract (docs/REGISTER_CONTRACT.md) and the VP decisions of
 * docs/spec_decisions.md, which are pending IP confirmation. M4b adds the
 * RAW DMA, its ping-pong buffers and the stream path format; the camera
 * manager on top (buffer bookkeeping, ISP hand-off) belongs to M6.
 */
#ifndef FX1_CSI2_DRV_H
#define FX1_CSI2_DRV_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- return codes ------------------------------------------------------------ */
#define FX1_CSI2_OK 0
#define FX1_CSI2_EINVAL (-1)    /* argument outside the contract */
#define FX1_CSI2_ETIMEDOUT (-2) /* bounded wait expired; hardware state left as is */
#define FX1_CSI2_ENODEV (-3)    /* IP_VERSION does not match the supported revision */
#define FX1_CSI2_ENODATA (-4)   /* nothing to report (empty FIFO) */
#define FX1_CSI2_ESTATE (-5)    /* call not allowed in the current driver state */
#define FX1_CSI2_ENOCLK (-6)    /* soft reset done in the system domain only: the byte clock is not
                                 * running, Lane Management keeps its state until its next edge (C20);
                                 * use fx1_csi2_full_reset() if that state matters */

#define FX1_CSI2_SUPPORTED_VERSION 0x00010100u /* D01: workbook v1.4 revision */

typedef struct fx1_csi2_hal {
    uint32_t (*read32)(void *ctx, uint32_t offset);            /* offset from the CSI-2 CSR base */
    void (*write32)(void *ctx, uint32_t offset, uint32_t value);
    void (*delay_us)(void *ctx, uint32_t us);                  /* sleep or busy-wait >= us */
    void *ctx;
} fx1_csi2_hal;

typedef struct fx1_csi2_dev {
    fx1_csi2_hal hal;
    uint32_t magic;       /* set by fx1_csi2_probe */
    uint32_t version;     /* IP_VERSION */
    unsigned max_lanes;   /* PROTOCOL_CONFIG.max_lanes decoded: 1, 2 or 4 */
    unsigned lanes;       /* lane count of the running PHY, 0 when stopped */
    uint32_t poll_us;     /* polling interval of bounded waits (0 at probe: 10; 0 later: 1) */
} fx1_csi2_dev;

/* Snapshot of the status registers. */
typedef struct fx1_csi2_status {
    uint32_t core_status, phy_status, lane_status, buffer_status, lane_align_status;
    uint32_t packet_count, frame_count, frame_drop_count, crc_error_count;
    uint32_t ecc_error_count, lane_abort_count, irq_status;
} fx1_csi2_status;

/* Reads IP_VERSION and the built lane count. ENODEV on another revision. */
int fx1_csi2_probe(fx1_csi2_dev *dev, const fx1_csi2_hal *hal);

/* Resets; each returns once CORE_STATUS.reset_in_progress reads 0 (M17).
 * soft_reset: datapath, configuration kept. reset_in_progress covers the
 * byte-clock domain while the clock lane is in HS (C20); with the byte clock
 * stopped the call returns FX1_CSI2_ENOCLK (MAS: check cl_rxclkactivehs). */
int fx1_csi2_soft_reset(fx1_csi2_dev *dev, uint32_t timeout_us);
int fx1_csi2_full_reset(fx1_csi2_dev *dev, uint32_t timeout_us);  /* D-PHY + datapath + registers */
int fx1_csi2_phy_reset(fx1_csi2_dev *dev, uint32_t timeout_us);   /* D-PHY reset pulse only */

/* PHY bring-up (MAS initial sequence steps 2-6): D-PHY reset, active_lanes,
 * enables of the clock lane and data lanes 0..lanes-1, then wait until the
 * clock lane and every active lane are in Stop State. lanes = 1, 2 or 4,
 * <= max_lanes. The PHY calibrates on enable (~340 us in the VP). */
int fx1_csi2_phy_start(fx1_csi2_dev *dev, unsigned lanes, uint32_t timeout_us);
/* phy_enable = 0: PHY held in shutdown (P09). */
int fx1_csi2_phy_stop(fx1_csi2_dev *dev);

int fx1_csi2_core_enable(fx1_csi2_dev *dev, int enable);

/* Clears all pending flags, then enables the sources of `mask` and the global enable. */
int fx1_csi2_irq_enable(fx1_csi2_dev *dev, uint32_t mask);
int fx1_csi2_irq_disable(fx1_csi2_dev *dev);
/* Interrupt handler body: returns the enabled flags that were set and clears
 * exactly those (W1C). A flag raised again meanwhile stays set (C02). */
uint32_t fx1_csi2_irq_handle(fx1_csi2_dev *dev);

int fx1_csi2_get_status(fx1_csi2_dev *dev, fx1_csi2_status *st);
/* TIMESTAMP_LO then TIMESTAMP_HI (C06). */
int fx1_csi2_read_timestamp(fx1_csi2_dev *dev, uint64_t *ns);
/* Pops one generic short packet; ENODATA when the FIFO is empty. */
int fx1_csi2_gsp_pop(fx1_csi2_dev *dev, uint8_t *vc, uint8_t *dt, uint16_t *data);

/* ---- RAW DMA (M4b) ----------------------------------------------------------- */

typedef struct fx1_csi2_dma_cfg {
    uint32_t base[2];          /* ping-pong sides, multiples of 8 (M04) */
    uint32_t size;             /* bytes of one side (M05) */
    uint16_t bytes_per_line;   /* line as produced: RAW16 2W, RAW10 5W/4, RAW12 3W/2 (M03) */
    uint8_t line_padding;      /* bytes_per_line + line_padding: multiple of 8 (M02, M04) */
    uint8_t max_burst_len;     /* beats - 1 (D03); 0 gives single-beat bursts (M20): use 255 */
    uint8_t qos, prot;         /* AWQOS, AWPROT (not observable on the FX1 bus, M14) */
    uint8_t max_outstanding;   /* stored; the VP issues one write at a time (P03) */
    uint8_t raw16;             /* 1: RAW16 right-aligned (M01), 0: packed RAW10/RAW12 */
    uint8_t release_on_error;  /* err_frame_policy: 1 release with error, 0 discard and rewind (M07) */
} fx1_csi2_dma_cfg;

typedef enum fx1_csi2_buf_state {
    FX1_CSI2_BUF_HW_OWNED = 0,
    FX1_CSI2_BUF_SW_OWNED_VALID = 1,
    FX1_CSI2_BUF_SW_OWNED_ERROR = 2
} fx1_csi2_buf_state;

typedef struct fx1_csi2_buf_info {
    fx1_csi2_buf_state state;
    uint16_t frame_id;         /* FRAME_ID half of the side, written at its claim */
    uint16_t last_valid_line;  /* BUFFER_ERROR_INFO, 0xFFFF: no line complete (M08) */
} fx1_csi2_buf_info;

typedef struct fx1_csi2_dma_fault {
    uint8_t awid;              /* side of the failing write (M14) */
    uint8_t resp;              /* 2 = SLVERR, 3 = DECERR */
    uint32_t addr;
    uint16_t slverr_count, decerr_count;
} fx1_csi2_dma_fault;

/* Programs the DMA with the DMA path stopped and idle: ESTATE when
 * DMA_CONTROL.dma_enable is set or DMA_STATUS.dma_busy reads 1 (M09: the
 * format is changed only then). EINVAL for a layout the hardware would refuse
 * or fault on: misaligned base or stride, bytes_per_line 0, a side smaller than
 * one line, a side that wraps above 4 GiB. Leaves dma_enable = dma_start = 0. */
int fx1_csi2_dma_config(fx1_csi2_dev *dev, const fx1_csi2_dma_cfg *cfg);
/* dma_enable = dma_start = 1: captures that open from now on are claimed. */
int fx1_csi2_dma_start(fx1_csi2_dev *dev);
/* Graceful stop: dma_start = 0 (no new claim), wait until the capture in
 * flight has been finalised (dma_busy = 0), then dma_enable = 0. ETIMEDOUT
 * leaves dma_start = 0 with the DMA enabled: call fx1_csi2_dma_abort(). */
int fx1_csi2_dma_stop(fx1_csi2_dev *dev, uint32_t timeout_us);
/* Abort (M16): dma_abort = 1, wait until the drain and the finalisation are
 * done (dma_busy = 0 and reset_in_progress = 0), then dma_abort = dma_enable =
 * dma_start = 0. The side in flight ends per err_frame_policy. */
int fx1_csi2_dma_abort(fx1_csi2_dev *dev, uint32_t timeout_us);
/* Gives a SW-owned side back to the hardware. EINVAL side > 1; ESTATE when the
 * side is HW_OWNED (the release would have no effect, M11). */
int fx1_csi2_buffer_release(fx1_csi2_dev *dev, unsigned side);
int fx1_csi2_get_buffer(fx1_csi2_dev *dev, unsigned side, fx1_csi2_buf_info *info);
/* AXI_FAULT_TRACE / AXI_FAULT_ADDR (last fault) and AXI_ERROR_COUNT. */
int fx1_csi2_get_dma_fault(fx1_csi2_dev *dev, fx1_csi2_dma_fault *fault);
/* Stream path towards the ISP (D02: 0 in the DDR-only milestone). The format
 * is changed only while the path is disabled (MAS Block 8, M09): ESTATE,
 * nothing written, when it is enabled and raw16 differs from the current
 * format; from disabled, a new format is written before the enable. Whether
 * the stream pipeline has drained before a restart is the caller's flow. */
int fx1_csi2_stream_config(fx1_csi2_dev *dev, int enable, int raw16);

#ifdef __cplusplus
}
#endif
#endif /* FX1_CSI2_DRV_H */
