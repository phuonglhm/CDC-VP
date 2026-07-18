/* SPDX-License-Identifier: Apache-2.0
 *
 * FreeRTOS bring-up firmware for VP_FX1_Full_SoC (Bremen rv32).
 *
 * Demonstrates, in one deterministic run (UART0 markers in brackets):
 *   1. scheduler boot + preemptive multitasking      [FreeRTOS FX1 boot]
 *   2. CLINT MTIP tick driving vTaskDelay            [CLINT tick OK]
 *   3. PLIC claim/dispatch/complete with TIMER0      [PLIC TIMER0 OK]
 *   4. NPU GEMM from a task woken by PLIC IRQ17      [FreeRTOS NPU PASS]
 *   5. everything together                           [FreeRTOS FX1 PASS]
 *
 * Build with NPU=0 to drop step 4 on public (NPU-disabled) VP builds.
 */

#include "FreeRTOS.h"
#include "task.h"

#include "plic.h"
#include "uart.h"

#include "soc/soc_irq_map.h"
#include "soc/soc_memory_map.h"

#if defined(DEMO_NPU)
#include "npu_v4.h"
#endif

/* The port's CLINT addresses are plain literals (FreeRTOSConfig.h is included
 * by assembly); pin them against the SoC ABI here. */
_Static_assert(configMTIME_BASE_ADDRESS ==
                   (CDC_CLINT_BASE + CDC_CLINT_MTIME),
               "configMTIME_BASE_ADDRESS drifted from soc_memory_map.h");
_Static_assert(configMTIMECMP_BASE_ADDRESS ==
                   (CDC_CLINT_BASE + CDC_CLINT_MTIMECMP),
               "configMTIMECMP_BASE_ADDRESS drifted from soc_memory_map.h");

/* ---- TIMER0 (PrimeCell-style, components/timer_tlm) ------------------- */
#define TIMER0_CTRL      (*(volatile uint32_t *)(uintptr_t)(CDC_TIMER0_BASE + 0x00u))
#define TIMER0_RELOAD    (*(volatile uint32_t *)(uintptr_t)(CDC_TIMER0_BASE + 0x08u))
#define TIMER0_INTSTATUS (*(volatile uint32_t *)(uintptr_t)(CDC_TIMER0_BASE + 0x0Cu))
#define TIMER0_CTRL_ENABLE (1u << 0)
#define TIMER0_CTRL_INTEN  (1u << 3)
#define TIMER0_INT_CLEAR   (1u << 0)

/* Model tick is 20 ns/count: 5000 counts ~= 100 us per interrupt. */
#define TIMER0_RELOAD_COUNTS 5000u
#define TIMER0_IRQS_EXPECTED 3u

/* ---- demo progress flags (each written by exactly one task) ----------- */
static volatile int tick_ok;
static volatile int timer_ok;
static volatile int npu_ok;

/* =======================================================================
 * Milestone 3: PLIC path - TIMER0 periodic interrupt wakes a task.
 * ===================================================================== */

static TaskHandle_t timer_task_handle;

