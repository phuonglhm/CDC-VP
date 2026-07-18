/* SPDX-License-Identifier: Apache-2.0 */

#include "plic.h"

#include "FreeRTOS.h"
#include "task.h"

#include "soc/soc_irq_map.h"
#include "soc/soc_memory_map.h"

static inline void write32(uint32_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
}

static inline uint32_t read32(uint32_t address)
{
    return *(volatile uint32_t *)(uintptr_t)address;
}

typedef struct {
    plic_handler_t handler;
    void *arg;
} plic_slot_t;

static plic_slot_t plic_slots[CDC_PLIC_NUM_SOURCES + 1u];
static volatile uint32_t in_isr_depth;

int bsp_in_isr(void)
{
    return in_isr_depth != 0u;
}

void plic_init(void)
{
    write32(CDC_PLIC_BASE + CDC_PLIC_ENABLE, 0u);
    write32(CDC_PLIC_BASE + CDC_PLIC_THRESHOLD, 0u);
}

int plic_register_handler(uint32_t source, plic_handler_t handler, void *arg)
{
    if (source == 0u || source > CDC_PLIC_NUM_SOURCES) {
        return -1;
    }
    plic_slots[source].handler = handler;
    plic_slots[source].arg = arg;
    return 0;
}

void plic_enable(uint32_t source, uint32_t priority)
{
    configASSERT(source != 0u && source <= CDC_PLIC_NUM_SOURCES);

    write32(CDC_PLIC_BASE + CDC_PLIC_PRIORITY(source), priority);

    taskENTER_CRITICAL();
    write32(CDC_PLIC_BASE + CDC_PLIC_ENABLE,
            read32(CDC_PLIC_BASE + CDC_PLIC_ENABLE) | (1u << source));
    taskEXIT_CRITICAL();
}

void plic_disable(uint32_t source)
{
    configASSERT(source != 0u && source <= CDC_PLIC_NUM_SOURCES);

    taskENTER_CRITICAL();
    write32(CDC_PLIC_BASE + CDC_PLIC_ENABLE,
            read32(CDC_PLIC_BASE + CDC_PLIC_ENABLE) & ~(1u << source));
    taskEXIT_CRITICAL();
}

static void plic_dispatch(void)
{
    /* Drain every pending source. The claim register returns 0 when nothing
     * is pending. A claim is ALWAYS completed, including unexpected sources,
     * after the device handler has deasserted the device level (guardrail 5:
     * device cause cleared before PLIC completion). */
    for (;;) {
        const uint32_t claim = read32(CDC_PLIC_BASE + CDC_PLIC_CLAIM);

        if (claim == 0u) {
            break;
        }
        if (claim <= CDC_PLIC_NUM_SOURCES &&
            plic_slots[claim].handler != NULL) {
            plic_slots[claim].handler(claim, plic_slots[claim].arg);
        }
        write32(CDC_PLIC_BASE + CDC_PLIC_CLAIM, claim);
    }
}

/* Overrides the weak spin-loop in portASM.S. Called by the FreeRTOS trap
 * handler for every asynchronous interrupt that is not the machine timer
 * (the port consumes MTIP itself for the tick). */
void freertos_risc_v_application_interrupt_handler(void)
{
    uint32_t mcause;

    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));

    in_isr_depth++;
    if ((mcause & 0x7FFFFFFFu) == CDC_CAUSE_MEIP) {
        plic_dispatch();
    }
    /* MSIP is unused in this firmware; ignore anything else. */
    in_isr_depth--;
}
