/* SPDX-License-Identifier: Apache-2.0
 *
 * FreeRTOS driver for the SAURIA NPU v4 (docs/NPU_V4_INTEGRATION_HANDOFF.md
 * section 13.7). Register/ABI definitions come exclusively from
 * fw/common/include/soc/regs/soc_regs_npu_v4.h - no duplicates here.
 *
 * Single hardware context: a mutex serializes callers; completion is
 * delivered by PLIC source 17 via a direct-to-task notification.
 */
#ifndef FREERTOS_FX1_NPU_V4_H
#define FREERTOS_FX1_NPU_V4_H

#include <stdint.h>

#include "FreeRTOS.h"

#define NPU_V4_OK            0
#define NPU_V4_ERR_PARAM    -1
#define NPU_V4_ERR_TIMEOUT  -2
#define NPU_V4_ERR_DEVICE   -3
#define NPU_V4_ERR_NOT_INIT -4

typedef struct {
    const int8_t *activations;      /* row-major A[32][k], physical RAM0 */
    const int8_t *weights;          /* row-major B[k][32], physical RAM0 */
    int32_t *output;                /* row-major C[32][32], physical RAM0 */
    uint32_t k;                     /* GEMM K dimension, 1..992 */
    uint32_t activation_stride;     /* bytes per A row; 0 means k */
} npu_gemm_job_t;

typedef struct {
    uint32_t cycle_count;
    uint32_t bytes_read;
    uint32_t bytes_written;
    uint32_t last_error;
} npu_v4_metrics_t;

int npu_v4_init(void);              /* registers PLIC source 17; call once before use */
int npu_v4_gemm(const npu_gemm_job_t *job, TickType_t timeout);
uint32_t npu_v4_last_status(void);  /* NPU STATUS captured in the last ISR */
void npu_v4_get_metrics(npu_v4_metrics_t *metrics);

#endif /* FREERTOS_FX1_NPU_V4_H */
