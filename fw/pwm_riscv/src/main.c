/* PWM SoC verification firmware.
 *
 * Test path:
 *   CPU -> bus_router -> PWM MMIO
 *   Write period, duty cycle, enable, then read back to verify.
 */

#define UART_TX             (*(volatile unsigned char *)0x10000000u)
#define PWM_BASE            0x10050000u
#define PWM_CFG_ADDR        (PWM_BASE + 0x08u)
#define PWM_EN_ADDR         (PWM_BASE + 0x0Cu)
#define PWM_INVERT_ADDR     (PWM_BASE + 0x10u)
#define PWM_PARAM0_ADDR     (PWM_BASE + 0x14u)
#define PWM_DUTY0_ADDR      (PWM_BASE + 0x2Cu)
#define PWM_PERIOD_ADDR     (PWM_BASE + 0x30u)

#define MMIO32(addr)        (*(volatile unsigned int *)(addr))

static void uart_putc(char c) { UART_TX = (unsigned char)c; }

static void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}

static void uart_put_hex32(unsigned value)
{
    static const char hex[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        uart_putc(hex[(value >> shift) & 0xFu]);
}

static void mmio_write32(const char *name, unsigned addr, unsigned value)
{
    MMIO32(addr) = value;
    uart_puts("WRITE ");
    uart_puts(name);
    uart_puts(" [");
    uart_put_hex32(addr);
    uart_puts("] <= ");
    uart_put_hex32(value);
    uart_puts("\n");
}

static unsigned mmio_read32(const char *name, unsigned addr)
{
    unsigned value = MMIO32(addr);
    uart_puts("READ  ");
    uart_puts(name);
    uart_puts(" [");
    uart_put_hex32(addr);
    uart_puts("] => ");
    uart_put_hex32(value);
    uart_puts("\n");
    return value;
}

int main(void)
{
    uart_puts("PWM platform start\n");

    /* Configure PWM: period=100, duty=50, enable */
    mmio_write32("PWM_PERIOD", PWM_PERIOD_ADDR, 100u);
    mmio_write32("PWM_DUTY0",  PWM_DUTY0_ADDR,  50u);
    mmio_write32("PWM_CFG",    PWM_CFG_ADDR,    0u);
    mmio_write32("PWM_PARAM0", PWM_PARAM0_ADDR, 0u);
    mmio_write32("PWM_INVERT", PWM_INVERT_ADDR, 0u);
    mmio_write32("PWM_EN",     PWM_EN_ADDR,     1u);

    /* Read back and verify */
    unsigned period = mmio_read32("PWM_PERIOD", PWM_PERIOD_ADDR);
    unsigned duty   = mmio_read32("PWM_DUTY0",  PWM_DUTY0_ADDR);
    unsigned en     = mmio_read32("PWM_EN",     PWM_EN_ADDR);

    if (period == 100u && duty == 50u && en == 1u) {
        uart_puts("PWM PASS\n");
    } else {
        uart_puts("PWM FAIL\n");
        uart_puts("  period=");
        uart_put_hex32(period);
        uart_puts("\n  duty=");
        uart_put_hex32(duty);
        uart_puts("\n  en=");
        uart_put_hex32(en);
        uart_puts("\n");
    }

    for (;;) __asm__ volatile("wfi");
    return 0;
}