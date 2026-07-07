/* Minimal ROM code for VP_FX1_Full_SoC (E2E test of the boot-strap flow).
 *
 * Mirrors docs/romcode_boot_hw_plan.md: read GPIO0 VALUE bit 1 (boot strap);
 * LOW  -> jump to the application at IFLASH 0x0400_0000,
 * HIGH -> probe: a. send a request byte over UART0 and wait for the host
 *         tool's response (VP side: --uart0-rx-file / --uart0-socket); a
 *         response enters the "UART download" loop (here: echo).
 *         b. else NOR READ (CMD 0x03) over SPI0 (VP side: --spi-flash);
 *         a non-erased image enters "SPI download" (here: dump 8 bytes).
 */

#define MMIO32(a)      (*(volatile unsigned int *)(a))
#define MMIO16(a)      (*(volatile unsigned short *)(a))

#define UART0_BASE     0x10000000u
#define UART_DR        (UART0_BASE + 0x000u)
#define UART_FR        (UART0_BASE + 0x018u)
#define UART_FR_TXFF   (1u << 5)
#define UART_FR_RXFE   (1u << 4)

#define PROBE_REQ      'R'   /* request byte sent to the host tool */

#define SPI0_BASE      0x10020000u   /* PL022 SSP, 16-bit regs */
#define SPI_CR0        (SPI0_BASE + 0x00u)
#define SPI_CR1        (SPI0_BASE + 0x04u)
#define SPI_DR         (SPI0_BASE + 0x08u)
#define SPI_SR         (SPI0_BASE + 0x0Cu)
#define SPI_CPSR       (SPI0_BASE + 0x10u)
#define SPI_CSR        (SPI0_BASE + 0x28u)  /* vendor: bit0 = CS assert */
#define SPI_CR0_DSS8   0x0007u
#define SPI_CR1_SSE    (1u << 1)
#define SPI_SR_TNF     (1u << 1)
#define SPI_SR_RNE     (1u << 2)
#define NOR_CMD_READ   0x03u

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

static void uart_puthex8(unsigned v)
{
    static const char hex[] = "0123456789ABCDEF";
    uart_putc(hex[(v >> 4) & 0xF]);
    uart_putc(hex[v & 0xF]);
}

/* One full-duplex SPI frame; returns the RX byte or -1 on timeout. */
static int spi_xfer(unsigned char b)
{
    unsigned t = 100000;
    while (!(MMIO16(SPI_SR) & SPI_SR_TNF)) if (!--t) return -1;
    MMIO16(SPI_DR) = b;
    t = 100000;
    while (!(MMIO16(SPI_SR) & SPI_SR_RNE)) if (!--t) return -1;
    return (int)(MMIO16(SPI_DR) & 0xFFu);
}

/* NOR READ (CMD 0x03) @0 over SPI0. "A response arrives" on a line with no
 * device would read all-erased/floating; treat anything besides 0xFF/0x00 in
 * the first bytes as a present image (real ROM: validate an image header).
 * Returns 1 and dumps the bytes when found. */
static int spi_probe(void)
{
    MMIO16(SPI_CR0) = SPI_CR0_DSS8;
    MMIO16(SPI_CPSR) = 2;
    MMIO16(SPI_CR1) = SPI_CR1_SSE;
    MMIO16(SPI_CSR) = 1;                       /* CS assert: command begins */

    unsigned char buf[8];
    int ok = spi_xfer(NOR_CMD_READ) >= 0 &&    /* CMD + 24-bit address 0 */
             spi_xfer(0) >= 0 && spi_xfer(0) >= 0 && spi_xfer(0) >= 0;
    int present = 0;
    if (ok) {
        for (unsigned i = 0; i < sizeof(buf); ++i) {
            int c = spi_xfer(0xFF);
            if (c < 0) { ok = 0; break; }
            buf[i] = (unsigned char)c;
            if (c != 0xFF && c != 0x00) present = 1;
        }
    }
    MMIO16(SPI_CSR) = 0;                       /* CS deassert: command ends */

    if (!ok || !present) return 0;
    uart_puts("BOOTROM: SPI flash answers -> SPI download mode, first bytes: ");
    for (unsigned i = 0; i < sizeof(buf); ++i) uart_puthex8(buf[i]);
    uart_puts("\nBOOTROM: SPI download done\n");
    return 1;
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
            if (spi_probe())                /* b. NOR READ over SPI0 */
                break;
        }
        uart_puts("BOOTROM: probe loop exit\n");
    }

    for (;;) __asm__ volatile("wfi");
}
