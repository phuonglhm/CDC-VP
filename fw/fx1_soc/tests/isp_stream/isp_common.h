/*
 * Shared by the ISP platform tests: the MMIO HAL of the reference C99 driver,
 * the fixture's RAW formula, the buffer layout and the NV12 CRC.
 */
#ifndef ISP_COMMON_H
#define ISP_COMMON_H

#include "fx1_fw.h"
#include "fx1_isp/fx1_isp_csr.h"
#include "fx1_isp/fx1_isp_drv.h"
#include "isp_fixture.h"

#define ISP_IN_STRIDE  (2u * ISP_FX_WIDTH) /* 16-bit containers; multiple of the 16-byte AXI beat */
#define ISP_OUT_STRIDE ISP_FX_WIDTH

/* DMA_ERR bits that mean a lost or corrupt frame. IDMA_UNDERRUN (a level while
 * no next input is valid) and ODMA_OVERFLOW (a stall on a busy output) are
 * lossless flow-control conditions (ISP guide, DEC-18): counted, not fatal. */
#define ISP_DMA_ERR_FATAL (FX1_ISP_DMA_ERR_ERR_ALIGN_OR_GEOMETRY_BIT | FX1_ISP_DMA_ERR_ERR_IDMA_AXI_BIT | \
                           FX1_ISP_DMA_ERR_ERR_ODMA_AXI_BIT)

extern const fx1_isp_hal isp_hal;

/* 4 inputs at FX1_FW_RAW_BUF_BASE, 4 Y/UV outputs at FX1_FW_YUV_BUF_BASE. */
void isp_buffers(fx1_isp_buffers* bufs);

/* ISP DMA burst (beats) of the phases that abort a frame while its input is
 * being read (isp_recovery C/D/E, isp_mas0710 A/B). Those checks need the CPU
 * to reach the abort point within one 64x48 frame (about 16 us of IDMA
 * activity). Since DDR3 subtask 5 the hart fetches every instruction from the
 * DDR3 controller (no cache in the VP), and the FX1 bus serializes all
 * MEMCTL_DDR traffic in FIFO order, so each CPU fetch waits for the ISP burst
 * ahead of it (an ECC write burst takes 100-200 ns, MAS companion read). With
 * 16-beat bursts the CPU needs about 18.5 us for the abort sequence; with 2
 * beats it needs about 12 us while the input read spans 31-37 us. The burst
 * length changes no data and no completion order (DDR3 decision log VPL-49).
 * Since CSR v1.07 the driver reads the recovery status, sequence and tuning
 * error before every reset request (rejected-request detection), which adds
 * about 20 us between the abort point and the reset write: with 2 beats the
 * input was fully read about 6 us before the reset landed. With 1 beat the
 * soft reset of isp_recovery C lands after 3952 of the 6144 input bytes,
 * about 30 us before the read would end (bus trace, V107-11). */
#define ISP_ABORT_MAX_BURST_BEATS 1u
/* Must match raw_sample() in gen_fixture.py exactly. */
uint32_t isp_raw_sample(uint32_t frame, uint32_t x, uint32_t y);
/* CPU writes fixture frame `frame` into the input buffer at `base`, then hands
 * the buffer to the device (fx1_dma_prepare TO_DEVICE, plan C12). */
void isp_fill_input(uint64_t base, uint32_t frame);
/* Hands an output buffer (both planes) to the device before the ODMA may write
 * it (fx1_dma_prepare FROM_DEVICE): all four before fx1_isp_start, and each one
 * again once the CPU has consumed it, since the driver may re-queue it. */
void isp_output_to_device(uint64_t y, uint64_t uv);
void isp_outputs_to_device(const fx1_isp_buffers* bufs);
/* Hands a completed output buffer back to the CPU (fx1_dma_complete
 * FROM_DEVICE on both planes) before the CPU reads it. */
void isp_output_to_cpu(uint64_t y, uint64_t uv);
/* CRC-32 (IEEE) of the active NV12 bytes, Y then UV. */
uint32_t isp_nv12_crc(uint64_t y, uint64_t uv);

#endif /* ISP_COMMON_H */
