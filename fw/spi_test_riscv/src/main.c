//Author: QuanNH107
//Verified by: HoangV11


/* SPI SoC integration firmware.
 *
 * Expected path:
 *   CPU -> bus_router -> SPI MMIO
 *   SPI irq -> PLIC source 3 -> CPU machine external interrupt
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
#define SPI_RIS_ROR           (1u << 0)
#define SPI_RIS_RT            (1u << 1)
#define SPI_RIS_RX            (1u << 2)
#define SPI_RIS_TX            (1u << 3)
#define SPI_IMSC_RORIM        SPI_RIS_ROR
#define SPI_IMSC_RTIM         SPI_RIS_RT
#define SPI_IMSC_RXIM         SPI_RIS_RX
#define SPI_ICR_RORIC         SPI_RIS_ROR
#define SPI_ICR_RTIC          SPI_RIS_RT

#define SPI_TEST_WORD_BASE    0xAAu
#define SPI_TEST_WORD_COUNT   4u
#define SPI_RESET_SR_VALUE    (SPI_SR_TFE | SPI_SR_TNF)
#define SPI_RESET_RIS_VALUE   SPI_RIS_TX

#define MIE_MEIE           (1u << 11)
#define MSTATUS_MIE        (1u << 3)

static volatile unsigned spi_irq_done;
static volatile unsigned spi_irq_claimed;
static volatile unsigned spi_irq_status;
static volatile unsigned spi_irq_error;
static volatile unsigned spi_rx_count;
static volatile unsigned spi_rx_values[SPI_TEST_WORD_COUNT];

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
    const unsigned value = MMIO16(addr) & 0xFFFFu;
    uart_puts("READ  ");
    uart_puts(name);
    uart_puts(" [");
    uart_put_hex32(addr);
    uart_puts("] => ");
    uart_put_hex32(value);
    uart_puts("\n");
    return value;
}

static unsigned expected_word(unsigned index)
{
    return (SPI_TEST_WORD_BASE + index) & 0xFFu;
}

static unsigned check_eq(const char *name, unsigned actual, unsigned expected)
{
    if (actual == expected) {
        uart_puts("  PASS ");
        uart_puts(name);
        uart_puts(" = ");
        uart_put_hex32(actual);
        uart_puts("\n");
        return 0u;
    }

    uart_puts("  FAIL ");
    uart_puts(name);
    uart_puts(" expected ");
    uart_put_hex32(expected);
    uart_puts(" got ");
    uart_put_hex32(actual);
    uart_puts("\n");
    return 1u;
}

static void short_delay(void)
{
    for (volatile unsigned i = 0; i < 128u; ++i) {
        __asm__ volatile("nop");
    }
}

static unsigned phase1_check_reset_defaults(void)
{
    unsigned failures = 0u;

    uart_puts("PHASE 1 START: power-on reset defaults\n");
    failures += check_eq("SPI_CR0 reset", mmio_read16("SPI_CR0", SPI_CR0_ADDR), 0u);
    failures += check_eq("SPI_CR1 reset", mmio_read16("SPI_CR1", SPI_CR1_ADDR), 0u);
    failures += check_eq("SPI_SR reset", mmio_read16("SPI_SR", SPI_SR_ADDR), SPI_RESET_SR_VALUE);
    failures += check_eq("SPI_IMSC reset", mmio_read16("SPI_IMSC", SPI_IMSC_ADDR), 0u);
    failures += check_eq("SPI_RIS reset", mmio_read16("SPI_RIS", SPI_RIS_ADDR), SPI_RESET_RIS_VALUE);
    failures += check_eq("SPI_MIS reset", mmio_read16("SPI_MIS", SPI_MIS_ADDR), 0u);

    if (failures == 0u) {
        uart_puts("PHASE 1 PASS\n");
        return 1u;
    }

    uart_puts("PHASE 1 FAIL\n");
    return 0u;
}

static void configure_plic(void)
{
    mmio_write32("PLIC_PRIORITY3", PLIC_PRIORITY3_ADDR, 1u);
    mmio_write32("PLIC_ENABLE", PLIC_ENABLE_ADDR, (1u << PLIC_SOURCE_SPI));
    mmio_write32("PLIC_THRESHOLD", PLIC_THRESHOLD_ADDR, 0u);
}

static void enable_machine_external_interrupts(void)
{
    unsigned tmp;
    __asm__ volatile("csrrs %0, mie, %1" : "=r"(tmp) : "r"(MIE_MEIE));
    __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE));
}

static void configure_spi(void)
{
    mmio_write16("SPI_CR1", SPI_CR1_ADDR, 0u);
    mmio_write16("SPI_ICR", SPI_ICR_ADDR, SPI_ICR_RORIC | SPI_ICR_RTIC);
    mmio_write16("SPI_CR0", SPI_CR0_ADDR, SPI_CR0_DSS_8BIT);
    mmio_write16("SPI_CPSR", SPI_CPSR_ADDR, 2u);
    mmio_write16("SPI_IMSC", SPI_IMSC_ADDR, SPI_IMSC_RXIM | SPI_IMSC_RORIM | SPI_IMSC_RTIM);
    mmio_write16("SPI_CR1", SPI_CR1_ADDR, SPI_CR1_SSE);
}

static unsigned phase2_loopback_data(void)
{
    unsigned failures = 0u;

    uart_puts("PHASE 2 START: 4-word loopback data\n");

    spi_irq_done = 0u;
    spi_irq_claimed = 0u;
    spi_irq_status = 0u;
    spi_irq_error = 0u;
    spi_rx_count = 0u;
    for (unsigned i = 0; i < SPI_TEST_WORD_COUNT; ++i) {
        spi_rx_values[i] = 0xFFFFFFFFu;
    }

    for (unsigned i = 0; i < SPI_TEST_WORD_COUNT; ++i) {
        mmio_write16("SPI_DR", SPI_DR_ADDR, expected_word(i));
    }

    for (volatile unsigned timeout = 0u; !spi_irq_done && timeout < 1000000u; ++timeout) {
        __asm__ volatile("nop");
    }

    if (!spi_irq_done) {
        uart_puts("  FAIL SPI interrupt timeout\n");
        failures += 1u;
    } else {
        failures += check_eq("PLIC claimed source", spi_irq_claimed, PLIC_SOURCE_SPI);
        failures += check_eq("SPI RX word count", spi_rx_count, SPI_TEST_WORD_COUNT);
        failures += check_eq("SPI IRQ error flags", spi_irq_error, 0u);

        for (unsigned i = 0; i < SPI_TEST_WORD_COUNT; ++i) {
            failures += check_eq("SPI RX data", spi_rx_values[i] & 0xFFu, expected_word(i));
        }
    }

    if (failures == 0u) {
        uart_puts("PHASE 2 PASS\n");
        return 1u;
    }

    uart_puts("PHASE 2 FAIL\n");
    return 0u;
}

static unsigned phase3_irq_clear_status(void)
{
    unsigned failures = 0u;

    uart_puts("PHASE 3 START: IRQ clear and status\n");
    short_delay();

    const unsigned mis = mmio_read16("SPI_MIS", SPI_MIS_ADDR);
    const unsigned ris = mmio_read16("SPI_RIS", SPI_RIS_ADDR);
    const unsigned sr = mmio_read16("SPI_SR", SPI_SR_ADDR);

    failures += check_eq("SPI_MIS after ICR", mis, 0u);
    failures += check_eq("SPI_RIS idle raw status", ris, SPI_RESET_RIS_VALUE);
    failures += check_eq("SPI_RIS RX/error bits", ris & (SPI_RIS_RX | SPI_RIS_ROR | SPI_RIS_RT), 0u);
    failures += check_eq("SPI_SR RNE after drain", sr & SPI_SR_RNE, 0u);

    if (failures == 0u) {
        uart_puts("PHASE 3 PASS\n");
        return 1u;
    }

    uart_puts("PHASE 3 FAIL\n");
    return 0u;
}

void __attribute__((interrupt("machine"))) trap_handler(void)
{
    unsigned mcause;
    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));

    if ((mcause & 0x7FFFFFFFu) == 11u) {
        const unsigned id = mmio_read32("PLIC_CLAIM", PLIC_CLAIM_ADDR);
        spi_irq_claimed = id;

        if (id == PLIC_SOURCE_SPI) {
            spi_irq_status = mmio_read16("SPI_SR", SPI_SR_ADDR);
            spi_rx_count = 0u;
            spi_irq_error = 0u;

            for (unsigned i = 0; i < SPI_TEST_WORD_COUNT; ++i) {
                const unsigned sr = mmio_read16("SPI_SR", SPI_SR_ADDR);
                if ((sr & SPI_SR_RNE) == 0u) {
                    spi_irq_error |= 1u;
                    break;
                }

                const unsigned rx = mmio_read16("SPI_DR", SPI_DR_ADDR) & 0xFFu;
                spi_rx_values[i] = rx;
                spi_rx_count++;
                if (rx != expected_word(i)) {
                    spi_irq_error |= (1u << (i + 4u));
                }
            }

            mmio_write16("SPI_ICR", SPI_ICR_ADDR, SPI_ICR_RORIC | SPI_ICR_RTIC);
            spi_irq_done = 1u;
            uart_puts("SPI IRQ handled, SR=");
            uart_put_hex32(spi_irq_status);
            uart_puts("\n");
        } else {
            spi_irq_error = 0x80000000u;
        }

        mmio_write32("PLIC_CLAIM", PLIC_CLAIM_ADDR, id);
    }
}

int main(void)
{
    uart_puts("SPI platform start\n");

    __asm__ volatile("csrw mtvec, %0" ::"r"(trap_handler));

    const unsigned phase1_pass = phase1_check_reset_defaults();

    configure_plic();
    configure_spi();
    enable_machine_external_interrupts();

    const unsigned phase2_pass = phase2_loopback_data();
    const unsigned phase3_pass = phase3_irq_clear_status();

    if (phase1_pass && phase2_pass && phase3_pass) {
        uart_puts("SPI PASS\n");
    } else {
        uart_puts("SPI FAIL\n");
    }

    for (;;) {
        __asm__ volatile("wfi");
    }

    return 0;
}
