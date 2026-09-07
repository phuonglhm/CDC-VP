#include <stdint.h>

#define MMIO32(address) (*(volatile uint32_t *)(uintptr_t)(address))

#define UART_TX_ADDRESS 0x10000000u
#define TIMER_BASE      0x10010000u

#define TIMER_CTRL      MMIO32(TIMER_BASE + 0x00u)
#define TIMER_LOAD      MMIO32(TIMER_BASE + 0x04u)
#define TIMER_VALUE     MMIO32(TIMER_BASE + 0x08u)
#define TIMER_STATUS    MMIO32(TIMER_BASE + 0x0Cu)
#define TIMER_ID        MMIO32(TIMER_BASE + 0xFCu)

#define CTRL_ENABLE     (1u << 0)
#define CTRL_IRQ_ENABLE (1u << 2)
#define STATUS_PENDING  (1u << 0)
#define EXPECTED_ID     0x45445554u

#define MIE_MTIE        (1u << 7)
#define MSTATUS_MIE     (1u << 3)

static volatile uint32_t interrupt_seen;

static void uart_putc(char value)
{
    *(volatile uint8_t *)(uintptr_t)UART_TX_ADDRESS = (uint8_t)value;
}

static void uart_puts(const char *text)
{
    while (*text != '\0') {
        uart_putc(*text++);
    }
}

void __attribute__((interrupt("machine"))) trap_handler(void)
{
    uint32_t mcause;
    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));

    if (mcause == 0x80000007u && (TIMER_STATUS & STATUS_PENDING) != 0u) {
        TIMER_STATUS = STATUS_PENDING; /* W1C and lower the IRQ level. */
        interrupt_seen = 1u;
        uart_puts("FW: IRQ PASS\n");
    } else {
        uart_puts("FW: unexpected trap\n");
        for (;;) { }
    }
}

int main(void)
{
    unsigned csr_value;

    uart_puts("FW: start\n");

    if (TIMER_ID != EXPECTED_ID) {
        uart_puts("FW: ID FAIL\n");
        for (;;) { }
    }
    uart_puts("FW: ID PASS\n");

    TIMER_LOAD = 5u;
    if (TIMER_LOAD != 5u || TIMER_VALUE != 5u) {
        uart_puts("FW: RW FAIL\n");
        for (;;) { }
    }
    uart_puts("FW: RW PASS\n");

    __asm__ volatile("csrw mtvec, %0" : : "r"(trap_handler));
    __asm__ volatile("csrrs %0, mie, %1"
                     : "=r"(csr_value) : "r"(MIE_MTIE));
    __asm__ volatile("csrrs %0, mstatus, %1"
                     : "=r"(csr_value) : "r"(MSTATUS_MIE));

    interrupt_seen = 0u;
    TIMER_CTRL = CTRL_ENABLE | CTRL_IRQ_ENABLE;
    while (interrupt_seen == 0u) { }

    if ((TIMER_STATUS & STATUS_PENDING) != 0u) {
        uart_puts("FW: W1C FAIL\n");
        for (;;) { }
    }

    uart_puts("FW: ALL PASS\n");
    for (;;) { }
}

