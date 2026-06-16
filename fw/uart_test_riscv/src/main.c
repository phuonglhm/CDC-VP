#define UART_BASE             0x10000000u
#define UART_DR_ADDR          (UART_BASE + 0x000u)
#define UART_FR_ADDR          (UART_BASE + 0x018u)

#define MMIO32(addr)          (*(volatile unsigned int *)(addr))

#define UART_FR_TXFF          (1u << 5)

static void uart_wait_tx(void)
{
    while (MMIO32(UART_FR_ADDR) & UART_FR_TXFF);
}

static void uart_putc_raw(char c)
{
    uart_wait_tx();
    MMIO32(UART_DR_ADDR) = (unsigned char)c;
}

static void uart_puts_raw(const char *s)
{
    while (*s) uart_putc_raw(*s++);
}

int main(void)
{
    uart_puts_raw("UART platform start\n");
    uart_puts_raw("UART TX test\n");

    const unsigned char test_byte = 0x55u;
    uart_wait_tx();
    MMIO32(UART_DR_ADDR) = test_byte;
    uart_wait_tx();

    uart_puts_raw("UART PASS\n");

    for (;;) {
        __asm__ volatile("wfi");
    }
    return 0;
}