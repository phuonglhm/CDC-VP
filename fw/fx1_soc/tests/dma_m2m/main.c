/*
 * G3 / plan P3: SYS_DMA on the FX1 platform, driven by the CPU.
 *   1  256-byte aligned M2M copy, completion through PLIC source 2 (MEIP),
 *      handler checks CORE_STATUS / CH_INTERRUPT_STATUS, clears at the DMA
 *      (W1C) before completing at the PLIC; data compared byte for byte
 *   2  1023-byte (maximum) copy with odd source/destination alignment
 *   3  error path: destination unmapped -> WRITE_DECERR interrupt, no completion
 *   4  sub-word CPU access to the APB3 DMA CSRs -> load/store access fault
 *      (the DMA answers BURST_ERROR; the FX1 CPU port maps it, plan C4)
 */
#include "fx1_fw.h"
#include "dma/fx1_dma_regs.h"

#define DMA(offset)   (FX1_SYS_DMA_CSR_BASE + (offset))
#define CH0(offset)   DMA(FX1_DMA_CH(0, (offset)))
#define SRC           (FX1_FW_TEST_AREA_BASE + 0x0000u)
#define DST           (FX1_FW_TEST_AREA_BASE + 0x10000u)
#define UNMAPPED      0x60000000u
#define CFG_M2M       (FX1_DMA_ADDRESS_INCREMENT | FX1_DMA_CFG_TOKENS(1) | FX1_DMA_CFG_BURST_BYTES(32))

static volatile uint32_t irqs, last_status, last_core;
static volatile uint32_t faults, fault_cause;

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    (void)mtval;
    if (mcause == (FX1_MCAUSE_INTERRUPT | FX1_IRQ_M_EXT)) {
        const uint32_t id = fx1_plic_claim(0);
        if (id == FX1_IRQ_SYS_DMA) {
            last_core = fx1_read32(DMA(FX1_DMA_CORE_STATUS));
            last_status = fx1_read32(CH0(FX1_DMA_CH_INTERRUPT_STATUS));
            fx1_write32(CH0(FX1_DMA_CH_INTERRUPT_CLEAR), last_status); /* clear at the device */
            ++irqs;
        }
        if (id) fx1_plic_complete(0, id);
        return mepc;
    }
    if (mcause == 5u || mcause == 7u) { /* probe in step 4 */
        ++faults;
        fault_cause = mcause;
        return mepc + 4;
    }
    fx1_log_hex("unexpected mcause", mcause);
    fx1_fail(0x0E0);
}

static void fill(uint32_t base, uint32_t bytes, uint32_t seed)
{
    for (uint32_t i = 0; i < bytes; ++i) *(volatile uint8_t*)(base + i) = (uint8_t)(i * 13u + seed);
}

static int same(uint32_t a, uint32_t b, uint32_t bytes)
{
    for (uint32_t i = 0; i < bytes; ++i)
        if (*(volatile uint8_t*)(a + i) != *(volatile uint8_t*)(b + i)) return 0;
    return 1;
}

static int wait_irqs(uint32_t expected)
{
    const uint64_t start = fx1_mtime();
    while (irqs != expected)
        if (fx1_mtime() - start > FX1_MTIME_HZ / 1000) return 0; /* 1 ms */
    return 1;
}

static void run_m2m(uint32_t src, uint32_t dst, uint32_t bytes)
{
    fx1_write32(CH0(FX1_DMA_CH_ENABLE), 0);
    fx1_write32(CH0(FX1_DMA_CH_INTERRUPT_CLEAR), FX1_DMA_INTERRUPT_MASK);
    fx1_write32(CH0(FX1_DMA_CH_CMD_READ_ADDR), src);
    fx1_write32(CH0(FX1_DMA_CH_CMD_WRITE_ADDR), dst);
    fx1_write32(CH0(FX1_DMA_CH_CMD_TRANSFER_SIZE), bytes);
    fx1_write32(CH0(FX1_DMA_CH_CMD_CONTROL), FX1_DMA_CMD_SET_INT | FX1_DMA_CMD_LAST);
    fx1_write32(CH0(FX1_DMA_CH_READ_CONFIG), CFG_M2M);
    fx1_write32(CH0(FX1_DMA_CH_WRITE_CONFIG), CFG_M2M);
    fx1_write32(CH0(FX1_DMA_CH_PERIPHERAL_CONFIG), 0);
    fx1_write32(CH0(FX1_DMA_CH_MODE_CONFIG), FX1_DMA_MODE_SWAP_NONE);
    fx1_write32(CH0(FX1_DMA_CH_ENABLE), 1);
    /* Plan C12: the source and the destination both go to the device. */
    fx1_dma_prepare((const volatile void*)src, bytes, FX1_DMA_TO_DEVICE);
    fx1_dma_prepare((const volatile void*)dst, bytes, FX1_DMA_FROM_DEVICE);
    fx1_write32(CH0(FX1_DMA_CH_START), 1);
}

