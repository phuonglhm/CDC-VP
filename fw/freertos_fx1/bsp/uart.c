/* SPDX-License-Identifier: Apache-2.0 */

#include "uart.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "plic.h"
#include "soc/soc_memory_map.h"

#define UART_DR (*(volatile uint8_t *)(uintptr_t)CDC_UART0_BASE)
#define UART_FR (*(volatile uint32_t *)(uintptr_t)(CDC_UART0_BASE + 0x18u))
#define UART_FR_TXFF (1u << 5)

static SemaphoreHandle_t uart_mutex;

/* PL011-correct TX: wait for FIFO space instead of blind DR writes. The
 * 16-deep TX FIFO drains only when the UART thread gets scheduled (once per
 * TLM quantum with a temporally decoupled CPU), so blind writes silently drop
 * everything past 16 chars at large --quantum values. */
static void uart_putc(char c)
{
    while ((UART_FR & UART_FR_TXFF) != 0u) {
    }
    UART_DR = (uint8_t)c;
}

void uart_init(void)
{
    uart_mutex = xSemaphoreCreateMutex();
    configASSERT(uart_mutex != NULL);
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
        uart_putc(*s++);
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
        uart_putc(buf[--i]);
    }
}

static void put_hex_raw(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";

    put_raw("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        uart_putc(hex[(value >> shift) & 0xFu]);
    }
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

void uart_put_hex32(const char *prefix, uint32_t value, const char *suffix)
{
    const int locked = uart_lock();

    put_raw(prefix);
    put_hex_raw(value);
    put_raw(suffix);
    uart_unlock(locked);
}
