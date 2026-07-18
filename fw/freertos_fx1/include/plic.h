/* SPDX-License-Identifier: Apache-2.0
 *
 * Single machine-context PLIC dispatcher for the FreeRTOS firmware
 * (docs/NPU_V4_INTEGRATION_HANDOFF.md section 13.6).
 *
 * Exactly one MEIP entry point exists: the FreeRTOS port's trap handler calls
 * freertos_risc_v_application_interrupt_handler() (implemented in plic.c),
 * which claims, dispatches to the registered per-source handler, and always
 * completes the claim. Device handlers must deassert the device-level cause
 * (W1C) before returning - completion happens after the handler returns.
 */
#ifndef FREERTOS_FX1_PLIC_H
#define FREERTOS_FX1_PLIC_H

#include <stdint.h>

typedef void (*plic_handler_t)(uint32_t source, void *arg);

void plic_init(void);
int  plic_register_handler(uint32_t source, plic_handler_t handler, void *arg);
void plic_enable(uint32_t source, uint32_t priority);
void plic_disable(uint32_t source);

/* True while a PLIC/BSP interrupt handler is executing (ISR context marker
 * for code that must choose between FromISR and task APIs). */
int  bsp_in_isr(void);

#endif /* FREERTOS_FX1_PLIC_H */
