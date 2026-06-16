/* SPI SoC verification firmware.
 *
 * Expected path:
 *   CPU -> bus_router -> SPI MMIO
 *   SPI intr -> PLIC source 3 -> CPU machine external interrupt
 *   SPI to_peri_socket -> platform loopback placeholder -> SPI RX FIFO
 */

#define UART_TX            (*(volatile unsigned char *)0x10000000u)

#define PLIC_BASE             0x0C000000u
#define PLIC_PRIORITY3_ADDR   (PLIC_BASE + 0x00000Cu)
#define PLIC_ENABLE_ADDR      (PLIC_BASE + 0x02000u)
#define PLIC_THRESHOLD_ADDR   (PLIC_BASE + 0x200000u)
#define PLIC_CLAIM_ADDR       (PLIC_BASE + 0x200004u)
#define PLIC_SOURCE_SPI       3u

#define SPI_BASE              0x10020000u
#define SPI_CR0_ADDR          (SPI_BASE + 0x000u)
#define SPI_CR1_ADDR          (SPI_BASE + 0x004u)
#define SPI_DR_ADDR           (SPI_BASE + 0x008u)
#define SPI_SR_ADDR           (SPI_BASE + 0x00Cu)
#define SPI_CPSR_ADDR         (SPI_BASE + 0x010u)
#define SPI_IMSC_ADDR         (SPI_BASE + 0x014u)
#define SPI_RIS_ADDR          (SPI_BASE + 0x018u)
#define SPI_MIS_ADDR          (SPI_BASE + 0x01Cu)
#define SPI_ICR_ADDR          (SPI_BASE + 0x020u)

#define MMIO32(addr)       (*(volatile unsigned int *)(addr))
#define MMIO16(addr)       (*(volatile unsigned short *)(addr))

#define SPI_CR0_DSS_8BIT      0x0007u
#define SPI_CR1_SSE           (1u << 1)
#define SPI_SR_TFE            (1u << 0)
#define SPI_SR_TNF            (1u << 1)
#define SPI_SR_RNE            (1u << 2)
#define SPI_SR_RFF            (1u << 3)
#define SPI_SR_BSY            (1u << 4)
#define SPI_IMSC_RORIM        (1u << 0)
#define SPI_IMSC_RTIM         (1u << 1)
#define SPI_IMSC_RXIM         (1u << 2)
#define SPI_IMSC_TXIM         (1u << 3)
#define SPI_ICR_RORIC         (1u << 0)
#define SPI_ICR_RTIC          (1u << 1)

#define MIE_MEIE           (1u << 11)
#define MSTATUS_MIE        (1u << 3)

static volatile unsigned spi_irq_done = 0;
static volatile unsigned test_passed = 0;
static volatile unsigned spi_rx_value = 0;
static volatile unsigned spi_status_value = 0;
static const unsigned spi_tx_value = 0xAAu;
static const unsigned spi_tx_count = 4u;

static void uart_putc(char c)
{
    UART_TX = (unsigned char)c;
}

static void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

static void uart_put_hex32(unsigned value)
{
    static const char hex[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        uart_putc(hex[(value >> shift) & 0xFu]);
    }
}

static void mmio_write32(const char *name, unsigned addr, unsigned value)
{
    unsigned old_mstatus;
    __asm__ volatile("csrrc %0, mstatus, %1" : "=r"(old_mstatus) : "r"(MSTATUS_MIE) : "memory");

    MMIO32(addr) = value;
    uart_puts("WRITE ");
    uart_puts(name);
    uart_puts(" [");
    uart_put_hex32(addr);
    uart_puts("] <= ");
    uart_put_hex32(value);
    uart_puts("\n");

    if ((old_mstatus & MSTATUS_MIE) != 0u) {
        unsigned tmp;
        __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE) : "memory");
    }
}

static unsigned mmio_read32(const char *name, unsigned addr)
{
    const unsigned value = MMIO32(addr);
    uart_puts("READ  ");
    uart_puts(name);
    uart_puts(" [");
    uart_put_hex32(addr);
    uart_puts("] => ");
    uart_put_hex32(value);
    uart_puts("\n");
    return value;
}

