/* Tiny IFLASH application: proves the BOOTROM "strap LOW" path reached
 * 0x0400_0000 and that XIP from the read-only window works. */

#define MMIO32(a)      (*(volatile unsigned int *)(a))

#define UART0_BASE     0x10000000u
#define UART_DR        (UART0_BASE + 0x000u)
#define UART_FR        (UART0_BASE + 0x018u)
#define UART_FR_TXFF   (1u << 5)

static void uart_putc(char c)
{
    while (MMIO32(UART_FR) & UART_FR_TXFF) {}
    MMIO32(UART_DR) = (unsigned char)c;
}

static void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}

void app_main(void)
{
    uart_puts("APP: hello from internal flash (IFLASH @ 0x04000000)\n");
    uart_puts("APP: boot flow PASS\n");
    for (;;) __asm__ volatile("wfi");
}
