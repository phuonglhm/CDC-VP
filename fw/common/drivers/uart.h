/*
 * uart.h - polled console driver for uart2_tlm (PL011-style) on VP_FX1.
 *
 * Register offsets (from UART base):
 *   0x000 UARTDR  data register
 *   0x018 UARTFR  flag register (bit5 TXFF = transmit FIFO full)
 *
 * The VP models a functional PL011; for console output no baud-rate program is
 * required. This driver is polled TX only - enough for firmware/driver bring-up.
 */
#ifndef CDC_UART_H
#define CDC_UART_H

#include <stdint.h>

#define UART_DR       0x000u
#define UART_FR       0x018u
#define UART_FR_TXFF  (1u << 5)

/* Send one byte, blocking while the TX FIFO is full. */
void uart_putc(uint32_t base, char c);

/* Send a NUL-terminated string (LF is passed through as-is). */
void uart_puts(uint32_t base, const char *s);

#endif /* CDC_UART_H */
