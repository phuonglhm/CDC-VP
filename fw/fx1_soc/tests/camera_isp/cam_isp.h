/*
 * CSI-2 M6a: camera -> ISP tests on fx1_soc --camera, shared code. The camera
 * manager (fw/fx1_soc/drivers/fx1_camera) moves the sensor's frames through
 * the CSI-2 RX DMA into the ISP; every NV12 output is checked against the
 * golden of the stand-alone ISP model (camera_isp_golden.h, gen_golden.py).
 */
#ifndef CAM_ISP_TEST_H
#define CAM_ISP_TEST_H

#include <stdint.h>

#include "camera_isp_golden.h"
#include "fx1_camera.h"
#include "fx1_fw.h"

/* Sensor timing as in the M5b camera smokes: 10 us lines, 3 ms frame period. */
#define CAM_ISP_LINE_LENGTH 2000u
#define CAM_ISP_FRAME_LENGTH 300u
#define CAM_ISP_FRAME_US 3000u
#define CAM_ISP_WAIT_US (4u * CAM_ISP_FRAME_US)

/* Buffers: CSI-2 sides and RAW10 staging in the RAW region, NV12 slots in the
 * YUV region (16-byte aligned). */
#define CAM_ISP_SIDE(i) (FX1_FW_RAW_BUF_BASE + (i) * 0x10000u)
#define CAM_ISP_STAGING(i) (FX1_FW_RAW_BUF_BASE + 0x40000u + (i) * 0x10000u)
#define CAM_ISP_Y(i) (FX1_FW_YUV_BUF_BASE + (i) * 0x10000u)
#define CAM_ISP_UV(i) (CAM_ISP_Y(i) + 0x8000u)

extern fx1_camera g_cam;

/* Trap handler (PLIC 7 -> CSI-2, PLIC 3 -> ISP) and PLIC set-up; MIE on. */
void cam_isp_irqs(void);
/* Configuration of a 64x16 sensor stream at `bits` (RAW12 4 lanes, RAW10 2
 * lanes), the matching golden profile. */
fx1_camera_cfg cam_isp_cfg(uint32_t bits);
/* CRC-32 (IEEE) of the active NV12 bytes of a frame, Y then UV. */
uint32_t cam_isp_crc(const fx1_camera_frame* f, uint32_t y_stride, uint32_t uv_stride);
/* Frame number whose golden CRC equals crc (1..CAM_ISP_FRAMES), 0 if none. */
uint32_t cam_isp_frame_of(uint32_t bits, uint32_t crc);
/* Dequeues one frame, checks its NV12 against the golden, its geometry and
 * metadata (zero-copy for RAW12, staging for RAW10), releases it. Returns its
 * sensor frame number. */
uint32_t cam_isp_take(uint32_t bits, uint32_t code);

#endif /* CAM_ISP_TEST_H */
