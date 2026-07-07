/* Minimal ROM code for VP_FX1_Full_SoC (E2E test of the boot-strap flow).
 *
 * Mirrors docs/romcode_boot_hw_plan.md: read GPIO0 VALUE bit 1 (boot strap);
 * LOW  -> jump to the application at IFLASH 0x0400_0000,
 * HIGH -> announce the UART/SPI download probe (Phase 2/3, not modeled yet).
 */

#define MMIO32(a)      (*(volatile unsigned int *)(a))

#define UART0_BASE     0x10000000u
#define UART_DR        (UART0_BASE + 0x000u)
#define UART_FR        (UART0_BASE + 0x018u)
#define UART_FR_TXFF   (1u << 5)

#define GPIO0_BASE     0x10160000u
#define GPIO_VALUE     (GPIO0_BASE + 0x00u)
#define GPIO_BOOT_PIN  1u

#define IFLASH_BASE    0x04000000u

static void uart_putc(char c)
{
    while (MMIO32(UART_FR) & UART_FR_TXFF) {}
    MMIO32(UART_DR) = (unsigned char)c;
}

static void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}

void rom_main(void)
{
    uart_puts("BOOTROM: reset, reading boot strap GPIO0.1\n");

    unsigned strap = (MMIO32(GPIO_VALUE) >> GPIO_BOOT_PIN) & 1u;
    if (strap == 0u) {
        uart_puts("BOOTROM: strap LOW -> jump to app @ IFLASH 0x04000000\n");
        ((void (*)(void))IFLASH_BASE)();
        uart_puts("BOOTROM: app returned\n");
    } else {
        uart_puts("BOOTROM: strap HIGH -> probe UART0/SPI0 download (not modeled yet)\n");
    }

    for (;;) __asm__ volatile("wfi");
}
