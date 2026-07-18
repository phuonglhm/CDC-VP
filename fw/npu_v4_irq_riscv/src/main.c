#include <stdint.h>

#include "soc/regs/soc_regs_npu_v4.h"
#include "soc/soc_irq_map.h"
#include "soc/soc_memory_map.h"

#define UART_TX (*(volatile uint8_t *)(uintptr_t)CDC_UART0_BASE)

#define NPU_M 32u
#define NPU_N 32u
#define NPU_K 64u

#define NPU_SRC_ADDR_VALUE (CDC_VPU_OUT0_BASE)
#define NPU_WGT_ADDR_VALUE (CDC_NPU_WGT0_BASE)
#define NPU_DST_ADDR_VALUE (CDC_NPU_WORK0_BASE)

static volatile uint32_t npu_irq_seen;
static volatile uint32_t npu_irq_status;
static volatile uint32_t npu_status_at_irq;

static inline void write32(uint32_t address, uint32_t value)
{
    *(volatile uint32_t *)(uintptr_t)address = value;
}

static inline uint32_t read32(uint32_t address)
{
    return *(volatile uint32_t *)(uintptr_t)address;
}

static inline void npu_write(uint32_t offset, uint32_t value)
{
    write32(CDC_NPU0_BASE + offset, value);
}

static inline uint32_t npu_read(uint32_t offset)
{
    return read32(CDC_NPU0_BASE + offset);
}

static void uart_putc(char c)
{
    UART_TX = (uint8_t)c;
}

static void uart_puts(const char *s)
{
    while (*s != '\0') {
        uart_putc(*s++);
    }
}

static void uart_put_hex32(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        uart_putc(hex[(value >> shift) & 0xFu]);
    }
}

void __attribute__((interrupt("machine"))) trap_handler(void)
{
    uint32_t mcause;
    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));

    if ((mcause & 0x7FFFFFFFu) == CDC_CAUSE_MEIP) {
        const uint32_t claim =
            read32(CDC_PLIC_BASE + CDC_PLIC_CLAIM);
        if (claim == CDC_IRQ_NPU0) {
            npu_status_at_irq = npu_read(CDC_NPU_STATUS);
            npu_irq_status = npu_read(CDC_NPU_IRQ_STATUS);

            /* Device cause first, PLIC completion second: level IRQ policy. */
            npu_write(CDC_NPU_IRQ_STATUS,
                      npu_irq_status & (CDC_NPU_IRQ_DONE |
                                        CDC_NPU_IRQ_ERROR));
            npu_irq_seen = 1u;
        }
        write32(CDC_PLIC_BASE + CDC_PLIC_CLAIM, claim);
    }
}

