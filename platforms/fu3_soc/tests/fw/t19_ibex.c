// Plan T19 GDB test, Ibex side (VP verification firmware, not a Boot ROM):
// ROM stub (ibex_rom.S) -> this image in IBEX_ISRAM.
//   boot 1: t19_boot_mark(1), then a software reset (SYSCSR SYS_CTRL);
//   boot 2: t19_boot_mark(2), releases the preloaded CVA6 image (CRM FCLK_SW),
//           t19_after_handoff(), waits for the CVA6 result and prints it
//           (t19_report: "cva6 result 0x........", then "PASS").
// A boot counter in DSRAM (kept across the software reset) tells the boots
// apart. The GDB test breaks on t19_boot_mark in both boots (a breakpoint
// across a reset of the hart) and on t19_after_handoff.
#include "fw_common.h"

struct fw_mailbox *const fw_box = FW_MAILBOX_IBEX;

#define REG(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#define REG8(addr) (*(volatile uint8_t *)(uintptr_t)(addr))
#define UART0 0x440d0000u
#define UART_LSR (UART0 + 5)
#define SYSCSR_SYS_CTRL 0x44010074u
#define CRM_FCLK_SW 0x50010034u

#define T19_MAGIC REG(0x0103f200u)
#define T19_BOOTS REG(0x0103f204u)
#define T19_CVA6_DONE REG(0x0103f208u)
#define T19_CVA6_RESULT REG(0x0103f20cu)

static void putc_(char c)
{
    while (!(REG8(UART_LSR) & 0x20))
        ;
    REG8(UART0) = (uint8_t)c;
}

static void puts_(const char *s)
{
    while (*s)
        putc_(*s++);
}

static void puthex(uint32_t v)
{
    puts_("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        putc_("0123456789abcdef"[(v >> shift) & 0xf]);
}

// Breakpoint targets of the GDB test (a0 = boot number).
__attribute__((noinline)) void t19_boot_mark(uint32_t boot)
{
    T19_BOOTS = boot;
}

__attribute__((noinline)) void t19_after_handoff(void)
{
    __asm__ volatile("nop");
}

// Runs after the GDB test detached (a breakpoint left there must be gone).
__attribute__((noinline)) void t19_report(uint32_t result)
{
    puts_("cva6 result ");
    puthex(result);
    puts_("\nPASS\n");
}

int main(void)
{
    if (T19_MAGIC != 0x7019b007u) {   // first boot after power-on
        T19_MAGIC = 0x7019b007u;
        T19_BOOTS = 0;
        T19_CVA6_DONE = 0;
    }
    t19_boot_mark(T19_BOOTS + 1);

    REG8(UART0 + 3) = 0x80;   // divisor 1, 8N1, FIFOs (test console only)
    REG8(UART0 + 0) = 1;
    REG8(UART0 + 1) = 0;
    REG8(UART0 + 3) = 0x03;
    REG8(UART0 + 2) = 0x07;
    puts_("T19 boot ");
    putc_((char)('0' + T19_BOOTS));
    putc_('\n');

    if (T19_BOOTS == 1) {
        while (!(REG8(UART_LSR) & 0x40))   // line sent (TEMT)
            ;
        REG(SYSCSR_SYS_CTRL) = 1;   // sw_reset_req
        for (;;)
            ;
    }

    REG(CRM_FCLK_SW) = 1;   // release CVA6 (OSC clock)
    t19_after_handoff();
    while (!T19_CVA6_DONE)
        ;
    t19_report(T19_CVA6_RESULT);
    for (;;)
        __asm__ volatile("wfi");
    return 0;
}