int main(void)
{
    fx1_check(fx1_read32(DMA(FX1_DMA_CORE_CAPABILITY_STATUS0)) == FX1_DMA_CAPABILITY0, 0x01,
              "DMA identity register");
    fx1_set_trap_handler(handler);
    fx1_plic_set_priority(FX1_IRQ_SYS_DMA, 1);
    fx1_plic_enable(0, FX1_IRQ_SYS_DMA, 1);
    fx1_irq_enable(FX1_MIE_MEIE);
    fx1_global_irq_enable();

    /* 1: aligned 256-byte copy */
    fill(SRC, 256, 7);
    run_m2m(SRC, DST, 256);
    fx1_check(wait_irqs(1), 0x10, "1: completion interrupt");
    fx1_dma_complete((const volatile void*)DST, 256, FX1_DMA_FROM_DEVICE);
    fx1_check(last_status == FX1_DMA_INT_COMMAND_COMPLETE && (last_core & 1u), 0x11,
              "1: CH0 COMMAND_COMPLETE and CORE_STATUS bit 0");
    fx1_check(same(SRC, DST, 256), 0x12, "1: destination matches source");
    fx1_check((fx1_read32(CH0(FX1_DMA_CH_TRANSFER_COUNT)) & 0xFFFu) == 1 &&
                  fx1_read32(CH0(FX1_DMA_CH_ACTIVE_STATUS)) == 0 &&
                  fx1_read32(DMA(FX1_DMA_CORE_STATUS)) == 0,
              0x13, "1: one command done, channel idle, interrupt cleared");
    fx1_log("1: 256-byte M2M copy, IRQ via PLIC source 2, cleared at the DMA");

    /* 2: maximum transfer, odd alignment. A DMA feature check: on silicon with
     * a non-coherent cache a device-written buffer must cover whole cache lines
     * (fx1_fw.h, plan C12), so the bytes around this destination must not be
     * CPU data there. */
    fill(SRC + 1, FX1_DMA_MAX_TRANSFER, 91);
    run_m2m(SRC + 1, DST + 0x1003, FX1_DMA_MAX_TRANSFER);
    fx1_check(wait_irqs(2), 0x20, "2: completion interrupt");
    fx1_dma_complete((const volatile void*)(DST + 0x1003), FX1_DMA_MAX_TRANSFER, FX1_DMA_FROM_DEVICE);
    fx1_check(same(SRC + 1, DST + 0x1003, FX1_DMA_MAX_TRANSFER), 0x21, "2: 1023-byte unaligned copy");
    fx1_log("2: 1023-byte copy with odd alignment");

    /* 3: destination unmapped -> write decode error */
    run_m2m(SRC, UNMAPPED, 64);
    fx1_check(wait_irqs(3), 0x30, "3: error interrupt");
    fx1_check(last_status & FX1_DMA_INT_WRITE_DECERR, 0x31, "3: WRITE_DECERR reported");
    fx1_check(!(last_status & FX1_DMA_INT_COMMAND_COMPLETE), 0x32, "3: failed command is no completion");
    fx1_log_hex("3: unmapped destination -> interrupt status", last_status);

    /* 4: sub-word CSR access is a guest access fault */
    uint32_t value = 0;
    const uint32_t csr = DMA(FX1_DMA_CORE_STATUS);
    __asm__ volatile("lb %0, 0(%1)" : "=r"(value) : "r"(csr) : "memory");
    fx1_check(faults == 1 && fault_cause == 5u, 0x40, "4: byte load from a DMA CSR -> load access fault");
    __asm__ volatile("sh %0, 0(%1)" ::"r"(0u), "r"(DMA(FX1_DMA_CH(0, FX1_DMA_CH_ENABLE))) : "memory");
    fx1_check(faults == 2 && fault_cause == 7u, 0x41, "4: halfword store to a DMA CSR -> store access fault");
    (void)value;
    fx1_log("4: sub-word DMA CSR accesses trap");
    return 0;
}