int main(void)
{
    volatile int8_t *const activations =
        (volatile int8_t *)(uintptr_t)NPU_SRC_ADDR_VALUE;
    volatile int8_t *const weights =
        (volatile int8_t *)(uintptr_t)NPU_WGT_ADDR_VALUE;
    volatile int32_t *const output =
        (volatile int32_t *)(uintptr_t)NPU_DST_ADDR_VALUE;

    uart_puts("NPU v4 platform start\n");

    for (uint32_t row = 0; row < NPU_M; ++row) {
        const int8_t a = (int8_t)((row & 3u) + 1u);
        for (uint32_t k = 0; k < NPU_K; ++k) {
            activations[row * NPU_K + k] = a;
        }
    }
    for (uint32_t k = 0; k < NPU_K; ++k) {
        for (uint32_t col = 0; col < NPU_N; ++col) {
            weights[k * NPU_N + col] =
                (int8_t)((int32_t)(col & 3u) - 1);
        }
    }
    for (uint32_t i = 0; i < NPU_M * NPU_N; ++i) {
        output[i] = (int32_t)0x55555555;
    }

    __asm__ volatile("csrw mtvec, %0" : : "r"(trap_handler));

    write32(CDC_PLIC_BASE + CDC_PLIC_PRIORITY(CDC_IRQ_NPU0), 1u);
    write32(CDC_PLIC_BASE + CDC_PLIC_ENABLE,
            1u << CDC_IRQ_NPU0);
    write32(CDC_PLIC_BASE + CDC_PLIC_THRESHOLD, 0u);

    uint32_t tmp;
    __asm__ volatile("csrrs %0, mie, %1"
                     : "=r"(tmp)
                     : "r"(CDC_MIE_MEIE));
    __asm__ volatile("csrrs %0, mstatus, %1"
                     : "=r"(tmp)
                     : "r"(CDC_MSTATUS_MIE));

    npu_write(CDC_NPU_SRC_ADDR, NPU_SRC_ADDR_VALUE);
    npu_write(CDC_NPU_SRC_SIZE_BYTES, NPU_M * NPU_K);
    npu_write(CDC_NPU_SRC_STRIDE_BYTES, NPU_K);
    npu_write(CDC_NPU_WEIGHTS_ADDR, NPU_WGT_ADDR_VALUE);
    npu_write(CDC_NPU_WEIGHTS_SIZE_BYTES, NPU_K * NPU_N);
    npu_write(CDC_NPU_DST_ADDR, NPU_DST_ADDR_VALUE);
    npu_write(CDC_NPU_DST_SIZE_BYTES,
              NPU_M * NPU_N * (uint32_t)sizeof(int32_t));
    npu_write(CDC_NPU_WIDTH, NPU_N);
    npu_write(CDC_NPU_HEIGHT, NPU_M);
    npu_write(CDC_NPU_K_DIMENSION, NPU_K);
    npu_write(CDC_NPU_FORMAT, CDC_NPU_FORMAT_INT8_INT8_INT32);
    npu_write(CDC_NPU_OP_MODE, CDC_NPU_OP_GEMM);
    npu_write(CDC_NPU_IRQ_ENABLE,
              CDC_NPU_IRQ_DONE | CDC_NPU_IRQ_ERROR);
    npu_write(CDC_NPU_CTRL,
              CDC_NPU_CTRL_ENABLE | CDC_NPU_CTRL_IRQ_EN);
    npu_write(CDC_NPU_CTRL,
              CDC_NPU_CTRL_ENABLE | CDC_NPU_CTRL_IRQ_EN |
                  CDC_NPU_CTRL_START);

    while (npu_irq_seen == 0u) {
        __asm__ volatile("wfi");
    }

    uint32_t mismatch = 0u;
    for (uint32_t row = 0; row < NPU_M; ++row) {
        for (uint32_t col = 0; col < NPU_N; ++col) {
            const int32_t expected =
                (int32_t)NPU_K * (int32_t)((row & 3u) + 1u) *
                ((int32_t)(col & 3u) - 1);
            if (output[row * NPU_N + col] != expected) {
                mismatch = row * NPU_N + col + 1u;
                break;
            }
        }
        if (mismatch != 0u) {
            break;
        }
    }

    if ((npu_irq_status & CDC_NPU_IRQ_DONE) != 0u &&
        (npu_irq_status & CDC_NPU_IRQ_ERROR) == 0u &&
        (npu_status_at_irq & CDC_NPU_STATUS_DONE) != 0u &&
        mismatch == 0u) {
        uart_puts("NPU IRQ17\n");
        uart_puts("NPU PASS\n");
    } else {
        uart_puts("NPU FAIL status=");
        uart_put_hex32(npu_status_at_irq);
        uart_puts(" irq=");
        uart_put_hex32(npu_irq_status);
        uart_puts(" mismatch=");
        uart_put_hex32(mismatch);
        uart_puts(" error=");
        uart_put_hex32(npu_read(CDC_NPU_LAST_ERROR));
        uart_puts("\n");
    }

    for (;;) {
        __asm__ volatile("wfi");
    }
}
