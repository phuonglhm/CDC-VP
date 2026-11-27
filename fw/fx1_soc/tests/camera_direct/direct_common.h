/*
 * Direct CSI-2 -> ISP firmware tests on fx1_soc --camera: shared code
 * (plan DIRECT_CSI_ISP D2a/D2b; contract
 * components/FX1_Components/mipi-csi2/docs/DIRECT_CSI_ISP_CONTRACT.md).
 *
 * Bare metal, polling, the C drivers directly. The sensor (CcsSensor through
 * the detailed D-PHY) or the CSI-2 RX test pattern generator (CAM_TPG=1)
 * streams RAW16 over the CSI's AXI4-Stream master into ISP_INGRESS
 * (SOURCE = direct, ISP IDMA off). The CSI RAW DMA is off unless a test turns
 * it on (direct + RAW dump, D2b).
 */
#ifndef CAM_DIRECT_COMMON_H
#define CAM_DIRECT_COMMON_H

#include <stdint.h>

#include "cam_isp.h"
#include "fx1_csi2/fx1_csi2_regs.h"
#include "fx1_csi2_hal_mmio.h"
#include "fx1_fw.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_ingress_drv.h"
#include "fx1_isp_hal_mmio.h"

#ifndef CAM_BITS
#define CAM_BITS 12
#endif
#ifndef CAM_TPG
#define CAM_TPG 0
#endif
/* Sensor frame period in 10 us lines (cam_isp.h: 300, 3 ms). A test whose
 * firmware checks more per frame than one blanking allows sets it longer; the
 * golden does not depend on it. */
#ifndef CAM_FRAME_LENGTH
#define CAM_FRAME_LENGTH CAM_ISP_FRAME_LENGTH
#endif
#define DIRECT_FRAME_US (CAM_FRAME_LENGTH * (CAM_ISP_FRAME_US / CAM_ISP_FRAME_LENGTH))

#if CAM_TPG
#define W CAM_ISP_TPG640_W
#define H CAM_ISP_TPG640_H
#define Y_BUF(i) (FX1_FW_YUV_BUF_BASE + (i) * 0x80000u)
#define UV_BUF(i) (Y_BUF(i) + 0x50000u)
#else
#define W CAM_ISP_W
#define H CAM_ISP_H
#define Y_BUF(i) CAM_ISP_Y(i)
#define UV_BUF(i) CAM_ISP_UV(i)
#endif

extern fx1_isp_ingress g_ing;

uint32_t direct_isp_rd(uint32_t off);
void direct_isp_wr(uint32_t off, uint32_t v);
uint64_t direct_ticks(uint32_t us);
void direct_spin_us(uint32_t us);

/* Probe, CSI-2 (sensor mode + D-PHY, or TPG), RAW16 stream on, ISP geometry /
 * depth / profile / ODMA, SOURCE = direct, `armed` output buffers armed. The
 * sensor is not started (direct_start). Codes 0x10..0x1B. */
void direct_setup(unsigned armed);
/* Sensor streaming (sensor) or the first TPG frame. Code 0x1C. */
void direct_start(void);
/* Waits for a READY context entry; 0 on timeout. */
int direct_wait_ctx(uint32_t us);
/* The ODMA buffer with DONE whose BUF_TAG is `tag` (publish gate, P26); -1 if none. */
int direct_find_done(uint32_t tag);
/* Consumed (DONE W1C) and given back to the ODMA (FREE W1S). */
void direct_arm(unsigned buf);
#if CAM_TPG
void direct_tpg_frame(void);
#endif
/* One published frame in ODMA buffer `buf`: context outcome / geometry, BUF_TAG,
 * BUF_TS = FRAME_ID, ISP frame ID, NV12 against the stand-alone model's golden.
 * Returns the sensor frame number (golden index; 1 for TPG). Codes code..code|6. */
uint32_t direct_check_frame(const fx1_isp_ingress_ctx* c, unsigned buf, uint32_t code);
/* After the sensor stopped: pops what is left until the ingress is IDLE and the
 * ISP not BUSY (budget `us`), reads the counters. */
void direct_drain(fx1_isp_ingress_counters* k, uint32_t us);
/* Logs a context entry that was not published. */
void direct_log_failed(const fx1_isp_ingress_ctx* c);

#endif /* CAM_DIRECT_COMMON_H */
