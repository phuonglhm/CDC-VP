/* SPDX-License-Identifier: Apache-2.0 */

#include "uart.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "plic.h"
#include "soc/soc_irq_map.h"
#include "soc/soc_memory_map.h"

/* PL011 register subset implemented by components/uart2_tlm. The model's
 * firmware-facing MMIO contract is 32-bit, including UARTDR. */
#define UART_DR   (*(volatile uint32_t *)(uintptr_t)(CDC_UART0_BASE + 0x000u))
#define UART_ECR  (*(volatile uint32_t *)(uintptr_t)(CDC_UART0_BASE + 0x004u))
#define UART_FR   (*(volatile uint32_t *)(uintptr_t)(CDC_UART0_BASE + 0x018u))
#define UART_IFLS (*(volatile uint32_t *)(uintptr_t)(CDC_UART0_BASE + 0x034u))
#define UART_IMSC (*(volatile uint32_t *)(uintptr_t)(CDC_UART0_BASE + 0x038u))
#define UART_MIS  (*(volatile uint32_t *)(uintptr_t)(CDC_UART0_BASE + 0x040u))
#define UART_ICR  (*(volatile uint32_t *)(uintptr_t)(CDC_UART0_BASE + 0x044u))

#define UART_FR_RXFE (1u << 4)
#define UART_FR_TXFF (1u << 5)

#define UART_INT_RX  (1u << 4)
#define UART_INT_RT  (1u << 6)
#define UART_INT_FE  (1u << 7)
#define UART_INT_PE  (1u << 8)
#define UART_INT_BE  (1u << 9)
#define UART_INT_OE  (1u << 10)
#define UART_INT_RX_ERRORS \
    (UART_INT_FE | UART_INT_PE | UART_INT_BE | UART_INT_OE)
#define UART_INT_RX_ALL (UART_INT_RX | UART_INT_RT | UART_INT_RX_ERRORS)

#define UART_RX_QUEUE_LENGTH 128u

static SemaphoreHandle_t uart_mutex;
static QueueHandle_t uart_rx_queue;
static volatile uint32_t rx_dropped;
static int rx_started;

/* PL011-correct TX: wait for FIFO space instead of blind DR writes. The
 * 16-deep TX FIFO drains only when the UART thread gets scheduled (once per
 * TLM quantum with a temporally decoupled CPU), so blind writes silently drop
 * everything past 16 chars at large --quantum values. */
static void uart_putc_raw(char c)
{
    while ((UART_FR & UART_FR_TXFF) != 0u) {
    }
    UART_DR = (uint32_t)(uint8_t)c;
}

void uart_init(void)
{
    uart_mutex = xSemaphoreCreateMutex();
    configASSERT(uart_mutex != NULL);
    uart_rx_queue = xQueueCreate(UART_RX_QUEUE_LENGTH, sizeof(char));
    configASSERT(uart_rx_queue != NULL);
}

static void uart0_isr(uint32_t source, void *arg)
{
    (void)source;
    (void)arg;

    BaseType_t woken = pdFALSE;
    const uint32_t pending = UART_MIS;

    /* Drain the FIFO first. Reading the final byte deasserts RX/RT in the
     * model. This is the device-clear-before-PLIC-complete ordering required
     * for every level interrupt on this platform. */
    while ((UART_FR & UART_FR_RXFE) == 0u) {
        const char c = (char)(UART_DR & 0xFFu);

        if (xQueueSendFromISR(uart_rx_queue, &c, &woken) != pdPASS) {
            ++rx_dropped;
        }
    }

    if ((pending & UART_INT_RX_ERRORS) != 0u) {
        UART_ECR = 0u;
    }
    UART_ICR = pending & UART_INT_RX_ALL;

    portYIELD_FROM_ISR(woken);
}

void uart_rx_start(void)
{
    if (rx_started) {
        return;
    }

    configASSERT(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
    configASSERT(uart_rx_queue != NULL);

    /* Mask and clear stale causes before exposing source 1 to the PLIC. Keep
     * any bytes that arrived before the task started in the hardware FIFO.
     * Selecting the 1/8 RX trigger afterwards re-evaluates that FIFO level. */
    UART_IMSC = 0u;
    UART_ICR = UART_INT_RX_ALL;
    UART_ECR = 0u;

    configASSERT(plic_register_handler(CDC_IRQ_UART0, uart0_isr, NULL) == 0);
    plic_enable(CDC_IRQ_UART0, 1u);

    UART_IFLS &= 0x7u; /* preserve TXIFLSEL, set RXIFLSEL=0 (2-byte trigger) */
    UART_IMSC = UART_INT_RX_ALL;
    rx_started = 1;
}

int uart_getc(char *out, TickType_t timeout)
{
    if (out == NULL || uart_rx_queue == NULL || !rx_started) {
        return 0;
    }
    return xQueueReceive(uart_rx_queue, out, timeout) == pdPASS;
}

uint32_t uart_rx_dropped(void)
{
    return rx_dropped;
}

static int uart_lock(void)
{
    /* The GCC/RISC-V port has no xPortIsInsideInterrupt(); bsp_in_isr() is
     * maintained by the PLIC dispatcher instead. ISR/pre-scheduler callers
     * print unlocked. */
    if (uart_mutex != NULL &&
        xTaskGetSchedulerState() == taskSCHEDULER_RUNNING &&
        !bsp_in_isr()) {
        xSemaphoreTake(uart_mutex, portMAX_DELAY);
        return 1;
    }
    return 0;
}

static void uart_unlock(int locked)
{
    if (locked) {
        xSemaphoreGive(uart_mutex);
    }
}

static void put_raw(const char *s)
{
    while (*s != '\0') {
        uart_putc_raw(*s++);
    }
}

static void put_dec_raw(uint32_t value)
{
    char buf[11];
    int i = 0;

    do {
        buf[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);
    while (i > 0) {
        uart_putc_raw(buf[--i]);
    }
}

static void put_hex_raw(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";

    put_raw("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        uart_putc_raw(hex[(value >> shift) & 0xFu]);
    }
}

void uart_putc(char c)
{
    const int locked = uart_lock();

    uart_putc_raw(c);
    uart_unlock(locked);
}

void uart_puts(const char *s)
{
    const int locked = uart_lock();

    put_raw(s);
    uart_unlock(locked);
}

void uart_puts2(const char *a, const char *b)
{
    const int locked = uart_lock();

    put_raw(a);
    put_raw(b);
    uart_unlock(locked);
}

void uart_put_u32(const char *prefix, uint32_t value, const char *suffix)
{
    const int locked = uart_lock();

    put_raw(prefix);
    put_dec_raw(value);
    put_raw(suffix);
    uart_unlock(locked);
}

void uart_put_i32(const char *prefix, int32_t value, const char *suffix)
{
    const int locked = uart_lock();
    uint32_t magnitude;

    put_raw(prefix);
    if (value < 0) {
        uart_putc_raw('-');
        magnitude = 0u - (uint32_t)value;
    } else {
        magnitude = (uint32_t)value;
    }
    put_dec_raw(magnitude);
    put_raw(suffix);
    uart_unlock(locked);
}

void uart_put_hex32(const char *prefix, uint32_t value, const char *suffix)
{
    const int locked = uart_lock();

    put_raw(prefix);
    put_hex_raw(value);
    put_raw(suffix);
    uart_unlock(locked);
}
