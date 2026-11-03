/*
 * Host stand-in for fw/fx1_soc/common/fx1_fw.h, for building BSP drivers on
 * the host against SystemC models (test_i2c_bsp_budget, test_eth_manager).
 * Only what those drivers use: MMIO, mtime and the hart interrupt mask come
 * from the test bench (MMIO through TLM to the real model, mtime from
 * sc_time_stamp, fx1_irq_save/restore gate the bench's ISR thread); the
 * PLIC is absent (polling); DMA cache maintenance is a no-op (no cache).
 */
#ifndef FX1_FW_HOSTSHIM_H
#define FX1_FW_HOSTSHIM_H

#include <stddef.h>
#include <stdint.h>

#include "fx1/fx1_memory_map.h"

#ifdef __cplusplus
extern "C" {
#endif

uint64_t fx1_mtime(void);
uint32_t fx1_read32(uintptr_t address);
void fx1_write32(uintptr_t address, uint32_t value);
/* defined by a bench whose driver masks interrupts (test_eth_manager) */
uint32_t fx1_irq_save(void);
void fx1_irq_restore(uint32_t saved);

typedef enum {
    FX1_DMA_TO_DEVICE = 1,
    FX1_DMA_FROM_DEVICE = 2,
    FX1_DMA_BIDIRECTIONAL = 3,
} fx1_dma_dir_t;
static inline void fx1_dma_prepare(const volatile void* addr, uint32_t bytes, fx1_dma_dir_t dir) { (void)addr; (void)bytes; (void)dir; }
static inline void fx1_dma_complete(const volatile void* addr, uint32_t bytes, fx1_dma_dir_t dir) { (void)addr; (void)bytes; (void)dir; }

static inline void fx1_plic_set_priority(uint32_t source, uint32_t priority) { (void)source; (void)priority; }
static inline void fx1_plic_enable(uint32_t context, uint32_t source, int on) { (void)context; (void)source; (void)on; }
static inline uint32_t fx1_plic_pending(void) { return 0; }
static inline uint32_t fx1_plic_claim(uint32_t context) { (void)context; return 0; }
static inline void fx1_plic_complete(uint32_t context, uint32_t source) { (void)context; (void)source; }

#ifdef __cplusplus
}
#endif

#endif /* FX1_FW_HOSTSHIM_H */
