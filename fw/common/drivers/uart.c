/* uart.c - polled TX for uart2_tlm (PL011-style). See uart.h. */
#include "uart.h"
#include "mmio.h"

void uart_putc(uint32_t base, char c)
{
    while (mmio_read32(base, UART_FR) & UART_FR_TXFF) {
        /* spin until the transmit FIFO has room */
    }
    mmio_write32(base, UART_DR, (uint32_t)(unsigned char)c);
}

void uart_puts(uint32_t base, const char *s)
{
    while (*s) {
        uart_putc(base, *s++);
    }
}