static void mmio_write16(const char *name, unsigned addr, unsigned value)
{
    unsigned old_mstatus;
    __asm__ volatile("csrrc %0, mstatus, %1" : "=r"(old_mstatus) : "r"(MSTATUS_MIE) : "memory");

    MMIO16(addr) = (unsigned short)value;
    uart_puts("WRITE ");
    uart_puts(name);
    uart_puts(" [");
    uart_put_hex32(addr);
    uart_puts("] <= ");
    uart_put_hex32(value & 0xFFFFu);
    uart_puts("\n");

    if ((old_mstatus & MSTATUS_MIE) != 0u) {
        unsigned tmp;
        __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE) : "memory");
    }
}

static unsigned mmio_read16(const char *name, unsigned addr)
{
    const unsigned value = MMIO16(addr);
    uart_puts("READ  ");
    uart_puts(name);
    uart_puts(" [");
    uart_put_hex32(addr);
    uart_puts("] => ");
    uart_put_hex32(value & 0xFFFFu);
    uart_puts("\n");
    return value & 0xFFFFu;
}

void __attribute__((interrupt("machine"))) trap_handler(void)
{
    unsigned mcause;
    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));

    if ((mcause & 0x7FFFFFFFu) == 11u) {
        const unsigned id = mmio_read32("PLIC_CLAIM", PLIC_CLAIM_ADDR);
        if (id == PLIC_SOURCE_SPI) {
            spi_status_value = mmio_read16("SPI_SR", SPI_SR_ADDR);
            if ((spi_status_value & SPI_SR_RNE) != 0u) {
                spi_rx_value = mmio_read16("SPI_DR", SPI_DR_ADDR) & 0xFFu;
                test_passed = (spi_rx_value == (spi_tx_value & 0xFFu));
            } else {
                spi_rx_value = 0xFFFFFFFFu;
                test_passed = 0u;
            }

            mmio_write16("SPI_ICR", SPI_ICR_ADDR, SPI_ICR_RORIC | SPI_ICR_RTIC);
            spi_irq_done = 1u;
            uart_puts("SPI IRQ\n");
            uart_puts("SPI status=");
            uart_put_hex32(spi_status_value);
            uart_puts("\n");
            uart_puts("SPI rx=");
            uart_put_hex32(spi_rx_value);
            uart_puts("\n");
        }
        mmio_write32("PLIC_CLAIM", PLIC_CLAIM_ADDR, id);
    }
}

int main(void)
{
    uart_puts("SPI platform start\n");

    __asm__ volatile("csrw mtvec, %0" ::"r"(trap_handler));

    mmio_write32("PLIC_PRIORITY3", PLIC_PRIORITY3_ADDR, 1u);
    mmio_write32("PLIC_ENABLE", PLIC_ENABLE_ADDR, (1u << PLIC_SOURCE_SPI));
    mmio_write32("PLIC_THRESHOLD", PLIC_THRESHOLD_ADDR, 0u);

    unsigned tmp;
    __asm__ volatile("csrrs %0, mie, %1" : "=r"(tmp) : "r"(MIE_MEIE));
    __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE));

    mmio_write16("SPI_CR1", SPI_CR1_ADDR, 0u);
    mmio_write16("SPI_ICR", SPI_ICR_ADDR, SPI_ICR_RORIC | SPI_ICR_RTIC);
    mmio_write16("SPI_CR0", SPI_CR0_ADDR, SPI_CR0_DSS_8BIT);
    mmio_write16("SPI_CPSR", SPI_CPSR_ADDR, 2u);
    mmio_write16("SPI_IMSC", SPI_IMSC_ADDR, SPI_IMSC_RXIM | SPI_IMSC_RORIM | SPI_IMSC_RTIM);
    mmio_write16("SPI_CR1", SPI_CR1_ADDR, SPI_CR1_SSE);

    uart_puts("SPI tx=");
    uart_put_hex32(spi_tx_value);
    uart_puts("\n");
    for (unsigned i = 0; i < spi_tx_count; ++i) {
        mmio_write16("SPI_DR", SPI_DR_ADDR, spi_tx_value + i);
    }

    while (!spi_irq_done) {
        __asm__ volatile("wfi");
    }

    uart_puts("SPI final status=");
    uart_put_hex32(spi_status_value);
    uart_puts("\n");
    uart_puts("SPI final rx=");
    uart_put_hex32(spi_rx_value);
    uart_puts("\n");

    if (test_passed) {
        uart_puts("SPI PASS\n");
    } else {
        uart_puts("SPI FAIL\n");
    }

    for (;;) {
        __asm__ volatile("wfi");
    }

    return 0;
}
