// Plan T21 wake/reset test, Ibex side (VP verification firmware, not a Boot
// ROM): ROM stub (ibex_rom.S) -> this image; the CVA6 image (t21_cva6.c) is
// preloaded and released here. Interrupts stay disabled (mstatus.MIE = 0):
// WFI wakes on a pending interrupt enabled in mie, without a trap.
// Boot 1:
//   1. WFI woken by TIMER1 through INTMAP (irq_timer_i, mip.MTIP);
//   2. TIMER1 clock gated before its match, TIMER2 (irq_external_i) as the
//      waker: TIMER1 raises nothing while gated, then matches once ungated;
//   3. CVA6 released: it checks its own WFI wakes (TIMER0, PLIC, SWIRQ from
//      Ibex) and wakes Ibex with SWIRQ (irq_software_i);
//   4. WDT0 armed (warning, then reset request) while both CPUs run memory and
//      MMIO accesses: a watchdog reset arrives in the middle of their traffic.
// Boot 2: RESET_CAUSE shows POR and watchdog; "PASS".
// Any failed step prints "FAIL <step>".
#include "fw_common.h"
#include "fu3_vp_abi.h"

struct fw_mailbox *const fw_box = FW_MAILBOX_IBEX;

#define REG(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#define REG8(addr) (*(volatile uint8_t *)(uintptr_t)(addr))
#define UART0 0x440d0000u
#define UART_LSR (UART0 + 5)
#define CRM 0x50010000u
#define CRM_CLK_GATE_1 (CRM + 0x2c)
#define CRM_FCLK_SW (CRM + 0x34)
#define TIMER1 0x44070000u
#define TIMER2 0x50060000u
#define TIMER_COUNT 0x0
#define TIMER_CONTROL 0x4
#define TIMER_COMPARE 0x8
#define GATE1_TIMER1 (1u << 19)
#define WDT0 0x44050000u
#define ABI(off) REG(FU3_VP_ABI_SYSCSR_BASE + (off))

#define MIP_MSIP (1u << 3)
#define MIP_MTIP (1u << 7)
#define MIP_MEIP (1u << 11)

#define T21_MAGIC REG(0x0103f300u)
#define T21_BOOTS REG(0x0103f304u)
#define T21_CVA6_READY REG(0x0103f308u)   // CVA6 waits in WFI for the Ibex SWIRQ
#define T21_CVA6_RESULT REG(0x0103f30cu)  // bit 0 TIMER0, 1 PLIC, 2 SWIRQ; 0x80000000 done
#define T21_TRAFFIC REG(0x0103f310u)      // both CPUs keep incrementing it until the reset

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

static void fail(const char *step)
{
    puts_("FAIL ");
    puts_(step);
    putc_('\n');
    for (;;)
        ;
}

static uint32_t mip(void)
{
    return CSR_READ(mip);
}

static int wait_mip(uint32_t bits, unsigned limit)
{
    for (unsigned i = 0; i < limit; ++i)
        if (mip() & bits)
            return 1;
    return 0;
}

static void timer_arm(uint32_t base, uint32_t ticks)
{
    REG(base + TIMER_COMPARE) = REG(base + TIMER_COUNT) + ticks;
    REG(base + TIMER_CONTROL) = 1;
}

static void timer_ack(uint32_t base)
{
    REG(base + TIMER_CONTROL) = 0;
    REG(base + TIMER_COMPARE) = 0;   // CMP write acknowledges (Q03-d)
}

