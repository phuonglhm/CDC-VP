/* PMU SoC verification firmware.
 *
 * Expected path:
 *   CPU -> bus_router -> PMU MMIO
 *   PMU wakeup_irq -> PLIC source 1 -> CPU machine external interrupt
 */

#define UART_TX            (*(volatile unsigned char *)0x10000000u)

#define PLIC_PRIORITY1_ADDR   0x0C000004u
#define PLIC_ENABLE_ADDR      0x0C002000u
#define PLIC_THRESHOLD_ADDR   0x0C200000u
#define PLIC_CLAIM_ADDR       0x0C200004u

#define PMU_BASE              0x10070000u

#define PMU_INTR_STATE_ADDR   (PMU_BASE + 0x00u)
#define PMU_INTR_ENABLE_ADDR  (PMU_BASE + 0x04u)
#define PMU_INTR_TEST_ADDR    (PMU_BASE + 0x08u)
#define PMU_CONTROL_ADDR      (PMU_BASE + 0x14u)
#define PMU_WAKEUP_EN_ADDR    (PMU_BASE + 0x20u)
#define PMU_WAKE_STATUS_ADDR  (PMU_BASE + 0x24u)

#define MMIO32(addr)          (*(volatile unsigned int *)(addr))

#define PMU_INTR_WAKEUP       (1u << 0)
#define PMU_CONTROL_DEFAULT   0x180u

#define MIE_MEIE              (1u << 11)
#define MSTATUS_MIE           (1u << 3)

static volatile unsigned pmu_irq_done = 0;

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

    __asm__ volatile(
        "csrrc %0, mstatus, %1"
        : "=r"(old_mstatus)
        : "r"(MSTATUS_MIE)
        : "memory"
    );

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

        __asm__ volatile(
            "csrrs %0, mstatus, %1"
            : "=r"(tmp)
            : "r"(MSTATUS_MIE)
            : "memory"
        );
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
            uart_puts("PMU IRQ\n");

            mmio_write32("PMU_INTR_STATE", PMU_INTR_STATE_ADDR, PMU_INTR_WAKEUP);

            pmu_irq_done = 1u;
        }

        mmio_write32("PLIC_CLAIM", PLIC_CLAIM_ADDR, id);
    }
}

int main(void)
{
    uart_puts("PMU platform start\n");

    __asm__ volatile("csrw mtvec, %0" :: "r"(trap_handler));

    mmio_write32("PLIC_PRIORITY1", PLIC_PRIORITY1_ADDR, 1u);
    mmio_write32("PLIC_ENABLE", PLIC_ENABLE_ADDR, (1u << 1)); /* enable source id 1 */
    mmio_write32("PLIC_THRESHOLD", PLIC_THRESHOLD_ADDR, 0u);

    unsigned tmp;
    __asm__ volatile("csrrs %0, mie, %1" : "=r"(tmp) : "r"(MIE_MEIE));
    __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE));

    const unsigned control = mmio_read32("PMU_CONTROL", PMU_CONTROL_ADDR);

    mmio_write32("PMU_WAKEUP_EN", PMU_WAKEUP_EN_ADDR, 0x1u);
    const unsigned wakeup_en = mmio_read32("PMU_WAKEUP_EN", PMU_WAKEUP_EN_ADDR);

    mmio_write32("PMU_INTR_ENABLE", PMU_INTR_ENABLE_ADDR, PMU_INTR_WAKEUP);

    /* Trigger PMU interrupt through INTR_TEST. */
    mmio_write32("PMU_INTR_TEST", PMU_INTR_TEST_ADDR, PMU_INTR_WAKEUP);

    while (!pmu_irq_done) {
        __asm__ volatile("wfi");
    }

    const unsigned intr_state = mmio_read32("PMU_INTR_STATE", PMU_INTR_STATE_ADDR);
    const unsigned wake_status = mmio_read32("PMU_WAKE_STATUS", PMU_WAKE_STATUS_ADDR);

    if (control == PMU_CONTROL_DEFAULT &&
        wakeup_en == 0x1u &&
        pmu_irq_done == 1u &&
        ((intr_state & PMU_INTR_WAKEUP) == 0u)) {
        uart_puts("PMU PASS\n");
    } else {
        uart_puts("PMU FAIL\n");

        uart_puts("control=");
        uart_put_hex32(control);
        uart_puts("\n");

        uart_puts("wakeup_en=");
        uart_put_hex32(wakeup_en);
        uart_puts("\n");

        uart_puts("intr_state=");
        uart_put_hex32(intr_state);
        uart_puts("\n");

        uart_puts("wake_status=");
        uart_put_hex32(wake_status);
        uart_puts("\n");
    }

    for (;;) {
        __asm__ volatile("wfi");
    }

    return 0;
}
