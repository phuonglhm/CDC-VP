/* SPDX-License-Identifier: Apache-2.0
 *
 * Steps 12.1 to 12.8: staged FreeRTOS proof for platforms/noc_soc.
 *
 * Keep this image deliberately smaller than the inherited FX1 demonstration:
 * no optional accelerator or TFLM. Two tasks at different
 * priorities exchange direct notifications for a bounded number of rounds.
 * Step 12.2 adds an independent CLINT delay/wake proof and TIMER0 interrupt
 * proof through PLIC source 4. Step 12.3 then adds a sequenced DMA transfer
 * completed through PLIC source 7. Step 12.4 keeps every earlier proof and
 * then runs them *together*: two CPU tasks driving RAM and MMIO, a free
 * running TIMER0 through the PLIC, and repeated DMA transfers, all overlapped
 * and bounded. Step 12.8 retains that signed workload and releases an
 * interrupt-driven UART0 CLI only after the complete bring-up reports PASS.
 * Optional accelerators remain out.
 *
 * Every stage is a compile level, so each earlier step stays independently
 * reproducible from the same source.
 */

#include "FreeRTOS.h"
#include "task.h"

#include "plic.h"
#include "uart.h"

#include "soc/soc_irq_map.h"
#include "soc/soc_memory_map.h"
#include "soc/regs/soc_regs_dma.h"

#include <stdint.h>

#ifndef NOC_FREERTOS_STEP
#define NOC_FREERTOS_STEP 123
#endif

#if NOC_FREERTOS_STEP == 128
#include "cli.h"
#endif

#if NOC_FREERTOS_STEP != 121 && NOC_FREERTOS_STEP != 122 && \
    NOC_FREERTOS_STEP != 123 && NOC_FREERTOS_STEP != 124 && \
    NOC_FREERTOS_STEP != 125 && NOC_FREERTOS_STEP != 128
#error "NOC_FREERTOS_STEP must be 121, 122, 123, 124, 125 or 128"
#endif

/*
 * Level 125 is level 124 plus RTOS tick instrumentation, and exists only so
 * the Step 12.7 measurement baseline can report tick jitter. It is kept out of
 * 124 deliberately: the hook runs on every tick and reads CLINT `mtime` over
 * the NoC, so folding it into the signed acceptance image would change the
 * very counters that image is signed on.
 */
#if NOC_FREERTOS_STEP == 125 && configUSE_TICK_HOOK != 1
#error "level 125 needs -DconfigUSE_TICK_HOOK=1"
#endif

#define NOC_SWITCH_ROUNDS 8u
#define NOC_LOW_PRIORITY  1u
#define NOC_HIGH_PRIORITY 2u
#define NOC_TIMER_PRIORITY 3u
#define NOC_TASK_STACK_WORDS 384u

_Static_assert(configMTIME_BASE_ADDRESS ==
                   (CDC_CLINT_BASE + CDC_CLINT_MTIME),
               "CLINT mtime address drift");
_Static_assert(configMTIMECMP_BASE_ADDRESS ==
                   (CDC_CLINT_BASE + CDC_CLINT_MTIMECMP),
               "CLINT mtimecmp address drift");
_Static_assert(CDC_IRQ_TIMER0 == 4u, "TIMER0 must remain PLIC source 4");
_Static_assert(CDC_IRQ_DMA0 == 7u, "DMA0 completion must remain PLIC source 7");
_Static_assert(CDC_IRQ_DMA0_ABORT == 8u,
               "DMA0 abort must remain PLIC source 8");

typedef enum {
    TURN_BOOT = 0,
    TURN_LOW_COMPLETE,
    TURN_HIGH_COMPLETE
} turn_state_t;

static TaskHandle_t low_task_handle;
static TaskHandle_t high_task_handle;
static volatile uint32_t low_rounds;
static volatile uint32_t high_rounds;
static volatile turn_state_t turn_state;
static volatile int scheduler_ok;

#if NOC_FREERTOS_STEP == 128
static TaskHandle_t cli_task_handle;
#endif

#if NOC_FREERTOS_STEP >= 122

#define TIMER0_CTRL \
    (*(volatile uint32_t *)(uintptr_t)(CDC_TIMER0_BASE + 0x00u))
#define TIMER0_RELOAD \
    (*(volatile uint32_t *)(uintptr_t)(CDC_TIMER0_BASE + 0x08u))
#define TIMER0_INTSTATUS \
    (*(volatile uint32_t *)(uintptr_t)(CDC_TIMER0_BASE + 0x0Cu))
#define TIMER0_CTRL_ENABLE (1u << 0)
#define TIMER0_CTRL_INTEN  (1u << 3)
#define TIMER0_INT_CLEAR   (1u << 0)
#define TIMER0_RELOAD_COUNTS 5000u
#define TIMER0_IRQS_EXPECTED 3u
#define CLINT_WAKE_ROUNDS 3u

static TaskHandle_t timer_task_handle;
static volatile uint32_t timer_irq_count;
static volatile int timer_ok;
static volatile int tick_ok;

#endif

#if NOC_FREERTOS_STEP >= 123

#define DMA_EVENT_DONE 3u
#define DMA_EVENT_MASK (1u << DMA_EVENT_DONE)
#define DMA_COPY_BYTES 32u
#define DMA_PROGRAM_BYTES 32u
#define DMA_PROGRAM_USED_BYTES 25u

static TaskHandle_t dma_task_handle;
static volatile uint32_t dma_irq_count;
static volatile uint8_t dma_program[DMA_PROGRAM_BYTES]
    __attribute__((aligned(16)));
static volatile uint8_t dma_source[DMA_COPY_BYTES]
    __attribute__((aligned(16)));
static volatile uint8_t dma_destination[DMA_COPY_BYTES]
    __attribute__((aligned(16)));

_Static_assert(DMA_PROGRAM_USED_BYTES <= DMA_PROGRAM_BYTES,
               "DMA channel program does not fit its buffer");

extern uint8_t __firmware_ram_end[];

#endif

#if NOC_FREERTOS_STEP >= 124

/*
 * Step 12.4 concurrent phase.
 *
 * The phase is bounded twice over. Each checkpoint is a `vTaskDelay`, so the
 * supervisor cannot spin, and the checkpoint count itself has a hard limit; a
 * phase that never reaches its completion condition fails with a named reason
 * instead of hanging. Every blocking wait below carries its own timeout.
 *
 * UART is deliberately not a synchronisation mechanism: the workers and the
 * DMA task print nothing while the phase runs, so no participant's rate can be
 * set by console back-pressure. Only the supervisor prints, and only from its
 * own delay loop.
 */

