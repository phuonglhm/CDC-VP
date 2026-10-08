#include "fx1_fw.h"

/* PL011-style UART (components/uart2_tlm). */
#define UART_DR   0x000u
#define UART_FR   0x018u
#define UART_TXFF 0x20u

/* Forced into .data: the flag must be 0 in the loaded image and must not rely
 * on hart 0's .bss clear having run. */
volatile uint32_t fx1_secondary_release __attribute__((section(".data"))) = 0;

static fx1_trap_handler_t trap_handlers[FX1_NUM_HARTS];

/* Console lock: 0 when free, otherwise owner hartid + 1. Knowing the owner lets
 * a hart that faults while printing still report, instead of spinning on the
 * lock it already holds. */
static volatile uint32_t console_owner;

/* Bounded wait used by the fatal path before printing anyway. */
#define FATAL_LOCK_TRIES 100000u

static int cas32(volatile uint32_t* word, uint32_t expected, uint32_t desired)
{
    uint32_t old, failed;
    __asm__ volatile("1: lr.w.aq %0, (%2)\n"
                     "   bne %0, %3, 2f\n"
                     "   sc.w.rl %1, %4, (%2)\n"
                     "   bnez %1, 1b\n"
                     "2:"
                     : "=&r"(old), "=&r"(failed)
                     : "r"(word), "r"(expected), "r"(desired)
                     : "memory");
    return old == expected;
}

enum console_grant { CONSOLE_ACQUIRED, CONSOLE_NESTED, CONSOLE_UNLOCKED };

/* tries == 0 waits for ever. A hart that already owns the lock (it trapped
 * while printing) is granted nested, unlocked access. */
static enum console_grant console_take(uint32_t tries)
{
    const uint32_t me = fx1_hartid() + 1u;
    if (console_owner == me) return CONSOLE_NESTED;
    for (uint32_t i = 0; tries == 0 || i < tries; ++i) {
        if (cas32(&console_owner, 0, me)) return CONSOLE_ACQUIRED;
    }
    return CONSOLE_UNLOCKED;
}

static void console_give(enum console_grant grant)
{
    if (grant == CONSOLE_ACQUIRED)
        __asm__ volatile("amoswap.w.rl zero, zero, (%0)" ::"r"(&console_owner) : "memory");
}

void fx1_lock(fx1_lock_t* lock)
{
    uint32_t taken;
    do {
        __asm__ volatile("amoswap.w.aq %0, %1, (%2)"
                         : "=r"(taken)
                         : "r"(1u), "r"(&lock->word)
                         : "memory");
    } while (taken);
}

void fx1_unlock(fx1_lock_t* lock)
{
    __asm__ volatile("amoswap.w.rl zero, zero, (%0)" ::"r"(&lock->word) : "memory");
}

void fx1_putc(char c)
{
    while (fx1_read32(FX1_UART_BASE + UART_FR) & UART_TXFF) {
    }
    fx1_write32(FX1_UART_BASE + UART_DR, (uint8_t)c);
}

void fx1_puts(const char* s)
{
    while (*s) fx1_putc(*s++);
}

void fx1_puthex(uint32_t value)
{
    fx1_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        fx1_putc("0123456789abcdef"[(value >> shift) & 0xF]);
}

