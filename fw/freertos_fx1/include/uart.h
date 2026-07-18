/* SPDX-License-Identifier: Apache-2.0
 *
 * UART0 console for the FreeRTOS firmware. Task-safe: once uart_init() has
 * created the lock and the scheduler is running, each call prints atomically.
 * Callable from ISRs and pre-scheduler code too (lock silently skipped).
 */
#ifndef FREERTOS_FX1_UART_H
#define FREERTOS_FX1_UART_H

#include <stdint.h>

void uart_init(void);              /* create the console mutex (after kernel heap is usable) */
void uart_puts(const char *s);     /* atomic string print */
void uart_puts2(const char *a, const char *b);          /* atomic "a" + "b" */
void uart_put_u32(const char *prefix, uint32_t value,
                  const char *suffix);                  /* atomic "<prefix><dec><suffix>" */
void uart_put_hex32(const char *prefix, uint32_t value,
                    const char *suffix);                /* atomic "<prefix>0x<hex><suffix>" */

#endif /* FREERTOS_FX1_UART_H */