#define NOC_WORK_WORDS 64u
#define NOC_CONCURRENT_DMA_TRANSFERS 4u

/*
 * The Step 12.3 transfer is 32 bytes and completes in well under a microsecond
 * of modeled time - shorter than a single CPU-task loop iteration, so nothing
 * can be shown to run while it is in flight. The concurrent phase therefore
 * uses its own, larger transfer built from a DMALP loop over 16-byte bursts,
 * which gives a window the lower-priority CPU tasks demonstrably execute in.
 * The Step 12.3 objects are left untouched so that acceptance stays exactly as
 * it was signed off.
 */
#define DMA_CONCURRENT_BURST_BYTES 16u   /* CCR: 4 beats of 4 bytes */
#define DMA_CONCURRENT_BURSTS 256u
#define DMA_CONCURRENT_BYTES \
    (DMA_CONCURRENT_BURSTS * DMA_CONCURRENT_BURST_BYTES)
#define DMA_CONCURRENT_WORDS (DMA_CONCURRENT_BYTES / 4u)
#define DMA_CONCURRENT_PROGRAM_BYTES 32u
#define DMA_CONCURRENT_PROGRAM_USED_BYTES 27u

#define NOC_CONCURRENT_CHECKPOINTS 4u
#define NOC_CHECKPOINT_MS 5u
#define NOC_CHECKPOINT_LIMIT 60u          /* 300 ms of modeled phase time */
#define NOC_CONCURRENT_TIMER_IRQS 20u     /* floor, not an expected count */
#define NOC_SUPERVISOR_PRIORITY 2u
#define NOC_WORKER_PRIORITY 1u

/*
 * TIMER0 runs at 5000 counts of a 10 ns tick during Step 12.2, i.e. one
 * interrupt every 50 us. Left at that rate the concurrent phase is interrupt
 * saturated: an ISR entry plus PLIC claim/complete plus a context switch costs
 * a comparable amount of modeled time, so the CPU tasks barely advance and the
 * phase measures trap handling rather than concurrency. Step 12.4 therefore
 * slows TIMER0 to 200 us for the phase. It stays a real periodic interrupt
 * load; it stops being the only load.
 */
#define TIMER0_CONCURRENT_RELOAD_COUNTS 20000u

/* Which NoC-crossing MMIO register a worker reads once per iteration. */
#define NOC_WORKER_MMIO_CLINT 0
#define NOC_WORKER_MMIO_PLIC  1

/*
 * How many words a worker processes before yielding. One yield per full
 * iteration is too coarse: a DMA transfer window is far shorter than a
 * scheduler time slice, so with coarse yielding only whichever worker happened
 * to be running would ever be observed inside a window, and the other one's
 * overlap count would sit at zero or one. Yielding inside the loop makes both
 * workers present in every window, which is what the overlap check is meant to
 * measure.
 */
#define NOC_WORKER_YIELD_WORDS 8u

_Static_assert(DMA_CONCURRENT_BURSTS >= 1u && DMA_CONCURRENT_BURSTS <= 256u,
               "DMALP loop counter LC0 is 8 bits");
_Static_assert(DMA_CONCURRENT_PROGRAM_USED_BYTES <=
                   DMA_CONCURRENT_PROGRAM_BYTES,
               "concurrent DMA channel program does not fit its buffer");
_Static_assert(NOC_WORK_WORDS % NOC_WORKER_YIELD_WORDS == 0u,
               "the worker yield stride must divide the buffer");

static TaskHandle_t supervisor_task_handle;
static TaskHandle_t worker_a_task_handle;
static TaskHandle_t worker_b_task_handle;

static volatile uint32_t worker_a_iterations;
static volatile uint32_t worker_b_iterations;
/* Word granularity is the progress unit everywhere a bounded interval is
 * involved: a worker yields inside its loop, so a whole iteration can outlast
 * both a checkpoint interval and a DMA transfer window. */
static volatile uint32_t worker_a_words;
static volatile uint32_t worker_b_words;
static volatile uint32_t dma_concurrent_transfers;
static volatile uint32_t dma_concurrent_bytes;
static volatile int worker_a_done;
static volatile int worker_b_done;
static volatile int dma_concurrent_ok;
static volatile int timer_teardown_done;
static volatile int phase_stop;

/* Overlap evidence: how much each other participant advanced while the DMA
 * task was running its concurrent transfers. Measured by the DMA task itself,
 * so it cannot be satisfied by activity from before or after that window. */
static volatile uint32_t overlap_worker_a;
static volatile uint32_t overlap_worker_b;
static volatile uint32_t overlap_timer_irqs;

/* `volatile` on purpose: the point of the loop is to re-read RAM through the
 * NoC every iteration. Without it the compiler may forward the value it just
 * stored and the corruption check becomes a tautology. */
static volatile uint32_t work_a[NOC_WORK_WORDS];
static volatile uint32_t work_b[NOC_WORK_WORDS];

static volatile uint8_t dma_concurrent_program[DMA_CONCURRENT_PROGRAM_BYTES]
    __attribute__((aligned(16)));
static volatile uint32_t dma_concurrent_source[DMA_CONCURRENT_WORDS]
    __attribute__((aligned(16)));
static volatile uint32_t dma_concurrent_destination[DMA_CONCURRENT_WORDS]
    __attribute__((aligned(16)));

static uint32_t dma_concurrent_word(uint32_t transfer, uint32_t index)
{
    return (transfer * 0x01000193u) ^ (index * 0x9E3779B9u) ^ 0xA5A5A5A5u;
}

static uint32_t plic_reg_read(uint32_t offset)
{
    return *(volatile uint32_t *)(uintptr_t)(CDC_PLIC_BASE + offset);
}

static void plic_reg_write(uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)(CDC_PLIC_BASE + offset) = value;
}

static uint32_t clint_mtime_low(void)
{
    return *(volatile uint32_t *)(uintptr_t)(CDC_CLINT_BASE + CDC_CLINT_MTIME);
}

#endif

__attribute__((noreturn)) static void noc_fail(const char *reason)
{
    portDISABLE_INTERRUPTS();
    uart_puts2("FreeRTOS NoC FAIL: ", reason);
    uart_puts("\n");
    for (;;) {
        __asm__ volatile("wfi");
    }
}

#if NOC_FREERTOS_STEP == 125

/*
 * Tick jitter is a **platform/RTOS metric, not a NoC metric**. CLINT drives
 * MTIP straight into the CPU, so the interval measured here is dominated by
 * trap entry, the port's tick handler and whatever else held interrupts off.
 * The NoC appears in it only through the `mtime` read below and through the
 * memory traffic of the code being interrupted. Do not quote it as an
 * interconnect number.
 */
