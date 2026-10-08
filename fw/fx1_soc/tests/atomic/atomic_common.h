/*
 * Shared by the plan C7 atomic tests: LR/SC/AMO primitives, a one-command
 * SYS_DMA channel-0 M2M copy, and the test-area layout. Every word under test
 * sits in its own FX1_RESERVATION_GRANULE so cases do not disturb each other.
 */
#ifndef ATOMIC_COMMON_H
#define ATOMIC_COMMON_H

#include "fx1_fw.h"
#include "dma/fx1_dma_regs.h"

#define AT_DMA(offset) (FX1_SYS_DMA_CSR_BASE + (offset))
#define AT_CH0(offset) AT_DMA(FX1_DMA_CH(0, (offset)))
#define AT_GRANULE     FX1_RESERVATION_GRANULE
/* Test area: DMA sources at +0, the chained descriptor at +0x800, words under
 * test from +0x1000 (one granule each, up to 448), the DMA's padding copy at
 * +0x8000 -> +0xC000. */
#define AT_SRC         (FX1_FW_TEST_AREA_BASE + 0x0000u)
#define AT_DESC        (FX1_FW_TEST_AREA_BASE + 0x0800u)
#define AT_WORDS       (FX1_FW_TEST_AREA_BASE + 0x1000u)
#define AT_WORD(n)     (AT_WORDS + (uint32_t)(n) * AT_GRANULE)
#define AT_PAD_SRC     (FX1_FW_TEST_AREA_BASE + 0x8000u)
#define AT_PAD_DST     (FX1_FW_TEST_AREA_BASE + 0xC000u)
/* A padding copy of this size takes several microseconds: long enough for the
 * hart to be at its LR (or in its AMO loop) before the real write lands. */
#define AT_PAD_BYTES   FX1_DMA_MAX_TRANSFER

static inline uint32_t at_lr(uint32_t addr)
{
    uint32_t value;
    __asm__ volatile("lr.w %0, (%1)" : "=r"(value) : "r"(addr) : "memory");
    return value;
}

/* 0 = stored, nonzero = failed (the RISC-V convention). */
static inline uint32_t at_sc(uint32_t addr, uint32_t value)
{
    uint32_t failed;
    __asm__ volatile("sc.w %0, %2, (%1)" : "=&r"(failed) : "r"(addr), "r"(value) : "memory");
    return failed;
}

static inline uint32_t at_amoor(uint32_t addr, uint32_t bits)
{
    uint32_t old;
    __asm__ volatile("amoor.w %0, %2, (%1)" : "=r"(old) : "r"(addr), "r"(bits) : "memory");
    return old;
}

static inline uint32_t at_read(uint32_t addr) { return *(volatile uint32_t*)addr; }
static inline void at_write(uint32_t addr, uint32_t value) { *(volatile uint32_t*)addr = value; }

/* LR, then sleep in WFI until `mip & wake` (interrupts globally masked, so no
 * trap is taken), then SC: one asm block so the LR..SC distance is a few
 * instructions whatever the compiler does. Returns the SC result; *lr_value
 * gets what the LR read. */
static inline uint32_t at_lr_wait_sc(uint32_t addr, uint32_t value, uint32_t wake, uint32_t* lr_value)
{
    uint32_t old, failed, mip;
    __asm__ volatile("lr.w   %0, (%3)\n"
                     "1: csrr %2, mip\n"
                     "and    %2, %2, %5\n"
                     "bnez   %2, 2f\n"
                     "wfi\n"
                     "j      1b\n"
                     "2: sc.w %1, %4, (%3)\n"
                     : "=&r"(old), "=&r"(failed), "=&r"(mip)
                     : "r"(addr), "r"(value), "r"(wake)
                     : "memory");
    *lr_value = old;
    return failed;
}

/* Channel 0 M2M copy of `bytes` from src to dst with a completion interrupt
 * request; the caller decides whether the interrupt reaches a hart (PLIC
 * enable). Hands src (TO_DEVICE) and dst (FROM_DEVICE) to the device; the
 * caller calls fx1_dma_complete(dst, ..., FX1_DMA_FROM_DEVICE) after completion. With pad_bytes != 0 a padding copy of that size (AT_PAD_SRC ->
 * AT_PAD_DST, no interrupt) runs first and chains to the real copy through a
 * descriptor in memory, which delays the real write deterministically. */
void at_dma_start(uint32_t src, uint32_t dst, uint32_t bytes, uint32_t pad_bytes);
/* COMMAND_COMPLETE seen (and no error); 0 while busy. Fails the test on an error. */
int at_dma_done(void);
/* Clear the channel interrupt at the DMA. */
void at_dma_ack(void);
/* Claim/complete any pending PLIC source on context `ctx`; returns the ID. */
uint32_t at_plic_drain(uint32_t ctx);

#endif /* ATOMIC_COMMON_H */