static void timer0_isr(uint32_t source, void *arg)
{
    (void)source;
    (void)arg;

    /* Level-IRQ policy: clear the device cause (W1C) before the dispatcher
     * completes the claim. The level may drop a delta cycle after the W1C,
     * so a re-claim can still see INTSTATUS == 0: treat that as spurious
     * and do not count it. */
    if ((TIMER0_INTSTATUS & 1u) != 0u) {
        TIMER0_INTSTATUS = TIMER0_INT_CLEAR;

        BaseType_t woken = pdFALSE;
        vTaskNotifyGiveFromISR(timer_task_handle, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

static void timer_demo_task(void *params)
{
    (void)params;

    uint32_t irqs = 0u;

    plic_register_handler(CDC_IRQ_TIMER0, timer0_isr, NULL);
    plic_enable(CDC_IRQ_TIMER0, 1u);

    TIMER0_RELOAD = TIMER0_RELOAD_COUNTS;   /* also loads VALUE */
    TIMER0_CTRL = TIMER0_CTRL_ENABLE | TIMER0_CTRL_INTEN;

    while (irqs < TIMER0_IRQS_EXPECTED) {
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) == 0u) {
            uart_put_u32("PLIC TIMER0 FAIL: timeout after ", irqs, " irqs\n");
            vTaskDelete(NULL);
        }
        ++irqs;
    }

    TIMER0_CTRL = 0u;
    TIMER0_INTSTATUS = TIMER0_INT_CLEAR;
    plic_disable(CDC_IRQ_TIMER0);

    uart_put_u32("PLIC TIMER0 OK irqs=", irqs, "\n");
    timer_ok = 1;
    vTaskDelete(NULL);
}

/* =======================================================================
 * Milestones 4/5: NPU GEMM submitted from a task, completion via IRQ17.
 * ===================================================================== */

#if defined(DEMO_NPU)

#define NPU_M 32u
#define NPU_N 32u
#define NPU_K 64u

static void npu_demo_task(void *params)
{
    (void)params;

    /* Fixed physical windows outside the FW/RTOS region (handoff 13.8). */
    volatile int8_t *const activations =
        (volatile int8_t *)(uintptr_t)CDC_VPU_OUT0_BASE;
    volatile int8_t *const weights =
        (volatile int8_t *)(uintptr_t)CDC_NPU_WGT0_BASE;
    volatile int32_t *const output =
        (volatile int32_t *)(uintptr_t)CDC_NPU_WORK0_BASE;

    int rc = npu_v4_init();

    if (rc != NPU_V4_OK) {
        uart_put_u32("FreeRTOS NPU FAIL: init rc=", (uint32_t)-rc, "\n");
        vTaskDelete(NULL);
    }

    /* Same deterministic pattern as the bare-metal reference test. */
    for (uint32_t row = 0; row < NPU_M; ++row) {
        const int8_t a = (int8_t)((row & 3u) + 1u);
        for (uint32_t k = 0; k < NPU_K; ++k) {
            activations[row * NPU_K + k] = a;
        }
    }
    for (uint32_t k = 0; k < NPU_K; ++k) {
        for (uint32_t col = 0; col < NPU_N; ++col) {
            weights[k * NPU_N + col] = (int8_t)((int32_t)(col & 3u) - 1);
        }
    }
    for (uint32_t i = 0; i < NPU_M * NPU_N; ++i) {
        output[i] = (int32_t)0x55555555;
    }

    const npu_gemm_job_t job = {
        .activations = (const int8_t *)(uintptr_t)CDC_VPU_OUT0_BASE,
        .weights = (const int8_t *)(uintptr_t)CDC_NPU_WGT0_BASE,
        .output = (int32_t *)(uintptr_t)CDC_NPU_WORK0_BASE,
        .k = NPU_K,
        .activation_stride = 0u,
    };

    rc = npu_v4_gemm(&job, pdMS_TO_TICKS(1000));
    if (rc != NPU_V4_OK) {
        uart_put_u32("FreeRTOS NPU FAIL: gemm rc=", (uint32_t)-rc, "\n");
        uart_put_hex32("  status=", npu_v4_last_status(), "\n");
        vTaskDelete(NULL);
    }

    uint32_t mismatch = 0u;

    for (uint32_t row = 0; row < NPU_M && mismatch == 0u; ++row) {
        for (uint32_t col = 0; col < NPU_N; ++col) {
            const int32_t expected =
                (int32_t)NPU_K * (int32_t)((row & 3u) + 1u) *
                ((int32_t)(col & 3u) - 1);
            if (output[row * NPU_N + col] != expected) {
                mismatch = row * NPU_N + col + 1u;
                break;
            }
        }
    }

    if (mismatch != 0u) {
        uart_put_u32("FreeRTOS NPU FAIL: mismatch at ", mismatch - 1u, "\n");
        vTaskDelete(NULL);
    }

    uart_puts("FreeRTOS NPU PASS\n");
    npu_ok = 1;
    vTaskDelete(NULL);
}

#endif /* DEMO_NPU */

/* =======================================================================
 * Milestones 1/2: scheduler + CLINT tick, then overall verdict.
 * ===================================================================== */

static void console_task(void *params)
{
    (void)params;

    /* Prove the CLINT tick advances real (simulated) time: three 100 ms
     * delays must advance the tick count accordingly. */
    for (int i = 1; i <= 3; ++i) {
        const TickType_t before = xTaskGetTickCount();
        vTaskDelay(pdMS_TO_TICKS(100));
        const TickType_t after = xTaskGetTickCount();

        if ((after - before) < pdMS_TO_TICKS(100)) {
            uart_puts("CLINT tick FAIL\n");
            vTaskDelete(NULL);
        }
        uart_put_u32("FX1 tick=", (uint32_t)after, "\n");
    }
    uart_puts("CLINT tick OK\n");
    tick_ok = 1;

    /* Wait (bounded) for the other demos, then emit the overall verdict. */
    for (int waited_ms = 0; waited_ms < 3000; waited_ms += 100) {
        if (tick_ok && timer_ok && npu_ok) {
            uart_puts("FreeRTOS FX1 PASS\n");
            vTaskDelete(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    uart_puts("FreeRTOS FX1 FAIL: incomplete (");
    uart_put_u32("tick=", (uint32_t)tick_ok, " ");
    uart_put_u32("timer=", (uint32_t)timer_ok, " ");
    uart_put_u32("npu=", (uint32_t)npu_ok, ")\n");
    vTaskDelete(NULL);
}

int main(void)
{
    uart_puts("FreeRTOS FX1 boot\n");

    plic_init();
    uart_init();

#if !defined(DEMO_NPU)
    npu_ok = 1; /* NPU demo compiled out (public build) */
#endif

    BaseType_t ok;

    ok = xTaskCreate(console_task, "console", 384, NULL, 1, NULL);
    configASSERT(ok == pdPASS);
    ok = xTaskCreate(timer_demo_task, "timer0", 384, NULL, 2,
                     &timer_task_handle);
    configASSERT(ok == pdPASS);
#if defined(DEMO_NPU)
    ok = xTaskCreate(npu_demo_task, "npu", 512, NULL, 2, NULL);
    configASSERT(ok == pdPASS);
#endif

    vTaskStartScheduler();

    /* Only reached on allocation failure. */
    uart_puts("FreeRTOS FX1 FAIL: scheduler did not start\n");
    for (;;) {
    }
    return 0;
}

/* ---- kernel hooks ----------------------------------------------------- */

void vApplicationIdleHook(void)
{
    /* Let the ISS sleep to the next timed event instead of spinning; this is
     * what keeps host-time cost low on the no-DMI loosely-timed bus. */
    __asm__ volatile("wfi");
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;

    portDISABLE_INTERRUPTS();
    uart_puts2("FATAL: stack overflow in ", name);
    uart_puts("\n");
    for (;;) {
    }
}

void vApplicationMallocFailedHook(void)
{
    portDISABLE_INTERRUPTS();
    uart_puts("FATAL: kernel malloc failed\n");
    for (;;) {
    }
}

void vAssertCalled(const char *file, int line)
{
    portDISABLE_INTERRUPTS();
    uart_puts2("FATAL: configASSERT ", file);
    uart_put_u32(":", (uint32_t)line, "\n");
    for (;;) {
    }
}
