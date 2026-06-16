/* ADC SoC verification firmware.
 *
 * Expected path:
 *   CPU -> bus_router -> ADC MMIO
 *   ADC EOC -> PLIC source 1 -> CPU machine external interrupt
 */

#define UART_TX            (*(volatile unsigned char *)0x10000000u)

#define PLIC_PRIORITY1_ADDR   0x0C000004u
#define PLIC_ENABLE_ADDR      0x0C002000u
#define PLIC_THRESHOLD_ADDR   0x0C200000u
#define PLIC_CLAIM_ADDR       0x0C200004u

#define ADC_CONTROL_ADDR      0x10060000u
#define ADC_STATUS_ADDR       0x10060004u
#define ADC_DATA_ADDR         0x10060008u
#define ADC_INTR_ENABLE_ADDR  0x1006000Cu

#define MMIO32(addr)       (*(volatile unsigned int *)(addr))

#define ADC_CTRL_START     (1u << 0)
#define ADC_CTRL_EN        (1u << 1)
#define ADC_STATUS_EOC     (1u << 0)
#define ADC_INTR_EOC       (1u << 0)

#define MIE_MEIE           (1u << 11)
#define MSTATUS_MIE        (1u << 3)

static volatile unsigned adc_irq_done = 0;
static volatile unsigned adc_sample = 0;

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

void __attribute__((interrupt("machine"))) trap_handler(void)
{
    unsigned mcause;
    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));

    if ((mcause & 0x7FFFFFFFu) == 11u) {
        const unsigned id = mmio_read32("PLIC_CLAIM", PLIC_CLAIM_ADDR);
        if (id == 1u) {
            adc_sample = mmio_read32("ADC_DATA", ADC_DATA_ADDR);
            adc_irq_done = 1u;
            uart_puts("ADC IRQ\n");
            uart_puts("ADC sample=");
            uart_put_hex32(adc_sample);
            uart_puts("\n");
        }
        mmio_write32("PLIC_CLAIM", PLIC_CLAIM_ADDR, id);
    }
}

int main(void)
{
    uart_puts("ADC platform start\n");

    __asm__ volatile("csrw mtvec, %0" ::"r"(trap_handler));

    mmio_write32("PLIC_PRIORITY1", PLIC_PRIORITY1_ADDR, 1u);
    mmio_write32("PLIC_ENABLE", PLIC_ENABLE_ADDR, (1u << 1)); /* enable source id 1 */
    mmio_write32("PLIC_THRESHOLD", PLIC_THRESHOLD_ADDR, 0u);

    unsigned tmp;
    __asm__ volatile("csrrs %0, mie, %1" : "=r"(tmp) : "r"(MIE_MEIE));
    __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE));

    mmio_write32("ADC_INTR_ENABLE", ADC_INTR_ENABLE_ADDR, ADC_INTR_EOC);
    mmio_write32("ADC_CONTROL", ADC_CONTROL_ADDR, ADC_CTRL_EN | ADC_CTRL_START);

    while (!adc_irq_done) {
        __asm__ volatile("wfi");
    }

    const unsigned adc_status = mmio_read32("ADC_STATUS", ADC_STATUS_ADDR);
    if ((adc_status & ADC_STATUS_EOC) == 0u && adc_sample <= 0x0FFFu) {
        uart_puts("ADC PASS\n");
    } else {
        uart_puts("ADC FAIL\n");
    }

    for (;;) {
        __asm__ volatile("wfi");
    }

    return 0;
}
