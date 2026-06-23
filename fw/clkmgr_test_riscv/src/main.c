//Author: QuanNH107
//Verified: trangmn20

/* OpenTitan clkmgr behavioral firmware for clkmgr_platform.
 *
 * The platform temporarily maps clkmgr at the ADC placeholder MMIO base
 * 0x10060000. This test verifies behavior that depends on platform mock
 * signals: transactional idle_i qualification and AST external-clock ack.
 */

#include <stdint.h>

#define UART_TX (*(volatile uint8_t *)0x10000000u)
#define MMIO32(addr) (*(volatile uint32_t *)(addr))

#define CLKMGR_BASE 0x10060000u
#define CLKMGR_EXTCLK_CTRL_REGWEN_ADDR (CLKMGR_BASE + 0x04u)
#define CLKMGR_EXTCLK_CTRL_ADDR (CLKMGR_BASE + 0x08u)
#define CLKMGR_EXTCLK_STATUS_ADDR (CLKMGR_BASE + 0x0Cu)
#define CLKMGR_CLK_ENABLES_ADDR (CLKMGR_BASE + 0x18u)
#define CLKMGR_CLK_HINTS_ADDR (CLKMGR_BASE + 0x1Cu)
#define CLKMGR_CLK_HINTS_STATUS_ADDR (CLKMGR_BASE + 0x20u)

#define MUBI4_TRUE 0x6u
#define MUBI4_FALSE 0x9u

#define EXTCLK_CTRL_VALUE(sel, hi_speed) (((hi_speed) << 4) | (sel))

#define CLKMGR_CLK_HINTS_ALL 0xFu
#define CLKMGR_CLK_HINTS_NONE 0x0u
#define CLKMGR_POLL_TIMEOUT 200000u

static uint32_t failures = 0u;

static void uart_putc(char c)
{
    UART_TX = (uint8_t)c;
}

static void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

static void uart_put_hex32(uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";
    uart_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        uart_putc(hex[(value >> shift) & 0xFu]);
    }
}

static void uart_put_uint(uint32_t value)
{
    char buf[10];
    int i = 0;

    if (value == 0u) {
        uart_putc('0');
        return;
    }

    while (value != 0u) {
        buf[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

static uint32_t mmio_read32(const char *name, uint32_t addr)
{
    const uint32_t value = MMIO32(addr);
    uart_puts("READ  ");
    uart_puts(name);
    uart_puts(" [");
    uart_put_hex32(addr);
    uart_puts("] => ");
    uart_put_hex32(value);
    uart_puts("\n");
    return value;
}

static void mmio_write32(const char *name, uint32_t addr, uint32_t value)
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

static void pass(const char *name)
{
    uart_puts("PASS ");
    uart_puts(name);
    uart_puts("\n");
}

static void fail(const char *name, uint32_t last_value)
{
    uart_puts("FAIL ");
    uart_puts(name);
    uart_puts(": last=");
    uart_put_hex32(last_value);
    uart_puts("\n");
    failures++;
}

static int poll_masked_eq(const char *name, uint32_t addr, uint32_t mask,
                          uint32_t expected, uint32_t timeout)
{
    uint32_t last = 0u;

    for (uint32_t i = 0; i < timeout; ++i) {
        last = MMIO32(addr);
        if ((last & mask) == (expected & mask)) {
            uart_puts("POLL ");
            uart_puts(name);
            uart_puts(" matched after ");
            uart_put_uint(i + 1u);
            uart_puts(" reads\n");
            (void)mmio_read32(name, addr);
            return 1;
        }
    }

    fail(name, last);
    return 0;
}

static void check_reset_state(void)
{
    uart_puts("\n--- Reset sanity ---\n");

    if ((mmio_read32("EXTCLK_CTRL_REGWEN", CLKMGR_EXTCLK_CTRL_REGWEN_ADDR) & 0x1u) == 0x1u) {
        pass("EXTCLK_CTRL_REGWEN reset");
    } else {
        fail("EXTCLK_CTRL_REGWEN reset", MMIO32(CLKMGR_EXTCLK_CTRL_REGWEN_ADDR));
    }

    if ((mmio_read32("CLK_ENABLES", CLKMGR_CLK_ENABLES_ADDR) & 0xFu) == 0xFu) {
        pass("CLK_ENABLES reset");
    } else {
        fail("CLK_ENABLES reset", MMIO32(CLKMGR_CLK_ENABLES_ADDR));
    }
}

static void test_transactional_clock_hints(void)
{
    uart_puts("\n--- Test A: transactional clock hints polling ---\n");

    mmio_write32("CLK_HINTS", CLKMGR_CLK_HINTS_ADDR, CLKMGR_CLK_HINTS_NONE);

    if (poll_masked_eq("CLK_HINTS_STATUS", CLKMGR_CLK_HINTS_STATUS_ADDR,
                       0xFu, 0x0u, CLKMGR_POLL_TIMEOUT)) {
        pass("transactional clocks gated after idle_i qualification");
    }

    mmio_write32("CLK_HINTS", CLKMGR_CLK_HINTS_ADDR, CLKMGR_CLK_HINTS_ALL);

    if (poll_masked_eq("CLK_HINTS_STATUS", CLKMGR_CLK_HINTS_STATUS_ADDR,
                       0xFu, 0xFu, CLKMGR_POLL_TIMEOUT)) {
        pass("transactional clocks re-enabled by hints");
    }
}

static void test_external_clock_switch(void)
{
    uart_puts("\n--- Test B: external clock switch handshake polling ---\n");

    mmio_write32("EXTCLK_CTRL", CLKMGR_EXTCLK_CTRL_ADDR,
                 EXTCLK_CTRL_VALUE(MUBI4_TRUE, MUBI4_TRUE));

    if (poll_masked_eq("EXTCLK_STATUS", CLKMGR_EXTCLK_STATUS_ADDR,
                       0xFu, MUBI4_TRUE, CLKMGR_POLL_TIMEOUT)) {
        pass("external high-speed clock acknowledged");
    }

    mmio_write32("EXTCLK_CTRL", CLKMGR_EXTCLK_CTRL_ADDR,
                 EXTCLK_CTRL_VALUE(MUBI4_FALSE, MUBI4_TRUE));

    if (poll_masked_eq("EXTCLK_STATUS", CLKMGR_EXTCLK_STATUS_ADDR,
                       0xFu, MUBI4_FALSE, CLKMGR_POLL_TIMEOUT)) {
        pass("internal clock switch acknowledged");
    }
}

int main(void)
{
    uart_puts("CLKMGR behavioral platform start\n");

    check_reset_state();
    test_transactional_clock_hints();
    test_external_clock_switch();

    if (failures == 0u) {
        uart_puts("\nCLKMGR BEHAVIOR TEST PASS\n");
    } else {
        uart_puts("\nCLKMGR BEHAVIOR TEST FAIL\n");
    }

    for (;;) {
        __asm__ volatile("wfi");
    }

    return 0;
}
