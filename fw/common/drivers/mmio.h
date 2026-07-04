/*
 * mmio.h - minimal 32-bit MMIO accessors for the VP_FX1 SoC.
 *
 * All SoC control registers are 32-bit little-endian words. Use these helpers
 * (not raw casts) so driver code reads uniformly and stays volatile-correct.
 */
#ifndef CDC_MMIO_H
#define CDC_MMIO_H

#include <stdint.h>

static inline void mmio_write32(uint32_t base, uint32_t offset, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)(base + offset) = value;
}

static inline uint32_t mmio_read32(uint32_t base, uint32_t offset)
{
    return *(volatile uint32_t *)(uintptr_t)(base + offset);
}

static inline void mmio_set_bits(uint32_t base, uint32_t offset, uint32_t mask)
{
    mmio_write32(base, offset, mmio_read32(base, offset) | mask);
}

static inline void mmio_clr_bits(uint32_t base, uint32_t offset, uint32_t mask)
{
    mmio_write32(base, offset, mmio_read32(base, offset) & ~mask);
}

#endif /* CDC_MMIO_H */
