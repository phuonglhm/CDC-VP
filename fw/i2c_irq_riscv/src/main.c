#define UART_TX (*(volatile unsigned char *)0x10000000u)

#define PLIC_PRIORITY1_ADDR   0x0C000004u
#define PLIC_ENABLE_ADDR      0x0C002000u
#define PLIC_THRESHOLD_ADDR   0x0C200000u
#define PLIC_CLAIM_ADDR       0x0C200004u

#define I2C_BASE              0x10010000u
#define I2C_INTR_STATE        (I2C_BASE + 0x00)
#define I2C_INTR_ENABLE       (I2C_BASE + 0x04)
#define I2C_CTRL              (I2C_BASE + 0x10)
#define I2C_STATUS            (I2C_BASE + 0x14)
#define I2C_FDATA             (I2C_BASE + 0x1C)

#define MMIO32(addr) (*(volatile unsigned int *)(addr))

#define CTRL_ENABLEHOST       (1u << 0)

#define FDATA_START           (1u << 8)
#define FDATA_STOP            (1u << 9)

#define STATUS_FMTFULL        (1u << 0)
#define INTR_CMD_COMPLETE     (1u << 9)

#define MIE_MEIE              (1u << 11)
#define MSTATUS_MIE           (1u << 3)

static volatile unsigned i2c_irq_done = 0;
static volatile unsigned i2c_intr_seen = 0;

static void uart_putc(char c)
{
    UART_TX = (unsigned char)c;
}

static void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

static void uart_put_hex32(unsigned value)
{
    static const char hex[] = "0123456789ABCDEF";

    uart_puts("0x");

    for (int shift = 28; shift >= 0; shift -= 4) {
        uart_putc(hex[(value >> shift) & 0xF]);
    }
}

static void print_reg(const char *name, unsigned value)
{
    uart_puts(name);
    uart_puts("=");
    uart_put_hex32(value);
    uart_puts("\n");
}

static unsigned mmio_read32(const char *name, unsigned addr)
{
    unsigned value = MMIO32(addr);

    uart_puts("READ  ");
    uart_puts(name);
    uart_puts(" => ");
    uart_put_hex32(value);
    uart_puts("\n");

    return value;
}

static void mmio_write32(const char *name, unsigned addr, unsigned value)
{
    MMIO32(addr) = value;

    uart_puts("WRITE ");
    uart_puts(name);
    uart_puts(" <= ");
    uart_put_hex32(value);
    uart_puts("\n");
}

void __attribute__((interrupt("machine"))) trap_handler(void)
{
    unsigned mcause;
    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));

    uart_puts("TRAP mcause=");
    uart_put_hex32(mcause);
    uart_puts("\n");

    if ((mcause & 0x7FFFFFFFu) == 11u) {
        unsigned id = mmio_read32("PLIC_CLAIM", PLIC_CLAIM_ADDR);

        if (id == 1u) {
            i2c_intr_seen = mmio_read32("I2C_INTR_STATE", I2C_INTR_STATE);

            mmio_write32("I2C_INTR_STATE", I2C_INTR_STATE, i2c_intr_seen);

            i2c_irq_done = 1u;
            uart_puts("I2C IRQ\n");
        }

        mmio_write32("PLIC_CLAIM", PLIC_CLAIM_ADDR, id);
    }
}

static int test_ctrl_status(void)
{
    uart_puts("\n[TEST 1] CTRL/STATUS\n");

    mmio_write32("I2C_CTRL", I2C_CTRL, CTRL_ENABLEHOST);

    unsigned ctrl = mmio_read32("I2C_CTRL", I2C_CTRL);
    unsigned status = mmio_read32("I2C_STATUS", I2C_STATUS);

    print_reg("CTRL", ctrl);
    print_reg("STATUS", status);

    if ((ctrl & CTRL_ENABLEHOST) == 0u) {
        uart_puts("[FAIL] CTRL_ENABLEHOST not set\n");
        return 0;
    }

    uart_puts("[PASS] CTRL/STATUS\n");
    return 1;
}

