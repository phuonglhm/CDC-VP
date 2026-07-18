/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SAURIA NPU v4 register ABI. Offsets are relative to CDC_NPU0_BASE.
 * All accesses are aligned 32-bit little-endian words.
 */
#ifndef CDC_SOC_REGS_NPU_V4_H
#define CDC_SOC_REGS_NPU_V4_H

#define CDC_NPU_CTRL                0x0000u
#define CDC_NPU_STATUS              0x0004u
#define CDC_NPU_IRQ_ENABLE          0x0008u
#define CDC_NPU_IRQ_STATUS          0x000Cu
#define CDC_NPU_SRC_ADDR            0x0010u
#define CDC_NPU_DST_ADDR            0x0014u
#define CDC_NPU_SCRATCH_ADDR        0x0018u
#define CDC_NPU_SRC_SIZE_BYTES      0x001Cu
#define CDC_NPU_DST_SIZE_BYTES      0x0020u
#define CDC_NPU_WIDTH               0x0024u
#define CDC_NPU_HEIGHT              0x0028u
#define CDC_NPU_SRC_STRIDE_BYTES    0x002Cu
#define CDC_NPU_FORMAT              0x0030u
#define CDC_NPU_OP_MODE             0x0034u
#define CDC_NPU_WEIGHTS_ADDR        0x0038u
#define CDC_NPU_PARAM_ADDR          0x003Cu
#define CDC_NPU_WEIGHTS_SIZE_BYTES  0x0040u

#define CDC_NPU_K_DIMENSION         0x1000u
#define CDC_NPU_ZERO_THRESHOLD_FP32 0x1004u
#define CDC_NPU_ROWS_ACTIVE         0x1008u
#define CDC_NPU_DILATION_PATTERN    0x100Cu
#define CDC_NPU_CYCLE_COUNT         0x1010u
#define CDC_NPU_BYTES_READ          0x1014u
#define CDC_NPU_BYTES_WRITTEN       0x1018u
#define CDC_NPU_LAST_ERROR          0x101Cu
#define CDC_NPU_CORE_ID             0x1020u

#define CDC_NPU_CTRL_ENABLE         (1u << 0)
#define CDC_NPU_CTRL_START          (1u << 1)
#define CDC_NPU_CTRL_SOFT_RESET     (1u << 2)
#define CDC_NPU_CTRL_IRQ_EN         (1u << 3)

#define CDC_NPU_STATUS_BUSY         (1u << 0)
#define CDC_NPU_STATUS_DONE         (1u << 1)
#define CDC_NPU_STATUS_ERROR        (1u << 2)
#define CDC_NPU_STATUS_IDLE         (1u << 3)

#define CDC_NPU_IRQ_DONE            (1u << 0)
#define CDC_NPU_IRQ_ERROR           (1u << 1)

#define CDC_NPU_FORMAT_INT8_INT8_INT32 1u
#define CDC_NPU_OP_GEMM                0u
#define CDC_NPU_CORE_ID_VALUE          0x53415534u

#define CDC_NPU_ERROR_NONE               0u
#define CDC_NPU_ERROR_DISABLED           1u
#define CDC_NPU_ERROR_BUSY               2u
#define CDC_NPU_ERROR_INVALID_DIMENSIONS 3u
#define CDC_NPU_ERROR_INVALID_FORMAT     4u
#define CDC_NPU_ERROR_INVALID_OPERATION  5u
#define CDC_NPU_ERROR_INVALID_ADDRESS    6u
#define CDC_NPU_ERROR_INVALID_SIZE       7u
#define CDC_NPU_ERROR_DMA_READ           8u
#define CDC_NPU_ERROR_DMA_WRITE          9u
#define CDC_NPU_ERROR_CORE_DEADLOCK      10u
#define CDC_NPU_ERROR_CORE_TIMEOUT       11u
#define CDC_NPU_ERROR_RESET_ABORTED      12u

#endif /* CDC_SOC_REGS_NPU_V4_H */
