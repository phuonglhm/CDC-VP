/* PWM SoC verification firmware.
 *
 * Tests:
 *   1. Basic register read/write
 *   2. Duty cycle boundaries (0%, 50%, 100%, duty > period)
 *   3. Invert polarity register
 *   4. Enable / disable
 *   5. Large period value
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

static unsigned check(const char *name, unsigned got, unsigned expected) {
    if (got == expected) {
        uart_puts("  PASS ");
        uart_puts(name);
        uart_puts("\n");
        return 1;
    }
    uart_puts("  FAIL ");
    uart_puts(name);
    uart_puts(" expected=");
    uart_put_hex32(expected);
    uart_puts(" got=");
    uart_put_hex32(got);
    uart_puts("\n");
    return 0;
}

int main(void)
{
    uart_puts("PWM platform start\n");
    unsigned pass = 1;

    /* ── Test 1: Basic register read/write ── */
    uart_puts("\n[1] Basic register read/write\n");
    MMIO32(PWM_CFG_ADDR)    = 0u;
    MMIO32(PWM_INVERT_ADDR) = 0u;
    MMIO32(PWM_PARAM0_ADDR) = 0u;
    MMIO32(PWM_PERIOD_ADDR) = 100u;
    MMIO32(PWM_DUTY0_ADDR)  = 50u;
    MMIO32(PWM_EN_ADDR)     = 1u;
    pass &= check("period",  MMIO32(PWM_PERIOD_ADDR), 100u);
    pass &= check("duty",    MMIO32(PWM_DUTY0_ADDR),  50u);
    pass &= check("pwm_en",  MMIO32(PWM_EN_ADDR),     1u);
    pass &= check("cfg",     MMIO32(PWM_CFG_ADDR),    0u);
    pass &= check("invert",  MMIO32(PWM_INVERT_ADDR), 0u);
    pass &= check("param0",  MMIO32(PWM_PARAM0_ADDR), 0u);

    /* ── Test 2: Duty cycle boundaries ── */
    uart_puts("\n[2] Duty cycle boundaries\n");
    MMIO32(PWM_PERIOD_ADDR) = 100u;

    MMIO32(PWM_DUTY0_ADDR) = 0u;
    pass &= check("duty=0 (0%)", MMIO32(PWM_DUTY0_ADDR), 0u);

    MMIO32(PWM_DUTY0_ADDR) = 50u;
    pass &= check("duty=50 (50%)", MMIO32(PWM_DUTY0_ADDR), 50u);

    MMIO32(PWM_DUTY0_ADDR) = 100u;
    pass &= check("duty=100 (100%)", MMIO32(PWM_DUTY0_ADDR), 100u);

    /* duty > period: informational only, no assert */
    MMIO32(PWM_DUTY0_ADDR) = 200u;
    uart_puts("  INFO duty>period readback=");
    uart_put_hex32(MMIO32(PWM_DUTY0_ADDR));
    uart_puts("\n");

    /* ── Test 3: Invert polarity ── */
    uart_puts("\n[3] Invert polarity\n");
    MMIO32(PWM_INVERT_ADDR) = 0u;
    pass &= check("invert=0", MMIO32(PWM_INVERT_ADDR), 0u);
    MMIO32(PWM_INVERT_ADDR) = 1u;
    pass &= check("invert=1", MMIO32(PWM_INVERT_ADDR), 1u);
    MMIO32(PWM_INVERT_ADDR) = 0u; /* restore */

    /* ── Test 4: Enable / disable ── */
    uart_puts("\n[4] Enable / disable\n");
    MMIO32(PWM_EN_ADDR) = 1u;
    pass &= check("enable",  MMIO32(PWM_EN_ADDR), 1u);
    MMIO32(PWM_EN_ADDR) = 0u;
    pass &= check("disable", MMIO32(PWM_EN_ADDR), 0u);

    /* ── Test 5: Large period value ── */
    uart_puts("\n[5] Large period value\n");
    MMIO32(PWM_PERIOD_ADDR) = 0xFFFFu;
    pass &= check("period=0xFFFF", MMIO32(PWM_PERIOD_ADDR), 0xFFFFu);

    /* ── Result ── */
    uart_puts("\n");
    uart_puts(pass ? "PWM PASS\n" : "PWM FAIL\n");
    for (;;) __asm__ volatile("wfi");
    return 0;
}