void fx1_putdec(uint32_t value)
{
    char digits[10];
    int n = 0;
    do {
        digits[n++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    while (n) fx1_putc(digits[--n]);
}

static void log_prefix(void)
{
    fx1_puts("[h");
    fx1_putdec(fx1_hartid());
    fx1_puts("] ");
}

static void log_line(const char* text, int with_value, uint32_t value, uint32_t tries)
{
    /* Interrupts stay masked while the lock is held, so a handler on this hart
     * can never wait for a lock its own interrupted context owns. */
    const uint32_t saved = fx1_irq_save();
    const enum console_grant grant = console_take(tries);
    log_prefix();
    fx1_puts(text);
    if (with_value) {
        fx1_putc(' ');
        fx1_puthex(value);
    }
    fx1_putc('\n');
    console_give(grant);
    fx1_irq_restore(saved);
}

void fx1_log(const char* text) { log_line(text, 0, 0, 0); }
void fx1_log_hex(const char* text, uint32_t value) { log_line(text, 1, value, 0); }

static void __attribute__((noreturn)) finish(uint32_t verdict)
{
    fx1_mb(); /* everything before the verdict, including UART writes */
    fx1_write32(FX1_SIM_CTRL_BASE + FX1_SIM_CTRL_FINISH_OFF, verdict);
    for (;;) fx1_wfi();
}

void fx1_pass(void)
{
    fx1_log("PASS");
    finish(FX1_SIM_CTRL_PASS);
}

void fx1_fail(uint32_t code)
{
    /* Fatal path: the verdict must reach sim-control even if the console is
     * held, so printing is best effort with a bounded wait. */
    (void)fx1_irq_save();
    log_line("FAIL code", 1, code, FATAL_LOCK_TRIES);
    finish(((code & 0xFFFFu) << 16) | FX1_SIM_CTRL_FAIL);
}

void fx1_exit(int code)
{
    if (code == 0) fx1_pass();
    fx1_fail((uint32_t)code);
}

void fx1_check(int condition, uint32_t code, const char* what)
{
    if (condition) return;
    (void)fx1_irq_save();
    log_line(what, 0, 0, FATAL_LOCK_TRIES);
    fx1_fail(code);
}

uint64_t fx1_mtime(void)
{
    const uintptr_t base = FX1_CLINT_BASE + FX1_CLINT_MTIME_OFF;
    uint32_t hi, lo;
    do {
        hi = fx1_read32(base + 4);
        lo = fx1_read32(base);
    } while (hi != fx1_read32(base + 4));
    return ((uint64_t)hi << 32) | lo;
}

uint64_t fx1_rdtime(void)
{
    uint32_t hi, lo, hi2;
    do {
        __asm__ volatile("csrr %0, timeh" : "=r"(hi));
        __asm__ volatile("csrr %0, time" : "=r"(lo));
        __asm__ volatile("csrr %0, timeh" : "=r"(hi2));
    } while (hi != hi2);
    return ((uint64_t)hi << 32) | lo;
}

void fx1_set_mtimecmp(uint32_t hart, uint64_t value)
{
    /* Raise the high word first so no intermediate value lies in the past. */
    const uintptr_t reg = FX1_CLINT_BASE + FX1_CLINT_MTIMECMP_OFF + 8u * hart;
    fx1_write32(reg + 4, 0xFFFFFFFFu);
    fx1_write32(reg, (uint32_t)value);
    fx1_write32(reg + 4, (uint32_t)(value >> 32));
}

void fx1_set_msip(uint32_t hart, uint32_t value)
{
    fx1_write32(FX1_CLINT_BASE + FX1_CLINT_MSIP_OFF + 4u * hart, value & 1u);
}

static uintptr_t plic_context(uint32_t context)
{
    return FX1_PLIC_BASE + FX1_PLIC_CONTEXT_OFF + FX1_PLIC_CONTEXT_STRIDE * context;
}

void fx1_plic_set_priority(uint32_t source, uint32_t priority)
{
    fx1_write32(FX1_PLIC_BASE + FX1_PLIC_PRIORITY_OFF + 4u * source, priority);
}

void fx1_plic_enable(uint32_t context, uint32_t source, int on)
{
    const uintptr_t reg =
        FX1_PLIC_BASE + FX1_PLIC_ENABLE_OFF + FX1_PLIC_ENABLE_STRIDE * context + 4u * (source / 32);
    uint32_t bits = fx1_read32(reg);
    if (on) bits |= 1u << (source % 32);
    else bits &= ~(1u << (source % 32));
    fx1_write32(reg, bits);
}

void fx1_plic_set_threshold(uint32_t context, uint32_t threshold)
{
    fx1_write32(plic_context(context), threshold);
}

uint32_t fx1_plic_pending(void)
{
    return fx1_read32(FX1_PLIC_BASE + FX1_PLIC_PENDING_OFF);
}

uint32_t fx1_plic_claim(uint32_t context)
{
    return fx1_read32(plic_context(context) + 4);
}

void fx1_plic_complete(uint32_t context, uint32_t source)
{
    fx1_write32(plic_context(context) + 4, source);
}

void fx1_sim_irq_set(uint32_t mask)
{
    fx1_write32(FX1_SIM_CTRL_BASE + FX1_SIM_CTRL_IRQ_OFF, mask);
}

uint32_t fx1_sim_irq_get(void)
{
    return fx1_read32(FX1_SIM_CTRL_BASE + FX1_SIM_CTRL_IRQ_OFF);
}

void fx1_release_secondaries(void)
{
    fx1_release();             /* shared data written so far, then the flag */
    fx1_secondary_release = 1;
    fx1_mb();                  /* flag before the MSIP device write (W,O or stronger) */
    for (uint32_t hart = 1; hart < FX1_NUM_HARTS; ++hart) fx1_set_msip(hart, 1);
}

__attribute__((weak)) void secondary_main(uint32_t hart)
{
    (void)hart;
    for (;;) fx1_wfi();
}

void fx1_set_trap_handler(fx1_trap_handler_t handler)
{
    trap_handlers[fx1_hartid()] = handler;
}

uint32_t fx1_trap_dispatch(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    const uint32_t hart = fx1_hartid();
    if (hart < FX1_NUM_HARTS && trap_handlers[hart])
        return trap_handlers[hart](mcause, mepc, mtval);
    /* Default: fatal. Interrupts are already masked in the trap. */
    const enum console_grant grant = console_take(FATAL_LOCK_TRIES);
    log_prefix();
    fx1_puts("unhandled trap mcause=");
    fx1_puthex(mcause);
    fx1_puts(" mepc=");
    fx1_puthex(mepc);
    fx1_puts(" mtval=");
    fx1_puthex(mtval);
    fx1_putc('\n');
    console_give(grant);
    const uint32_t code = 0xE000u | (mcause & 0xFFu);
    finish(((code & 0xFFFFu) << 16) | FX1_SIM_CTRL_FAIL);
}