static volatile uint32_t tick_samples;
static volatile uint32_t tick_interval_min_us = 0xFFFFFFFFu;
static volatile uint32_t tick_interval_max_us;
static volatile uint32_t tick_deviation_max_us;
static volatile uint32_t tick_previous_us;

/* The configured period, in the microsecond units CLINT `mtime` counts in. */
#define NOC_TICK_PERIOD_US (1000000u / configTICK_RATE_HZ)

/* Discard the first few intervals. The first one is measured from a
 * zero-initialised timestamp, and the scheduler is still starting the earliest
 * tasks, so counting them would report startup as jitter. */
#define NOC_TICK_WARMUP 3u

void vApplicationTickHook(void)
{
    const uint32_t now =
        *(volatile uint32_t *)(uintptr_t)(CDC_CLINT_BASE + CDC_CLINT_MTIME);

    if (tick_previous_us != 0u) {
        const uint32_t interval = now - tick_previous_us;

        ++tick_samples;
        if (tick_samples > NOC_TICK_WARMUP) {
            const uint32_t deviation =
                (interval > NOC_TICK_PERIOD_US)
                    ? (interval - NOC_TICK_PERIOD_US)
                    : (NOC_TICK_PERIOD_US - interval);

            if (interval < tick_interval_min_us) {
                tick_interval_min_us = interval;
            }
            if (interval > tick_interval_max_us) {
                tick_interval_max_us = interval;
            }
            if (deviation > tick_deviation_max_us) {
                tick_deviation_max_us = deviation;
            }
        }
    }
    tick_previous_us = now;
}

static void report_tick_jitter(void)
{
    if (tick_samples <= NOC_TICK_WARMUP) {
        uart_puts("RTOS tick FAIL: too few tick samples\n");
        noc_fail("tick jitter sample count");
    }
    uart_put_u32("RTOS tick period us=", NOC_TICK_PERIOD_US, "\n");
    uart_put_u32("RTOS tick samples=",
                 tick_samples - NOC_TICK_WARMUP, "\n");
    uart_put_u32("RTOS tick interval min us=", tick_interval_min_us, "\n");
    uart_put_u32("RTOS tick interval max us=", tick_interval_max_us, "\n");
    uart_put_u32("RTOS tick deviation max us=", tick_deviation_max_us, "\n");
}

#endif

static void high_priority_task(void *params)
{
    (void)params;

    if (xTaskGetCurrentTaskHandle() != high_task_handle ||
        uxTaskPriorityGet(NULL) != NOC_HIGH_PRIORITY) {
        noc_fail("high-task context or priority");
    }

    for (uint32_t round = 1u; round <= NOC_SWITCH_ROUNDS; ++round) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) != 1u) {
            noc_fail("high-task notification count");
        }
        if (turn_state != TURN_LOW_COMPLETE ||
            low_rounds != round ||
            high_rounds != (round - 1u)) {
            noc_fail("high-task handoff order");
        }

        high_rounds = round;
        turn_state = TURN_HIGH_COMPLETE;
        uart_put_u32("NoC high round=", round, "\n");
        xTaskNotifyGive(low_task_handle);
    }

    vTaskDelete(NULL);
}

#if NOC_FREERTOS_STEP >= 122

