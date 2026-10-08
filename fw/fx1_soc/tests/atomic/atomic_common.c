#include "atomic_common.h"

#define CFG_M2M (FX1_DMA_ADDRESS_INCREMENT | FX1_DMA_CFG_TOKENS(1) | FX1_DMA_CFG_BURST_BYTES(32))

void at_dma_start(uint32_t src, uint32_t dst, uint32_t bytes, uint32_t pad_bytes)
{
    fx1_write32(AT_CH0(FX1_DMA_CH_ENABLE), 0);
    fx1_write32(AT_CH0(FX1_DMA_CH_INTERRUPT_CLEAR), FX1_DMA_INTERRUPT_MASK);
    if (pad_bytes) {
        /* Command 1 (registers): padding copy, chained to command 2 (memory). */
        volatile uint32_t* desc = (volatile uint32_t*)AT_DESC;
        desc[0] = src;
        desc[1] = dst;
        desc[2] = bytes;
        desc[3] = FX1_DMA_CMD_SET_INT | FX1_DMA_CMD_LAST;
        fx1_dma_prepare((const volatile void*)AT_DESC, 16, FX1_DMA_TO_DEVICE);
        fx1_dma_prepare((const volatile void*)AT_PAD_SRC, pad_bytes, FX1_DMA_TO_DEVICE);
        fx1_dma_prepare((const volatile void*)AT_PAD_DST, pad_bytes, FX1_DMA_FROM_DEVICE);
        fx1_write32(AT_CH0(FX1_DMA_CH_CMD_READ_ADDR), AT_PAD_SRC);
        fx1_write32(AT_CH0(FX1_DMA_CH_CMD_WRITE_ADDR), AT_PAD_DST);
        fx1_write32(AT_CH0(FX1_DMA_CH_CMD_TRANSFER_SIZE), pad_bytes);
        fx1_write32(AT_CH0(FX1_DMA_CH_CMD_CONTROL), AT_DESC); /* next = AT_DESC, not last, no interrupt */
    } else {
        fx1_write32(AT_CH0(FX1_DMA_CH_CMD_READ_ADDR), src);
        fx1_write32(AT_CH0(FX1_DMA_CH_CMD_WRITE_ADDR), dst);
        fx1_write32(AT_CH0(FX1_DMA_CH_CMD_TRANSFER_SIZE), bytes);
        fx1_write32(AT_CH0(FX1_DMA_CH_CMD_CONTROL), FX1_DMA_CMD_SET_INT | FX1_DMA_CMD_LAST);
    }
    fx1_write32(AT_CH0(FX1_DMA_CH_READ_CONFIG), CFG_M2M);
    fx1_write32(AT_CH0(FX1_DMA_CH_WRITE_CONFIG), CFG_M2M);
    fx1_write32(AT_CH0(FX1_DMA_CH_PERIPHERAL_CONFIG), 0);
    fx1_write32(AT_CH0(FX1_DMA_CH_MODE_CONFIG), FX1_DMA_MODE_SWAP_NONE);
    fx1_write32(AT_CH0(FX1_DMA_CH_ENABLE), 1);
    fx1_dma_prepare((const volatile void*)src, bytes, FX1_DMA_TO_DEVICE);
    fx1_dma_prepare((const volatile void*)dst, bytes, FX1_DMA_FROM_DEVICE);
    fx1_write32(AT_CH0(FX1_DMA_CH_START), 1);
}

int at_dma_done(void)
{
    const uint32_t status = fx1_read32(AT_CH0(FX1_DMA_CH_INTERRUPT_STATUS));
    if (status & ~FX1_DMA_INT_COMMAND_COMPLETE) {
        fx1_log_hex("DMA interrupt status", status);
        fx1_fail(0x0D0);
    }
    return status != 0;
}

void at_dma_ack(void)
{
    fx1_write32(AT_CH0(FX1_DMA_CH_INTERRUPT_CLEAR), FX1_DMA_INTERRUPT_MASK);
}

uint32_t at_plic_drain(uint32_t ctx)
{
    const uint32_t id = fx1_plic_claim(ctx);
    if (id) fx1_plic_complete(ctx, id);
    return id;
}