static void boot_1(void)
{
    CSR_WRITE(mstatus, 0);   // MIE = 0: WFI wakes without a trap
    ABI(FU3_VP_INTMAP_EN0_OFFSET) = 1u << FU3_VP_INTMAP_SRC_TIMER2;
    ABI(FU3_VP_INTMAP_EN1_OFFSET) = (1u << (FU3_VP_INTMAP_SRC_TIMER1 - 32)) | (1u << (FU3_VP_INTMAP_SRC_SW_IBEX - 32));

    // 1. TIMER1 -> irq_timer_i wakes WFI.
    CSR_WRITE(mie, MIP_MTIP);
    timer_arm(TIMER1, 40);
    __asm__ volatile("wfi");
    if (!(mip() & MIP_MTIP))
        fail("ibex wfi timer1");
    timer_ack(TIMER1);
    if (mip() & MIP_MTIP)
        fail("ibex timer1 ack");
    puts_("ibex wfi timer1 ok\n");

    // 2. TIMER1 gated well before its match (200 ticks); TIMER2 (1000 ticks)
    //    wakes WFI instead.
    timer_arm(TIMER1, 200);
    REG(CRM_CLK_GATE_1) &= ~GATE1_TIMER1;
    timer_arm(TIMER2, 1000);
    CSR_WRITE(mie, MIP_MTIP | MIP_MEIP);
    __asm__ volatile("wfi");
    const uint32_t woke = mip();
    if (!(woke & MIP_MEIP) || (woke & MIP_MTIP)) {
        puts_("mip ");
        puthex(woke);
        putc_('\n');
        fail("ibex gated timer1 woke or timer2 missing");
    }
    timer_ack(TIMER2);
    REG(CRM_CLK_GATE_1) |= GATE1_TIMER1;
    if (!wait_mip(MIP_MTIP, 2000))
        fail("ibex timer1 after ungate");
    timer_ack(TIMER1);
    puts_("ibex gated timer1 no wake ok\n");

    // 3. CVA6 checks its WFI wakes, then waits for SWIRQ; it answers with one.
    CSR_WRITE(mie, MIP_MSIP);
    T21_CVA6_READY = 0;
    T21_CVA6_RESULT = 0;
    REG(CRM_FCLK_SW) = 1;   // release CVA6
    while (!T21_CVA6_READY)
        ;
    ABI(FU3_VP_SWIRQ_SET_OFFSET) = FU3_VP_SWIRQ_CVA6;
    __asm__ volatile("wfi");   // CVA6 raises the Ibex software interrupt
    if (!(mip() & MIP_MSIP))
        fail("ibex wfi swirq");
    ABI(FU3_VP_SWIRQ_CLEAR_OFFSET) = FU3_VP_SWIRQ_IBEX;
    if (mip() & MIP_MSIP)
        fail("ibex swirq clear");
    while (!(T21_CVA6_RESULT & 0x80000000u))
        ;
    puts_("cva6 wfi ");
    puthex(T21_CVA6_RESULT);
    putc_('\n');
    if (T21_CVA6_RESULT != 0x80000007u)
        fail("cva6 wfi");
    puts_("ibex wfi swirq ok\n");

    // 4. Watchdog reset in the middle of the traffic of both CPUs (the line is
    //    sent first: at the OSC clock it takes longer than the watchdog).
    puts_("wdt armed\n");
    while (!(REG8(UART_LSR) & 0x40))
        ;
    REG(WDT0 + 0x1000) = 0x1acce551u;   // unlock
    REG(WDT0 + 0x1004) = 100;           // LOAD: two expiries of 100 ticks
    REG(WDT0 + 0x100c) = 3;             // INTEN | RESEN
    REG(WDT0 + 0x1000) = 0;             // lock
    for (;;) {
        T21_TRAFFIC = T21_TRAFFIC + 1;
        (void)REG(TIMER2 + TIMER_COUNT);
    }
}

int main(void)
{
    if (T21_MAGIC != 0x7021b007u) {
        T21_MAGIC = 0x7021b007u;
        T21_BOOTS = 0;
    }
    T21_BOOTS = T21_BOOTS + 1;
    REG8(UART0 + 3) = 0x80;   // divisor 1, 8N1, FIFOs (test console only)
    REG8(UART0 + 0) = 1;
    REG8(UART0 + 1) = 0;
    REG8(UART0 + 3) = 0x03;
    REG8(UART0 + 2) = 0x07;
    puts_("T21 boot ");
    putc_((char)('0' + T21_BOOTS));
    putc_('\n');
    if (T21_BOOTS == 1)
        boot_1();
    const uint32_t cause = ABI(FU3_VP_RESET_CAUSE_OFFSET);
    puts_("reset cause ");
    puthex(cause);
    puts_(" traffic ");
    puthex(T21_TRAFFIC);
    putc_('\n');
    if (cause != (FU3_VP_RESET_POR | FU3_VP_RESET_WATCHDOG) || T21_TRAFFIC < 10)
        fail("boot 2 reset cause");
    puts_("PASS\n");
    for (;;)
        __asm__ volatile("wfi");
    return 0;
}
