/* SPDX-License-Identifier: Apache-2.0 */

#include "npu_v4.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "plic.h"
#include "soc/regs/soc_regs_npu_v4.h"
#include "soc/soc_irq_map.h"
#include "soc/soc_memory_map.h"

#define NPU_M 32u
#define NPU_N 32u
#define NPU_K_MAX 992u

/* Notification bits handed from the ISR to the waiting task. */
#define NPU_NOTIFY_DONE  (1u << 0)
#define NPU_NOTIFY_ERROR (1u << 1)

static inline void npu_write(uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)(CDC_NPU0_BASE + offset) = value;
}

static inline uint32_t npu_read(uint32_t offset)
{
    return *(volatile uint32_t *)(uintptr_t)(CDC_NPU0_BASE + offset);
}

static struct {
    SemaphoreHandle_t mutex;        /* serializes the single HW context */
    TaskHandle_t waiter;            /* task blocked on completion        */
    volatile uint32_t last_status;  /* STATUS captured in the ISR        */
    volatile uint32_t last_irq;     /* IRQ_STATUS captured in the ISR    */
    npu_v4_metrics_t metrics;        /* counters captured after each job  */
    int initialized;
} npu;

/* Called from the PLIC dispatcher for source 17. Clears the device cause
 * (W1C) before returning; the dispatcher completes the claim afterwards
 * (guardrail 5: never complete before the device level is deasserted). */
static void npu_v4_isr(uint32_t source, void *arg)
{
    (void)source;
    (void)arg;

    const uint32_t status = npu_read(CDC_NPU_STATUS);
    const uint32_t irq = npu_read(CDC_NPU_IRQ_STATUS);

    npu_write(CDC_NPU_IRQ_STATUS,
              irq & (CDC_NPU_IRQ_DONE | CDC_NPU_IRQ_ERROR));

    npu.last_status = status;
    npu.last_irq = irq;

    if (npu.waiter != NULL) {
        BaseType_t woken = pdFALSE;
        uint32_t bits = 0u;

        if ((irq & CDC_NPU_IRQ_DONE) != 0u) {
            bits |= NPU_NOTIFY_DONE;
        }
        if ((irq & CDC_NPU_IRQ_ERROR) != 0u ||
            (status & CDC_NPU_STATUS_ERROR) != 0u) {
            bits |= NPU_NOTIFY_ERROR;
        }
        xTaskNotifyFromISR(npu.waiter, bits, eSetBits, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

int npu_v4_init(void)
{
    if (npu.initialized) {
        return NPU_V4_OK;
    }

    if (npu_read(CDC_NPU_CORE_ID) != CDC_NPU_CORE_ID_VALUE) {
        return NPU_V4_ERR_DEVICE;
    }

    npu.mutex = xSemaphoreCreateMutex();
    if (npu.mutex == NULL) {
        return NPU_V4_ERR_NOT_INIT;
    }

    plic_register_handler(CDC_IRQ_NPU0, npu_v4_isr, NULL);
    plic_enable(CDC_IRQ_NPU0, 1u);

    npu.initialized = 1;
    return NPU_V4_OK;
}

uint32_t npu_v4_last_status(void)
{
    return npu.last_status;
}

void npu_v4_get_metrics(npu_v4_metrics_t *metrics)
{
    if (metrics != NULL) {
        *metrics = npu.metrics;
    }
}

int npu_v4_gemm(const npu_gemm_job_t *job, TickType_t timeout)
{
    if (!npu.initialized) {
        return NPU_V4_ERR_NOT_INIT;
    }
    if (job == NULL || job->activations == NULL || job->weights == NULL ||
        job->output == NULL || job->k == 0u || job->k > NPU_K_MAX) {
        return NPU_V4_ERR_PARAM;
    }

    const uint32_t stride =
        (job->activation_stride != 0u) ? job->activation_stride : job->k;
    int result = NPU_V4_OK;

    xSemaphoreTake(npu.mutex, portMAX_DELAY);

    npu.metrics.cycle_count = 0u;
    npu.metrics.bytes_read = 0u;
    npu.metrics.bytes_written = 0u;
    npu.metrics.last_error = 0u;

    /* Clear stale completion state before arming a new job. */
    npu_write(CDC_NPU_IRQ_STATUS, CDC_NPU_IRQ_DONE | CDC_NPU_IRQ_ERROR);
    npu_write(CDC_NPU_STATUS, CDC_NPU_STATUS_DONE | CDC_NPU_STATUS_ERROR);
    npu.waiter = xTaskGetCurrentTaskHandle();
    xTaskNotifyStateClear(NULL);
    ulTaskNotifyValueClear(NULL, 0xFFFFFFFFu);

    /* Same register order as the bare-metal reference
     * (fw/npu_v4_irq_riscv/src/main.c). Buffer pointers are physical RAM0
     * addresses - guardrail 7. */
    npu_write(CDC_NPU_SRC_ADDR, (uint32_t)(uintptr_t)job->activations);
    npu_write(CDC_NPU_SRC_SIZE_BYTES, NPU_M * job->k);
    npu_write(CDC_NPU_SRC_STRIDE_BYTES, stride);
    npu_write(CDC_NPU_WEIGHTS_ADDR, (uint32_t)(uintptr_t)job->weights);
    npu_write(CDC_NPU_WEIGHTS_SIZE_BYTES, job->k * NPU_N);
    npu_write(CDC_NPU_DST_ADDR, (uint32_t)(uintptr_t)job->output);
    npu_write(CDC_NPU_DST_SIZE_BYTES, NPU_M * NPU_N * (uint32_t)sizeof(int32_t));
    npu_write(CDC_NPU_WIDTH, NPU_N);
    npu_write(CDC_NPU_HEIGHT, NPU_M);
    npu_write(CDC_NPU_K_DIMENSION, job->k);
    npu_write(CDC_NPU_FORMAT, CDC_NPU_FORMAT_INT8_INT8_INT32);
    npu_write(CDC_NPU_OP_MODE, CDC_NPU_OP_GEMM);
    npu_write(CDC_NPU_IRQ_ENABLE, CDC_NPU_IRQ_DONE | CDC_NPU_IRQ_ERROR);
    npu_write(CDC_NPU_CTRL, CDC_NPU_CTRL_ENABLE | CDC_NPU_CTRL_IRQ_EN);

    __asm__ volatile("fence" ::: "memory");

    npu_write(CDC_NPU_CTRL,
              CDC_NPU_CTRL_ENABLE | CDC_NPU_CTRL_IRQ_EN | CDC_NPU_CTRL_START);

    uint32_t bits = 0u;

    if (xTaskNotifyWait(0u, 0xFFFFFFFFu, &bits, timeout) == pdFALSE) {
        result = NPU_V4_ERR_TIMEOUT;
    } else if ((bits & NPU_NOTIFY_ERROR) != 0u ||
               (bits & NPU_NOTIFY_DONE) == 0u) {
        result = NPU_V4_ERR_DEVICE;
    }

    /* Order DMA result and counter reads after observing the completion IRQ. */
    __asm__ volatile("fence" ::: "memory");
    npu.metrics.cycle_count = npu_read(CDC_NPU_CYCLE_COUNT);
    npu.metrics.bytes_read = npu_read(CDC_NPU_BYTES_READ);
    npu.metrics.bytes_written = npu_read(CDC_NPU_BYTES_WRITTEN);
    npu.metrics.last_error = npu_read(CDC_NPU_LAST_ERROR);

    npu.waiter = NULL;
    xSemaphoreGive(npu.mutex);
    return result;
}