static int test_irq_poll(void)
{
    uart_puts("\n[TEST 2] I2C INTR_STATE / IRQ POLL\n");

    i2c_irq_done = 0;
    i2c_intr_seen = 0;

    unsigned old_intr = mmio_read32("I2C_INTR_STATE before clear", I2C_INTR_STATE);
    if (old_intr != 0u) {
        mmio_write32("I2C_INTR_STATE clear", I2C_INTR_STATE, old_intr);
    }

    unsigned after_clear = mmio_read32("I2C_INTR_STATE after clear", I2C_INTR_STATE);
    print_reg("INTR_AFTER_CLEAR", after_clear);

    mmio_write32("PLIC_PRIORITY1", PLIC_PRIORITY1_ADDR, 1u);
    mmio_write32("PLIC_ENABLE", PLIC_ENABLE_ADDR, (1u << 1));
    mmio_write32("PLIC_THRESHOLD", PLIC_THRESHOLD_ADDR, 0u);

    unsigned tmp;
    __asm__ volatile("csrrs %0, mie, %1" : "=r"(tmp) : "r"(MIE_MEIE));
    __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE));

    for (volatile unsigned i = 0; i < 1000u; ++i) {
        __asm__ volatile("nop");
    }

    mmio_write32("I2C_INTR_ENABLE", I2C_INTR_ENABLE, INTR_CMD_COMPLETE);

    mmio_write32("I2C_FDATA IRQ START", I2C_FDATA, FDATA_START | 0x52u);
    mmio_write32("I2C_FDATA IRQ DATA",  I2C_FDATA, 0x33u);
    mmio_write32("I2C_FDATA IRQ STOP",  I2C_FDATA, FDATA_STOP | 0x44u);

for (volatile unsigned i = 0; i < 10u; ++i) {
    __asm__ volatile("wfi");
}

    unsigned timeout = 1000000u;
    while (((MMIO32(I2C_INTR_STATE) & INTR_CMD_COMPLETE) == 0u) && timeout--) {
        __asm__ volatile("nop");
    }

    i2c_intr_seen = mmio_read32("I2C_INTR_STATE debug", I2C_INTR_STATE);
    print_reg("DEBUG_INTR", i2c_intr_seen);

    if ((i2c_intr_seen & INTR_CMD_COMPLETE) == 0u) {
        uart_puts("[FAIL] IRQ/POLL TIMEOUT\n");
        return 0;
    }

    uart_puts("I2C IRQ/POLL detected\n");

    if (i2c_irq_done) {
        uart_puts("CPU trap also detected\n");
    } else {
        uart_puts("CPU trap not detected, but IP interrupt state is set\n");
    }

    mmio_write32("I2C_INTR_STATE clear final", I2C_INTR_STATE, i2c_intr_seen);

    uart_puts("[PASS] I2C INTR_STATE / IRQ POLL\n");
    return 1;
}

static int test_fifo_command(void)
{
    uart_puts("\n[TEST 3] FDATA FIFO COMMAND\n");

    unsigned old_intr = mmio_read32("I2C_INTR_STATE before fifo", I2C_INTR_STATE);
    if (old_intr != 0u) {
        mmio_write32("I2C_INTR_STATE clear fifo", I2C_INTR_STATE, old_intr);
    }

    mmio_write32("I2C_FDATA START+ADDR", I2C_FDATA, FDATA_START | 0x50u);
    mmio_write32("I2C_FDATA DATA",       I2C_FDATA, 0xA5u);
    mmio_write32("I2C_FDATA STOP+DATA",  I2C_FDATA, FDATA_STOP | 0x5Au);

    unsigned status = mmio_read32("I2C_STATUS", I2C_STATUS);
    unsigned intr = mmio_read32("I2C_INTR_STATE", I2C_INTR_STATE);

    print_reg("STATUS_AFTER_FIFO", status);
    print_reg("INTR_AFTER_FIFO", intr);

    if ((intr & INTR_CMD_COMPLETE) == 0u) {
        uart_puts("[FAIL] CMD_COMPLETE not set\n");
        return 0;
    }

    if ((status & STATUS_FMTFULL) != 0u) {
        uart_puts("[FAIL] FMT FIFO full\n");
        return 0;
    }

    mmio_write32("I2C_INTR_STATE clear fifo final", I2C_INTR_STATE, intr);

    uart_puts("[PASS] FDATA FIFO COMMAND\n");
    return 1;
}

int main(void)
{
    uart_puts("I2C full firmware test start\n");

    __asm__ volatile("csrw mtvec, %0" ::"r"(trap_handler));

    int pass = 1;

    pass &= test_ctrl_status();
    pass &= test_irq_poll();
    pass &= test_fifo_command();

    if (pass) {
        uart_puts("\nI2C FULL PASS\n");
    } else {
        uart_puts("\nI2C FULL FAIL\n");
    }

    for (;;) {
        __asm__ volatile("wfi");
    }

    return 0;
}