static void timer0_isr(uint32_t source, void *arg)
{
    (void)source;
    (void)arg;

    /*
     * TIMER0 is level-sensitive. Clear the device cause before the common
     * PLIC dispatcher completes the claim. A delta-cycle stale claim with
     * INTSTATUS already clear is ignored rather than double-counted.
     */
    if ((TIMER0_INTSTATUS & 1u) != 0u) {
        BaseType_t woken = pdFALSE;

        TIMER0_INTSTATUS = TIMER0_INT_CLEAR;
        ++timer_irq_count;
        vTaskNotifyGiveFromISR(timer_task_handle, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

static void timer_interrupt_task(void *params)
{
    (void)params;

    if (uxTaskPriorityGet(NULL) != NOC_TIMER_PRIORITY) {
        noc_fail("TIMER0 task priority");
    }
    if (plic_register_handler(CDC_IRQ_TIMER0, timer0_isr, NULL) != 0) {
        noc_fail("TIMER0 PLIC handler registration");
    }

    plic_enable(CDC_IRQ_TIMER0, 1u);
    TIMER0_RELOAD = TIMER0_RELOAD_COUNTS;
    TIMER0_CTRL = TIMER0_CTRL_ENABLE | TIMER0_CTRL_INTEN;

    uint32_t received = 0u;
    while (received < TIMER0_IRQS_EXPECTED) {
        if (ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(100)) == 0u) {
            TIMER0_CTRL = 0u;
            uart_put_u32("PLIC TIMER0 FAIL: timeout after ",
                         received, " irqs\n");
            noc_fail("TIMER0 interrupt timeout");
        }
        ++received;
    }

#if NOC_FREERTOS_STEP < 124
    TIMER0_CTRL = 0u;
    TIMER0_INTSTATUS = TIMER0_INT_CLEAR;
    plic_disable(CDC_IRQ_TIMER0);
#endif

    if (timer_irq_count != TIMER0_IRQS_EXPECTED) {
        noc_fail("TIMER0 ISR/notification count");
    }
    timer_ok = 1;
    uart_put_u32("PLIC TIMER0 OK irqs=", timer_irq_count, "\n");

#if NOC_FREERTOS_STEP >= 124
    /*
     * Step 12.4 leaves TIMER0 free-running so the concurrent phase carries a
     * real periodic interrupt load. This task must therefore outlive the
     * acceptance above: `timer0_isr` targets `timer_task_handle`, and deleting
     * the task would leave the ISR notifying a dangling handle. Draining the
     * notifications here is the only reason it stays alive; the count the
     * supervisor reads is maintained by the ISR.
     */
    TIMER0_RELOAD = TIMER0_CONCURRENT_RELOAD_COUNTS;

    for (uint32_t drained = 0u; drained < 20000u; ++drained) {
        if (phase_stop) {
            TIMER0_CTRL = 0u;
            TIMER0_INTSTATUS = TIMER0_INT_CLEAR;
            plic_disable(CDC_IRQ_TIMER0);
            timer_teardown_done = 1;
            vTaskDelete(NULL);
        }
        (void)ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(1));
    }

    TIMER0_CTRL = 0u;
    plic_disable(CDC_IRQ_TIMER0);
    noc_fail("TIMER0 task drain bound exceeded");
#else
    vTaskDelete(NULL);
#endif
}

static void clint_tick_task(void *params)
{
    (void)params;

    if (uxTaskPriorityGet(NULL) != NOC_LOW_PRIORITY) {
        noc_fail("CLINT task priority");
    }

    for (uint32_t wake = 1u; wake <= CLINT_WAKE_ROUNDS; ++wake) {
        const TickType_t before = xTaskGetTickCount();

        vTaskDelay(pdMS_TO_TICKS(1));

        const TickType_t after = xTaskGetTickCount();
        if ((after - before) < pdMS_TO_TICKS(1)) {
            uart_puts("CLINT tick FAIL: delay did not advance\n");
            noc_fail("CLINT delay/wake");
        }
        uart_put_u32("NoC tick wake=", wake, "\n");
    }

    tick_ok = 1;
    uart_puts("CLINT tick OK\n");

    for (uint32_t waited = 0u; waited < 100u; ++waited) {
        if (scheduler_ok && timer_ok) {
#if NOC_FREERTOS_STEP == 122
            uart_puts("FreeRTOS NoC PASS\n");
            vTaskDelete(NULL);
#else
            xTaskNotifyGive(dma_task_handle);
            vTaskDelete(NULL);
#endif
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    uart_puts("FreeRTOS NoC FAIL: incomplete Step 12.2\n");
    noc_fail("scheduler/CLINT/PLIC completion");
}

#endif

#if NOC_FREERTOS_STEP >= 123

static uint32_t dma_reg_read(uint32_t offset)
{
    return *(volatile uint32_t *)(uintptr_t)(CDC_DMA0_BASE + offset);
}

static void dma_reg_write(uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)(CDC_DMA0_BASE + offset) = value;
}

static uint32_t dma_emit_mov(volatile uint8_t *program, uint32_t offset,
                             uint8_t reg, uint32_t value)
{
    program[offset + 0u] = 0xBCu;
    program[offset + 1u] = (uint8_t)(reg << 3);
    program[offset + 2u] = (uint8_t)(value >> 0);
    program[offset + 3u] = (uint8_t)(value >> 8);
    program[offset + 4u] = (uint8_t)(value >> 16);
    program[offset + 5u] = (uint8_t)(value >> 24);
    return offset + 6u;
}

static int dma_range_is_firmware_ram(const volatile uint8_t *first,
                                     uint32_t size)
{
    const uintptr_t address = (uintptr_t)first;
    const uintptr_t end = address + size;

    return end >= address &&
           address >= (uintptr_t)CDC_RAM0_BASE &&
           end <= (uintptr_t)__firmware_ram_end;
}

static void dma_done_isr(uint32_t source, void *arg)
{
    (void)arg;

    const uint32_t pending = dma_reg_read(CDC_DMA_INTMIS);
    if (source == CDC_IRQ_DMA0 && (pending & DMA_EVENT_MASK) != 0u) {
        BaseType_t woken = pdFALSE;

        /*
         * DMA IRQ is level-sensitive. Clear its W1C event before returning;
         * the common dispatcher completes PLIC source 7 afterwards.
         */
        dma_reg_write(CDC_DMA_INTCLR, DMA_EVENT_MASK);
        ++dma_irq_count;
        vTaskNotifyGiveFromISR(dma_task_handle, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

static void dma_transfer_task(void *params)
{
    (void)params;

    if (uxTaskPriorityGet(NULL) != NOC_HIGH_PRIORITY) {
        noc_fail("DMA task priority");
    }

    /* Task-level bound: Step 12.2 must finish before DMA begins. */
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) != 1u) {
        noc_fail("DMA prerequisite wait timeout");
    }
    if (!scheduler_ok || !tick_ok || !timer_ok) {
        noc_fail("DMA started before Step 12.2 completion");
    }

    if (!dma_range_is_firmware_ram(dma_program, sizeof(dma_program)) ||
        !dma_range_is_firmware_ram(dma_source, sizeof(dma_source)) ||
        !dma_range_is_firmware_ram(dma_destination,
                                   sizeof(dma_destination))) {
        noc_fail("DMA buffer outside noc_soc firmware RAM");
    }

    for (uint32_t i = 0u; i < DMA_COPY_BYTES; ++i) {
        dma_source[i] = (uint8_t)(0x41u + (i * 13u));
        dma_destination[i] = 0xA5u;
    }

    const uint32_t source_address = (uint32_t)(uintptr_t)dma_source;
    const uint32_t destination_address =
        (uint32_t)(uintptr_t)dma_destination;
    const uint32_t program_address = (uint32_t)(uintptr_t)dma_program;
    const uint32_t ccr =
        CDC_DMA_CCR_SRC_INC |
        (2u << CDC_DMA_CCR_SRC_BURST_SIZE_SHIFT) |
        (3u << CDC_DMA_CCR_SRC_BURST_LEN_SHIFT) |
        CDC_DMA_CCR_DST_INC |
        (2u << CDC_DMA_CCR_DST_BURST_SIZE_SHIFT) |
        (3u << CDC_DMA_CCR_DST_BURST_LEN_SHIFT);

    uint32_t pc = 0u;
    pc = dma_emit_mov(dma_program, pc, 1u, ccr);
    pc = dma_emit_mov(dma_program, pc, 0u, source_address);
    pc = dma_emit_mov(dma_program, pc, 2u, destination_address);
    dma_program[pc++] = 0x04u;
    dma_program[pc++] = 0x08u;
    dma_program[pc++] = 0x04u;
    dma_program[pc++] = 0x08u;
    dma_program[pc++] = 0x34u;
    dma_program[pc++] = (uint8_t)(DMA_EVENT_DONE << 3);
    dma_program[pc++] = 0x00u;
    if (pc != DMA_PROGRAM_USED_BYTES) {
        noc_fail("DMA program length");
    }

    dma_reg_write(CDC_DMA_INTEN, 0u);
    dma_reg_write(CDC_DMA_INTCLR, 0xffffffffu);
    if (dma_reg_read(CDC_DMA_INTMIS) != 0u ||
        dma_reg_read(CDC_DMA_FSRC) != 0u ||
        (dma_reg_read(CDC_DMA_CSR(0)) & 0xFu) != CDC_DMA_STATUS_STOPPED) {
        noc_fail("DMA not clean before launch");
    }
    if (plic_register_handler(CDC_IRQ_DMA0, dma_done_isr, NULL) != 0) {
        noc_fail("DMA PLIC handler registration");
    }
    plic_enable(CDC_IRQ_DMA0, 2u);

    dma_reg_write(CDC_DMA_INTEN, DMA_EVENT_MASK);
    if (dma_reg_read(CDC_DMA_INTEN) != DMA_EVENT_MASK) {
        noc_fail("DMA interrupt enable readback");
    }
    dma_reg_write(CDC_DMA_DBGINST0, 0xA0u << 16);
    dma_reg_write(CDC_DMA_DBGINST1, program_address);
    dma_reg_write(CDC_DMA_DBGCMD, 0u);

    /* Interrupt-completion bound: polling CSR/INTMIS is not acceptance. */
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) != 1u) {
        plic_disable(CDC_IRQ_DMA0);
        dma_reg_write(CDC_DMA_INTEN, 0u);
        noc_fail("DMA completion interrupt timeout");
    }

    plic_disable(CDC_IRQ_DMA0);
    dma_reg_write(CDC_DMA_INTEN, 0u);

    if (dma_irq_count != 1u) {
        noc_fail("DMA completion IRQ count");
    }
    if (dma_reg_read(CDC_DMA_INTMIS) != 0u ||
        dma_reg_read(CDC_DMA_INT_EVENT_RIS) != 0u) {
        noc_fail("DMA completion cause not cleared");
    }
    if ((dma_reg_read(CDC_DMA_CSR(0)) & 0xFu) !=
            CDC_DMA_STATUS_STOPPED ||
        dma_reg_read(CDC_DMA_FSRC) != 0u) {
        noc_fail("DMA channel completion status");
    }
    if (dma_reg_read(CDC_DMA_SAR(0)) !=
            source_address + DMA_COPY_BYTES ||
        dma_reg_read(CDC_DMA_DAR(0)) !=
            destination_address + DMA_COPY_BYTES) {
        noc_fail("DMA final source/destination address");
    }

    for (uint32_t i = 0u; i < DMA_COPY_BYTES; ++i) {
        const uint8_t expected = (uint8_t)(0x41u + (i * 13u));

        if (dma_source[i] != expected) {
            uart_put_u32("DMA NoC FAIL: source corruption byte=", i, "\n");
            noc_fail("DMA source comparison");
        }
        if (dma_destination[i] != expected) {
            uart_put_u32("DMA NoC FAIL: mismatch byte=", i, "\n");
            noc_fail("DMA payload comparison");
        }
    }

    uart_puts("DMA NoC IRQ source=7 count=1\n");
    uart_put_u32("DMA NoC bytes match=", DMA_COPY_BYTES, "\n");
    uart_puts("DMA NoC PASS\n");
#if NOC_FREERTOS_STEP == 123
    uart_puts("FreeRTOS NoC PASS\n");
    vTaskDelete(NULL);
#else
    /*
     * Hand the sequential proof to the supervisor and wait for it to start the
     * concurrent phase. The two-way handshake matters: the CPU workers must
     * already be running before DMA traffic is injected, otherwise the
     * transfers would be sequential again under a different name.
     */
    xTaskNotifyGive(supervisor_task_handle);
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) != 1u) {
        noc_fail("concurrent DMA start timeout");
    }

    if (plic_register_handler(CDC_IRQ_DMA0, dma_done_isr, NULL) != 0) {
        noc_fail("concurrent DMA PLIC handler registration");
    }
    if (!dma_range_is_firmware_ram((const volatile uint8_t *)
                                       dma_concurrent_program,
                                   sizeof(dma_concurrent_program)) ||
        !dma_range_is_firmware_ram((const volatile uint8_t *)
                                       dma_concurrent_source,
                                   sizeof(dma_concurrent_source)) ||
        !dma_range_is_firmware_ram((const volatile uint8_t *)
                                       dma_concurrent_destination,
                                   sizeof(dma_concurrent_destination))) {
        noc_fail("concurrent DMA buffer outside noc_soc firmware RAM");
    }

    const uint32_t concurrent_source_address =
        (uint32_t)(uintptr_t)dma_concurrent_source;
    const uint32_t concurrent_destination_address =
        (uint32_t)(uintptr_t)dma_concurrent_destination;
    const uint32_t concurrent_program_address =
        (uint32_t)(uintptr_t)dma_concurrent_program;

    /*
     * DMAMOV CCR/SAR/DAR, then DMALP over one DMALD/DMAST burst pair, then
     * DMASEV and DMAEND. DMALPEND jumps back by its own byte distance to the
     * loop body, and LC0 counts one fewer than the number of bursts.
     */
    uint32_t cpc = 0u;
    cpc = dma_emit_mov(dma_concurrent_program, cpc, 1u, ccr);
    cpc = dma_emit_mov(dma_concurrent_program, cpc, 0u,
                       concurrent_source_address);
    cpc = dma_emit_mov(dma_concurrent_program, cpc, 2u,
                       concurrent_destination_address);
    dma_concurrent_program[cpc++] = 0x20u;                    /* DMALP LC0  */
    dma_concurrent_program[cpc++] = (uint8_t)(DMA_CONCURRENT_BURSTS - 1u);
    const uint32_t loop_body = cpc;
    dma_concurrent_program[cpc++] = 0x04u;                    /* DMALD      */
    dma_concurrent_program[cpc++] = 0x08u;                    /* DMAST      */
    const uint32_t loop_end = cpc;
    dma_concurrent_program[cpc++] = 0x38u;                    /* DMALPEND   */
    dma_concurrent_program[cpc++] = (uint8_t)(loop_end - loop_body);
    dma_concurrent_program[cpc++] = 0x34u;                    /* DMASEV     */
    dma_concurrent_program[cpc++] = (uint8_t)(DMA_EVENT_DONE << 3);
    dma_concurrent_program[cpc++] = 0x00u;                    /* DMAEND     */
    if (cpc != DMA_CONCURRENT_PROGRAM_USED_BYTES) {
        noc_fail("concurrent DMA program length");
    }

    plic_enable(CDC_IRQ_DMA0, 2u);
    dma_reg_write(CDC_DMA_INTEN, DMA_EVENT_MASK);

    /*
     * TIMER0 overlap is measured across the whole transfer loop rather than
     * inside a single transfer: its period is an order of magnitude longer
     * than one transfer, so requiring an interrupt inside one window would be
     * a statement about the ratio of two periods, not about concurrency.
     */
    const uint32_t overlap_irqs_before = timer_irq_count;

    for (uint32_t transfer = 1u;
         transfer <= NOC_CONCURRENT_DMA_TRANSFERS;
         ++transfer) {
        const uint32_t expected_irqs = 1u + transfer;

        /* A per-transfer pattern: repeating the previous one would let a
         * dropped transfer pass on stale destination words. */
        for (uint32_t i = 0u; i < DMA_CONCURRENT_WORDS; ++i) {
            dma_concurrent_source[i] = dma_concurrent_word(transfer, i);
            dma_concurrent_destination[i] = ~dma_concurrent_word(transfer, i);
        }

        /* The channel program re-issues DMAMOV SAR/DAR/CCR on every launch,
         * so the same static program is replayable without rebuilding it. */
        const uint32_t overlap_a_before = worker_a_words;
        const uint32_t overlap_b_before = worker_b_words;

        dma_reg_write(CDC_DMA_DBGINST0, 0xA0u << 16);
        dma_reg_write(CDC_DMA_DBGINST1, concurrent_program_address);
        dma_reg_write(CDC_DMA_DBGCMD, 0u);

        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) != 1u) {
            plic_disable(CDC_IRQ_DMA0);
            dma_reg_write(CDC_DMA_INTEN, 0u);
            uart_put_u32("NoC concurrent FAIL: DMA timeout transfer=",
                         transfer, "\n");
            noc_fail("concurrent DMA completion timeout");
        }

        /*
         * Sampled between launch and completion, a window in which this task
         * was blocked. Whatever the lower-priority CPU tasks advanced here ran
         * while the transfer was in flight on the network.
         */
        overlap_worker_a += worker_a_words - overlap_a_before;
        overlap_worker_b += worker_b_words - overlap_b_before;

        if (dma_irq_count != expected_irqs) {
            uart_put_u32("NoC concurrent FAIL: DMA irq count=",
                         dma_irq_count, "\n");
            noc_fail("concurrent DMA completion count");
        }
        if ((dma_reg_read(CDC_DMA_CSR(0)) & 0xFu) !=
                CDC_DMA_STATUS_STOPPED ||
            dma_reg_read(CDC_DMA_FSRC) != 0u) {
            noc_fail("concurrent DMA channel status");
        }
        if (dma_reg_read(CDC_DMA_SAR(0)) !=
                concurrent_source_address + DMA_CONCURRENT_BYTES ||
            dma_reg_read(CDC_DMA_DAR(0)) !=
                concurrent_destination_address + DMA_CONCURRENT_BYTES) {
            noc_fail("concurrent DMA final source/destination address");
        }

        for (uint32_t i = 0u; i < DMA_CONCURRENT_WORDS; ++i) {
            const uint32_t expected = dma_concurrent_word(transfer, i);

            if (dma_concurrent_source[i] != expected) {
                uart_put_u32("NoC concurrent FAIL: DMA source word=", i, "\n");
                noc_fail("concurrent DMA source comparison");
            }
            if (dma_concurrent_destination[i] != expected) {
                uart_put_u32("NoC concurrent FAIL: DMA word=", i, "\n");
                noc_fail("concurrent DMA payload comparison");
            }
        }

        dma_concurrent_bytes += DMA_CONCURRENT_BYTES;
        dma_concurrent_transfers = transfer;
    }

    overlap_timer_irqs = timer_irq_count - overlap_irqs_before;

    plic_disable(CDC_IRQ_DMA0);
    dma_reg_write(CDC_DMA_INTEN, 0u);
    if (dma_reg_read(CDC_DMA_INTMIS) != 0u ||
        dma_reg_read(CDC_DMA_INT_EVENT_RIS) != 0u) {
        noc_fail("concurrent DMA cause not cleared");
    }
    dma_concurrent_ok = 1;
    vTaskDelete(NULL);
