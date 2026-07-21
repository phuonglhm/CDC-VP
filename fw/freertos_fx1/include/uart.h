/* SPDX-License-Identifier: Apache-2.0
 *
 * UART0 console for the FreeRTOS firmware.
 *
 * TX is task-safe: once uart_init() has created the lock and the scheduler is
 * running, each call prints atomically. TX is callable from ISRs and
 * pre-scheduler code too (the lock is silently skipped).
 *
 * RX is interrupt-driven through PLIC source 1. uart_rx_start() must be called
 * once from task context after the scheduler starts. Exactly one task may
 * block in uart_getc() at a time.
 */
#ifndef FREERTOS_FX1_UART_H
#define FREERTOS_FX1_UART_H

#include "FreeRTOS.h"

#include <stdint.h>

void uart_init(void);              /* create TX mutex and RX queue before scheduler start */
void uart_rx_start(void);          /* enable PL011 RX/RT IRQ + PLIC source 1 */
int  uart_getc(char *out, TickType_t timeout);
uint32_t uart_rx_dropped(void);    /* bytes dropped because the RX queue was full */

void uart_putc(char c);            /* atomic single-character print */
void uart_puts(const char *s);     /* atomic string print */
void uart_puts2(const char *a, const char *b);          /* atomic "a" + "b" */
void uart_put_u32(const char *prefix, uint32_t value,
                  const char *suffix);                  /* atomic "<prefix><dec><suffix>" */
void uart_put_i32(const char *prefix, int32_t value,
                  const char *suffix);                  /* atomic signed decimal */
void uart_put_hex32(const char *prefix, uint32_t value,
                    const char *suffix);                /* atomic "<prefix>0x<hex><suffix>" */

#endif /* FREERTOS_FX1_UART_H */
