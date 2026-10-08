/*
 * FX1 firmware quickstart: the smallest image that exercises what a driver
 * needs on this platform. Copy this directory to start a new test or driver
 * bring-up; see platforms/fx1_soc/docs/FW_GETTING_STARTED.md.
 *
 *   1  hart 0 prints, releases hart 1, which prints and reports back
 *   2  a CLINT timer interrupt on hart 0 (mtimecmp, MTIP)
 *   3  a SYS_DMA M2M copy with its completion interrupt through the PLIC,
 *      with the plan C12 buffer-ownership calls around it
 *   4  return 0 from main(): the simulator prints RESULT PASS and exits 0
 *
 * Registered as the fx1_smoke_example_quickstart test, so it keeps building
 * and passing.
 */
#include "fx1_fw.h"
#include "dma/fx1_dma_regs.h"

#define DMA(offset) (FX1_SYS_DMA_CSR_BASE + (offset))
#define CH0(offset) DMA(FX1_DMA_CH(0, (offset)))
#define SRC         (FX1_FW_TEST_AREA_BASE + 0x0000u) /* buffers live in the FW regions of fx1_memory_map.h */
#define DST         (FX1_FW_TEST_AREA_BASE + 0x1000u)
#define BYTES       256u

static volatile uint32_t hart1_up, timer_irqs, dma_irqs, dma_status;

/* One handler per hart (fx1_set_trap_handler); return the mepc to resume at. */
static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    (void)mtval;
    if (mcause == (FX1_MCAUSE_INTERRUPT | FX1_IRQ_M_TIMER)) {
        fx1_set_mtimecmp(0, ~0ull); /* one shot: push the deadline away, MTIP drops */
        ++timer_irqs;
    } else if (mcause == (FX1_MCAUSE_INTERRUPT | FX1_IRQ_M_EXT)) {
        const uint32_t id = fx1_plic_claim(0);
        if (id == FX1_IRQ_SYS_DMA) {
            dma_status = fx1_read32(CH0(FX1_DMA_CH_INTERRUPT_STATUS));
            fx1_write32(CH0(FX1_DMA_CH_INTERRUPT_CLEAR), dma_status); /* clear at the device ... */
            ++dma_irqs;
        }
        if (id) fx1_plic_complete(0, id); /* ... then complete at the PLIC */
    } else {
        fx1_log_hex("unexpected trap, mcause", mcause);
        fx1_fail(0x0E0);
    }
    return mepc;
}

void secondary_main(uint32_t hart)
{
    fx1_log_hex("hello from hart", hart);
    fx1_release(); /* everything before is visible before the flag */
    hart1_up = 1;
    for (;;) fx1_wfi();
}

static int wait_for(volatile uint32_t* counter, uint32_t value, uint32_t us)
{
    const uint64_t start = fx1_mtime();
    while (*counter < value)
        if (fx1_mtime() - start > (uint64_t)us * (FX1_MTIME_HZ / 1000000u)) return 0;
    return 1;
}

int main(void)
{
    /* 1 */
    fx1_log("hello from hart 0");
    fx1_release_secondaries();
    fx1_check(wait_for(&hart1_up, 1, 10000), 0x10, "hart 1 reports within 10 ms");

    fx1_set_trap_handler(handler);
    fx1_irq_enable(FX1_MIE_MTIE | FX1_MIE_MEIE);
    fx1_global_irq_enable();

    /* 2: timer 100 us from now */
    fx1_set_mtimecmp(0, fx1_mtime() + FX1_MTIME_HZ / 10000u);
    fx1_check(wait_for(&timer_irqs, 1, 1000), 0x20, "timer interrupt");
    fx1_log("timer interrupt taken");

    /* 3: DMA copy, completion interrupt through PLIC source 2 */
    fx1_plic_set_priority(FX1_IRQ_SYS_DMA, 1);
    fx1_plic_enable(0, FX1_IRQ_SYS_DMA, 1);
    for (uint32_t i = 0; i < BYTES; ++i) *(volatile uint8_t*)(SRC + i) = (uint8_t)(i ^ 0x5Au);
    fx1_dma_prepare((const volatile void*)SRC, BYTES, FX1_DMA_TO_DEVICE);   /* CPU -> device */
    fx1_dma_prepare((const volatile void*)DST, BYTES, FX1_DMA_FROM_DEVICE);
    fx1_write32(CH0(FX1_DMA_CH_ENABLE), 0); /* configure while disabled */
    fx1_write32(CH0(FX1_DMA_CH_CMD_READ_ADDR), SRC);
    fx1_write32(CH0(FX1_DMA_CH_CMD_WRITE_ADDR), DST);
    fx1_write32(CH0(FX1_DMA_CH_CMD_TRANSFER_SIZE), BYTES);
    fx1_write32(CH0(FX1_DMA_CH_CMD_CONTROL), FX1_DMA_CMD_SET_INT | FX1_DMA_CMD_LAST);
    fx1_write32(CH0(FX1_DMA_CH_READ_CONFIG), FX1_DMA_ADDRESS_INCREMENT | FX1_DMA_CFG_BURST_BYTES(32));
    fx1_write32(CH0(FX1_DMA_CH_WRITE_CONFIG), FX1_DMA_ADDRESS_INCREMENT | FX1_DMA_CFG_BURST_BYTES(32));
    fx1_write32(CH0(FX1_DMA_CH_ENABLE), 1);
    fx1_write32(CH0(FX1_DMA_CH_START), 1);
    fx1_check(wait_for(&dma_irqs, 1, 1000), 0x30, "DMA completion interrupt");
    fx1_check(dma_status == FX1_DMA_INT_COMMAND_COMPLETE, 0x31, "DMA completed without error");
    fx1_dma_complete((const volatile void*)DST, BYTES, FX1_DMA_FROM_DEVICE); /* device -> CPU */
    for (uint32_t i = 0; i < BYTES; ++i)
        fx1_check(*(volatile uint8_t*)(DST + i) == (uint8_t)(i ^ 0x5Au), 0x32, "DMA data");
    fx1_log("DMA copy verified");

    /* 4 */
    return 0;
}