#endif
}

#endif

#if NOC_FREERTOS_STEP >= 124

/*
 * The resting value of every worker word. Each iteration reads the previous
 * generation back before writing the next one, so a byte the DMA manager, the
 * other worker or the network corrupted is caught on the next pass rather than
 * only at the end.
 */
static uint32_t work_word(uint32_t seed, uint32_t index, uint32_t generation)
{
    return seed + (index * 0x01010101u) + (generation * 0x9E3779B9u);
}

static void noc_worker_body(volatile uint32_t *buffer, uint32_t seed,
                            volatile uint32_t *iterations,
                            volatile uint32_t *words,
                            volatile int *done, int mmio_kind)
{
    uint32_t generation = 0u;
    uint32_t previous_mtime = 0u;
    uint32_t word_count = 0u;

    if (uxTaskPriorityGet(NULL) != NOC_WORKER_PRIORITY) {
        noc_fail("concurrent worker priority");
    }
    for (uint32_t index = 0u; index < NOC_WORK_WORDS; ++index) {
        buffer[index] = work_word(seed, index, generation);
    }

    /* Idle until the supervisor opens the phase: a worker spinning through the
     * Step 12.1 to 12.3 acceptance would perturb proofs that are already
     * signed off. */
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500)) != 1u) {
        noc_fail("concurrent worker start timeout");
    }

    while (!phase_stop) {
        const uint32_t next = generation + 1u;

        for (uint32_t index = 0u; index < NOC_WORK_WORDS; ++index) {
            if (buffer[index] != work_word(seed, index, generation)) {
                uart_put_u32("NoC concurrent FAIL: RAM word=", index, "\n");
                noc_fail("concurrent RAM corruption");
            }
            buffer[index] = work_word(seed, index, next);
            *words = ++word_count;
            if ((word_count % NOC_WORKER_YIELD_WORDS) == 0u) {
                taskYIELD();
            }
        }
        generation = next;

        /* One NoC-crossing MMIO access per iteration, checked rather than
         * merely issued. Both registers are side-effect free. */
        if (mmio_kind == NOC_WORKER_MMIO_CLINT) {
            const uint32_t now = clint_mtime_low();

            if (now < previous_mtime) {
                uart_put_u32("NoC concurrent FAIL: mtime=", now, "\n");
                noc_fail("CLINT mtime went backwards");
            }
            previous_mtime = now;
        } else {
            if (plic_reg_read(CDC_PLIC_PRIORITY(CDC_IRQ_TIMER0)) != 1u) {
                noc_fail("PLIC TIMER0 priority readback");
            }
        }

        *iterations = generation;
        taskYIELD();
    }

    /*
     * The generation written by the last iteration has no following iteration
     * to verify it. Read it back explicitly before reporting teardown
     * complete; otherwise corruption in the final writes could escape the
     * phase simply because the supervisor happened to stop at that boundary.
     */
    for (uint32_t index = 0u; index < NOC_WORK_WORDS; ++index) {
        if (buffer[index] != work_word(seed, index, generation)) {
            uart_put_u32("NoC concurrent FAIL: final RAM word=", index, "\n");
            noc_fail("concurrent final RAM corruption");
        }
    }

    *done = 1;
    vTaskDelete(NULL);
}

