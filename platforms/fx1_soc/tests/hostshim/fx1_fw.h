/*
 * Host stand-in for fw/fx1_soc/common/fx1_fw.h, for building BSP drivers on
 * the host against SystemC models (test_i2c_bsp_budget). Only what the I2C
 * driver uses: MMIO and mtime come from the test bench (MMIO through TLM to
 * the real model, mtime from sc_time_stamp); the PLIC is absent (polling).
 */
#ifndef FX1_FW_HOSTSHIM_H
#define FX1_FW_HOSTSHIM_H

#include <stddef.h>
#include <stdint.h>

#define FX1_MTIME_HZ 20000000u

#ifdef __cplusplus
extern "C" {
#endif

uint64_t fx1_mtime(void);
uint32_t fx1_read32(uintptr_t address);
void fx1_write32(uintptr_t address, uint32_t value);

static inline void fx1_plic_set_priority(uint32_t source, uint32_t priority) { (void)source; (void)priority; }
static inline void fx1_plic_enable(uint32_t context, uint32_t source, int on) { (void)context; (void)source; (void)on; }
static inline uint32_t fx1_plic_pending(void) { return 0; }
static inline uint32_t fx1_plic_claim(uint32_t context) { (void)context; return 0; }
static inline void fx1_plic_complete(uint32_t context, uint32_t source) { (void)context; (void)source; }

#ifdef __cplusplus
}
#endif

#endif /* FX1_FW_HOSTSHIM_H */
