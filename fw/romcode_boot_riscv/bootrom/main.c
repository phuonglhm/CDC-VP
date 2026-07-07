/* Minimal ROM code for VP_FX1_Full_SoC (E2E test of the boot-strap flow).
 *
 * Mirrors docs/romcode_boot_hw_plan.md: read GPIO0 VALUE bit 1 (boot strap);
 * LOW  -> jump to the application at IFLASH 0x0400_0000,
 * HIGH -> probe: send a request byte over UART0 and wait for the host tool's
 *         response (VP side: --uart0-rx-file / --uart0-socket). A response
 *         enters the "UART download" loop (here: echo what was received).
 *         SPI0 probe is Phase 3, still a stub.
 */

#define MMIO32(a)      (*(volatile unsigned int *)(a))

#define UART0_BASE     0x10000000u
#define UART_DR        (UART0_BASE + 0x000u)
#define UART_FR        (UART0_BASE + 0x018u)
#define UART_FR_TXFF   (1u << 5)
#define UART_FR_RXFE   (1u << 4)

#define PROBE_REQ      'R'   /* request byte sent to the host tool */

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

/* Wait (bounded busy-poll; no baud modeling on the VP) for an RX byte.
 * Returns -1 on timeout. */
static int uart_getc_timeout(unsigned spins)
{
    while (spins--) {
        if ((MMIO32(UART_FR) & UART_FR_RXFE) == 0)
            return (int)(MMIO32(UART_DR) & 0xFFu);
    }
    return -1;
}

/* "UART bootloader" placeholder: echo the host's bytes until the line goes
 * idle, proving the RX path end to end. */
static void uart_download_loop(int first)
{
    uart_puts("BOOTROM: UART response -> download mode, echo: ");
    int c = first;
    do {
        uart_putc((char)c);
        c = uart_getc_timeout(200000);
    } while (c >= 0);
    uart_puts("\nBOOTROM: UART download done\n");
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
        uart_puts("BOOTROM: strap HIGH -> probe for download\n");
        /* Diagram's probe loop, bounded so a strap-HIGH run without any host
         * backend still terminates (see plan §6: real ROM needs an exit path
         * anyway or the WDT trips). */
        for (unsigned attempt = 0; attempt < 8; ++attempt) {
            uart_putc(PROBE_REQ);           /* a. request over USART0 */
            int c = uart_getc_timeout(200000);
            if (c >= 0) {
                uart_download_loop(c);
                break;
            }
            /* b. SPI0 NOR probe (CMD 0x03) — Phase 3, not modeled yet. */
        }
        uart_puts("BOOTROM: probe loop exit\n");
    }

    for (;;) __asm__ volatile("wfi");
}