static void worker_a_task(void *params)
{
    (void)params;
    noc_worker_body(work_a, 0x5A5A0001u, &worker_a_iterations,
                    &worker_a_words, &worker_a_done, NOC_WORKER_MMIO_CLINT);
}

static void worker_b_task(void *params)
{
    (void)params;
    noc_worker_body(work_b, 0xC3C30002u, &worker_b_iterations,
                    &worker_b_words, &worker_b_done, NOC_WORKER_MMIO_PLIC);
}

static void supervisor_task(void *params)
{
    (void)params;

    if (uxTaskPriorityGet(NULL) != NOC_SUPERVISOR_PRIORITY) {
        noc_fail("supervisor task priority");
    }
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200)) != 1u) {
        noc_fail("concurrent phase start timeout");
    }
    if (!scheduler_ok || !tick_ok || !timer_ok) {
        noc_fail("concurrent phase started before Step 12.3");
    }

    const uint32_t timer_irqs_before = timer_irq_count;

    uart_puts("FreeRTOS NoC Step 12.4 concurrent\n");
    xTaskNotifyGive(worker_a_task_handle);
    xTaskNotifyGive(worker_b_task_handle);
    xTaskNotifyGive(dma_task_handle);

    /*
     * Progress is tracked in words rather than iterations. A worker yields
     * inside its loop, so a full 64-word iteration can take longer than one
     * checkpoint interval; requiring a completed iteration per checkpoint
     * would be a throughput threshold, not a liveness check.
     */
    uint32_t previous_a = worker_a_words;
    uint32_t previous_b = worker_b_words;
    TickType_t previous_ticks = xTaskGetTickCount();
    uint32_t checkpoints = 0u;

    while (checkpoints < NOC_CHECKPOINT_LIMIT) {
        vTaskDelay(pdMS_TO_TICKS(NOC_CHECKPOINT_MS));
        ++checkpoints;

        const uint32_t a = worker_a_words;
        const uint32_t b = worker_b_words;
        const TickType_t ticks = xTaskGetTickCount();

        /* Forward progress, per task, at every checkpoint. */
        if (a <= previous_a) {
            uart_put_u32("NoC concurrent FAIL: cpu-a stalled at=", a, "\n");
            noc_fail("concurrent CPU task A made no progress");
        }
        if (b <= previous_b) {
            uart_put_u32("NoC concurrent FAIL: cpu-b stalled at=", b, "\n");
            noc_fail("concurrent CPU task B made no progress");
        }
        if (ticks <= previous_ticks) {
            noc_fail("CLINT tick stopped during the concurrent phase");
        }
        previous_a = a;
        previous_b = b;
        previous_ticks = ticks;

        uart_put_u32("NoC concurrent checkpoint=", checkpoints, "\n");

        if (checkpoints >= NOC_CONCURRENT_CHECKPOINTS && dma_concurrent_ok) {
            break;
        }
    }

    if (!dma_concurrent_ok) {
        uart_put_u32("NoC concurrent FAIL: DMA transfers=",
                     dma_concurrent_transfers, "\n");
        noc_fail("concurrent DMA did not finish inside the phase bound");
    }

    phase_stop = 1;
    for (uint32_t waited = 0u; waited < 500u; ++waited) {
        if (worker_a_done && worker_b_done && timer_teardown_done) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (!worker_a_done || !worker_b_done || !timer_teardown_done) {
        noc_fail("concurrent phase teardown timeout");
    }

    const uint32_t phase_irqs = timer_irq_count - timer_irqs_before;

    if (phase_irqs < NOC_CONCURRENT_TIMER_IRQS) {
        uart_put_u32("NoC concurrent FAIL: TIMER0 irqs=", phase_irqs, "\n");
        noc_fail("TIMER0 stopped during the concurrent phase");
    }
    if (dma_concurrent_transfers != NOC_CONCURRENT_DMA_TRANSFERS ||
        dma_concurrent_bytes !=
            NOC_CONCURRENT_DMA_TRANSFERS * DMA_CONCURRENT_BYTES) {
        noc_fail("concurrent DMA transfer accounting");
    }
    if (dma_irq_count != 1u + NOC_CONCURRENT_DMA_TRANSFERS) {
        uart_put_u32("NoC concurrent FAIL: DMA completions=",
                     dma_irq_count, "\n");
        noc_fail("lost DMA completion");
    }

    /*
     * Overlap, not merely coexistence. These counts were accumulated by the
     * DMA task strictly between each launch and its completion interrupt, a
     * window in which that task is blocked, so they can only have come from
     * work done while a transfer was in flight on the network. The unit is
     * worker words rather than iterations because one transfer is shorter than
     * a full iteration; that is a resolution statement, not a timing claim.
     *
     * The TIMER0 figure is weaker by construction: it is scoped to the whole
     * transfer loop, so it says interrupts kept arriving while DMA traffic was
     * being issued, not that one arrived inside a given transfer.
     */
    if (overlap_worker_a == 0u || overlap_worker_b == 0u) {
        uart_put_u32("NoC concurrent FAIL: overlap cpu-a=",
                     overlap_worker_a, "\n");
        uart_put_u32("NoC concurrent FAIL: overlap cpu-b=",
                     overlap_worker_b, "\n");
        noc_fail("CPU tasks did not run during the DMA transfers");
    }
    if (overlap_timer_irqs == 0u) {
        noc_fail("TIMER0 did not fire during the DMA transfers");
    }

    /*
     * Nothing may be left claimed or asserted. `claimed_` is not readable, so
     * this is checked in two complementary ways: the phase itself required
     * repeated re-delivery on sources 4 and 7, which is impossible unless every
     * claim was completed, and the probe below must find the gateway idle. A
     * nonzero read is a claim, so it is completed immediately before failing.
     */
    taskENTER_CRITICAL();
    const uint32_t pending = plic_reg_read(CDC_PLIC_PENDING);
    const uint32_t stray = plic_reg_read(CDC_PLIC_CLAIM);
    if (stray != 0u) {
        plic_reg_write(CDC_PLIC_CLAIM, stray);
    }
    taskEXIT_CRITICAL();

    if (stray != 0u) {
        uart_put_u32("NoC concurrent FAIL: stray PLIC claim=", stray, "\n");
        noc_fail("PLIC source still pending after the concurrent phase");
    }
    if ((pending & ((1u << CDC_IRQ_TIMER0) | (1u << CDC_IRQ_DMA0) |
                    (1u << CDC_IRQ_DMA0_ABORT))) != 0u) {
        uart_put_u32("NoC concurrent FAIL: PLIC pending=", pending, "\n");
        noc_fail("device interrupt line still asserted");
    }

    uart_put_u32("NoC concurrent cpu-a iterations=", worker_a_iterations, "\n");
    uart_put_u32("NoC concurrent cpu-a words=", worker_a_words, "\n");
    uart_put_u32("NoC concurrent cpu-b iterations=", worker_b_iterations, "\n");
    uart_put_u32("NoC concurrent cpu-b words=", worker_b_words, "\n");
    uart_put_u32("NoC concurrent TIMER0 irqs=", phase_irqs, "\n");
    uart_put_u32("NoC concurrent DMA transfers=",
                 dma_concurrent_transfers, "\n");
    uart_put_u32("NoC concurrent DMA bytes=", dma_concurrent_bytes, "\n");
    uart_put_u32("NoC concurrent overlap cpu-a words=",
                 overlap_worker_a, "\n");
    uart_put_u32("NoC concurrent overlap cpu-b words=",
                 overlap_worker_b, "\n");
    uart_put_u32("NoC concurrent overlap TIMER0 irqs=",
                 overlap_timer_irqs, "\n");
    uart_put_u32("FreeRTOS NoC concurrent OK checkpoints=", checkpoints, "\n");
#if NOC_FREERTOS_STEP == 125
    report_tick_jitter();
#endif
    uart_puts("FreeRTOS NoC PASS\n");
#if NOC_FREERTOS_STEP == 128
    xTaskNotifyGive(cli_task_handle);
#endif
    vTaskDelete(NULL);
}

