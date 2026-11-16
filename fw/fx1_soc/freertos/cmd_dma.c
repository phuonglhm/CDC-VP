/*
 * `dma test [BYTES]`: one SYS_DMA channel-0 M2M copy driven from a task.
 * Completion through PLIC source 2: the ISR clears at the DMA and gives a
 * semaphore the task waits on; buffers handed over with the plan C12 calls.
 * Source 2 is shared by every channel: CORE_STATUS tells which channel
 * interrupts; channel 1 belongs to the ISP stream (cmd_isp_stream.c).
 */
#include "FreeRTOS.h"
#include "semphr.h"

#include "dma/fx1_dma_regs.h"
#include "fx1_fw.h"
#include "rtos_app.h"
#include "rtos_bsp.h"

#define DMA(offset) (FX1_SYS_DMA_CSR_BASE + (offset))
#define CH0(offset) DMA(FX1_DMA_CH(0, (offset)))
#define SRC         (FX1_FW_TEST_AREA_BASE + 0x100000u)
#define DST         (FX1_FW_TEST_AREA_BASE + 0x110000u)
#define CFG_M2M     (FX1_DMA_ADDRESS_INCREMENT | FX1_DMA_CFG_TOKENS(1) | FX1_DMA_CFG_BURST_BYTES(32))

static SemaphoreHandle_t done;
static BaseType_t ch0_isr(void);
static volatile uint32_t last_status;
static uint32_t seed;

static BaseType_t dma_isr(void)
{
    BaseType_t woken = pdFALSE;
    const uint32_t core = fx1_read32(DMA(FX1_DMA_CORE_STATUS));
    if (core & (1u << 1)) woken = isp_stream_dma_isr();
    if (!(core & ~1u) || (core & 1u)) { /* channel 0, as before (also a source-2 call naming no channel) */
        woken |= ch0_isr();
    }
    return woken;
}

static BaseType_t ch0_isr(void)
{
    BaseType_t woken = pdFALSE;
    last_status = fx1_read32(CH0(FX1_DMA_CH_INTERRUPT_STATUS));
    fx1_write32(CH0(FX1_DMA_CH_INTERRUPT_CLEAR), last_status); /* clear at the device first */
    xSemaphoreGiveFromISR(done, &woken);
    return woken;
}

void dma_service_init(void)
{
    done = xSemaphoreCreateBinary();
    configASSERT(done);
    bsp_irq_register(FX1_IRQ_SYS_DMA, dma_isr);
}

int cmd_dma(int argc, char** argv)
{
    uint32_t bytes = 1023;
    if (argc < 2 || argv[1][0] != 't' || (argc > 2 && (!parse_u32(argv[2], &bytes) || !bytes ||
                                                       bytes > FX1_DMA_MAX_TRANSFER)))
        return 1;
    ++seed;
    for (uint32_t i = 0; i < bytes; ++i) {
        *(volatile uint8_t*)(SRC + i) = (uint8_t)(i * 13u + seed);
        *(volatile uint8_t*)(DST + i) = 0;
    }
    fx1_dma_prepare((const volatile void*)SRC, bytes, FX1_DMA_TO_DEVICE);
    fx1_dma_prepare((const volatile void*)DST, bytes, FX1_DMA_FROM_DEVICE);
    xSemaphoreTake(done, 0); /* drop a stale give */
    last_status = 0;
    fx1_write32(CH0(FX1_DMA_CH_ENABLE), 0);
    fx1_write32(CH0(FX1_DMA_CH_INTERRUPT_CLEAR), FX1_DMA_INTERRUPT_MASK);
    fx1_write32(CH0(FX1_DMA_CH_CMD_READ_ADDR), SRC);
    fx1_write32(CH0(FX1_DMA_CH_CMD_WRITE_ADDR), DST);
    fx1_write32(CH0(FX1_DMA_CH_CMD_TRANSFER_SIZE), bytes);
    fx1_write32(CH0(FX1_DMA_CH_CMD_CONTROL), FX1_DMA_CMD_SET_INT | FX1_DMA_CMD_LAST);
    fx1_write32(CH0(FX1_DMA_CH_READ_CONFIG), CFG_M2M);
    fx1_write32(CH0(FX1_DMA_CH_WRITE_CONFIG), CFG_M2M);
    fx1_write32(CH0(FX1_DMA_CH_PERIPHERAL_CONFIG), 0);
    fx1_write32(CH0(FX1_DMA_CH_MODE_CONFIG), FX1_DMA_MODE_SWAP_NONE);
    fx1_write32(CH0(FX1_DMA_CH_ENABLE), 1);
    const TickType_t t0 = xTaskGetTickCount();
    fx1_write32(CH0(FX1_DMA_CH_START), 1);
    const int irq = xSemaphoreTake(done, pdMS_TO_TICKS(100)) == pdTRUE;
    fx1_dma_complete((const volatile void*)DST, bytes, FX1_DMA_FROM_DEVICE);
    uint32_t bad = 0;
    for (uint32_t i = 0; i < bytes; ++i) bad += *(volatile uint8_t*)(DST + i) != (uint8_t)(i * 13u + seed);
    out("dma: ");
    out_dec(bytes);
    out(" bytes ");
    out_hex(SRC);
    out(" -> ");
    out_hex(DST);
    out(", status ");
    out_hex(last_status);
    out(", ");
    out_dec((uint32_t)(xTaskGetTickCount() - t0));
    out(" ms, mismatches ");
    out_dec(bad);
    out_line("");
    report("dma copy", irq && last_status == FX1_DMA_INT_COMMAND_COMPLETE && bad == 0);
    return 0;
}