#endif

static void low_priority_task(void *params)
{
    (void)params;

    if (xTaskGetCurrentTaskHandle() != low_task_handle ||
        uxTaskPriorityGet(NULL) != NOC_LOW_PRIORITY) {
        noc_fail("low-task context or priority");
    }

    for (uint32_t round = 1u; round <= NOC_SWITCH_ROUNDS; ++round) {
        const turn_state_t expected =
            (round == 1u) ? TURN_BOOT : TURN_HIGH_COMPLETE;

        if (turn_state != expected ||
            low_rounds != (round - 1u) ||
            high_rounds != (round - 1u)) {
            noc_fail("low-task handoff order");
        }

        low_rounds = round;
        turn_state = TURN_LOW_COMPLETE;
        uart_put_u32("NoC low round=", round, "\n");

        /*
         * Waking the higher-priority task causes an immediate preemption. The
         * high task acknowledges this round and blocks on its next receive;
         * only then can this task resume and consume the acknowledgement.
         */
        xTaskNotifyGive(high_task_handle);
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) != 1u) {
            noc_fail("low-task acknowledgement count");
        }
        if (turn_state != TURN_HIGH_COMPLETE ||
            low_rounds != round ||
            high_rounds != round) {
            noc_fail("low-task acknowledgement order");
        }
    }

    uart_put_u32("FreeRTOS NoC context switching OK rounds=",
                 NOC_SWITCH_ROUNDS, "\n");
#if NOC_FREERTOS_STEP == 121
    uart_puts("FreeRTOS NoC PASS\n");
#else
    scheduler_ok = 1;
#endif
    vTaskDelete(NULL);
}

int main(void)
{
    uart_puts("FreeRTOS NoC boot\n");
    uart_init();
#if NOC_FREERTOS_STEP >= 122
    plic_init();
    uart_puts("FreeRTOS NoC Step 12.2 CLINT+PLIC\n");
#endif
#if NOC_FREERTOS_STEP >= 123
    uart_puts("FreeRTOS NoC Step 12.3 DMA\n");
#endif
#if NOC_FREERTOS_STEP == 128
    uart_puts("FreeRTOS NoC Step 12.8 CLI\n");
#endif

    BaseType_t ok = xTaskCreate(low_priority_task, "noc-low",
                                NOC_TASK_STACK_WORDS, NULL,
                                NOC_LOW_PRIORITY, &low_task_handle);
    configASSERT(ok == pdPASS);
    ok = xTaskCreate(high_priority_task, "noc-high",
                     NOC_TASK_STACK_WORDS, NULL,
                     NOC_HIGH_PRIORITY, &high_task_handle);
    configASSERT(ok == pdPASS);
#if NOC_FREERTOS_STEP >= 122
    ok = xTaskCreate(clint_tick_task, "noc-tick",
                     NOC_TASK_STACK_WORDS, NULL,
                     NOC_LOW_PRIORITY, NULL);
    configASSERT(ok == pdPASS);
    ok = xTaskCreate(timer_interrupt_task, "noc-timer",
                     NOC_TASK_STACK_WORDS, NULL,
                     NOC_TIMER_PRIORITY, &timer_task_handle);
    configASSERT(ok == pdPASS);
#endif
#if NOC_FREERTOS_STEP >= 123
    ok = xTaskCreate(dma_transfer_task, "noc-dma",
                     NOC_TASK_STACK_WORDS, NULL,
                     NOC_HIGH_PRIORITY, &dma_task_handle);
    configASSERT(ok == pdPASS);
#endif
#if NOC_FREERTOS_STEP >= 124
    ok = xTaskCreate(supervisor_task, "noc-super",
                     NOC_TASK_STACK_WORDS, NULL,
                     NOC_SUPERVISOR_PRIORITY, &supervisor_task_handle);
    configASSERT(ok == pdPASS);
    ok = xTaskCreate(worker_a_task, "noc-cpu-a",
                     NOC_TASK_STACK_WORDS, NULL,
                     NOC_WORKER_PRIORITY, &worker_a_task_handle);
    configASSERT(ok == pdPASS);
    ok = xTaskCreate(worker_b_task, "noc-cpu-b",
                     NOC_TASK_STACK_WORDS, NULL,
                     NOC_WORKER_PRIORITY, &worker_b_task_handle);
    configASSERT(ok == pdPASS);
#endif
#if NOC_FREERTOS_STEP == 128
    ok = xTaskCreate(noc_cli_task, "noc-cli",
                     NOC_TASK_STACK_WORDS, NULL,
                     NOC_LOW_PRIORITY, &cli_task_handle);
    configASSERT(ok == pdPASS);
#endif

    if (uxTaskPriorityGet(low_task_handle) != NOC_LOW_PRIORITY ||
        uxTaskPriorityGet(high_task_handle) != NOC_HIGH_PRIORITY) {
        noc_fail("created task priorities");
    }
    uart_puts("FreeRTOS NoC tasks priorities low=1 high=2\n");

    vTaskStartScheduler();
    noc_fail("scheduler did not start");
}

void vApplicationIdleHook(void)
{
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
