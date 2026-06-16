/* CLINT timer-interrupt demo for the riscv_custom_soc platform.
 *
 * Memory map (must match platforms/riscv_custom_soc):
 *   UART  TXDATA   @ 0x10000000
 *   CLINT mtimecmp @ 0x02004000 (64-bit), mtime @ 0x0200BFF8 (64-bit)
 *
 * Programs mtimecmp = mtime + 100us, enables the machine timer interrupt, and
 * takes one timer interrupt (the handler prints "TIMER" and disables MTIE).
 */

#define UART_TX            (*(volatile unsigned char *)0x10000000u)
#define CLINT_MTIMECMP_LO  (*(volatile unsigned int  *)0x02004000u)
#define CLINT_MTIMECMP_HI  (*(volatile unsigned int  *)0x02004004u)
#define CLINT_MTIME_LO     (*(volatile unsigned int  *)0x0200BFF8u)
#define CLINT_MTIME_HI     (*(volatile unsigned int  *)0x0200BFFCu)

#define MIE_MTIE     (1u << 7)
#define MSTATUS_MIE  (1u << 3)

static void uart_puts(const char *s)
{
    while (*s) {
        UART_TX = (unsigned char)*s++;
    }
}

static volatile int ticks = 0;

void __attribute__((interrupt("machine"))) trap_handler(void)
{
    uart_puts("TIMER\n");
    ticks++;
    /* One-shot demo: disable the machine timer interrupt so it is not re-taken.
       (rd != x0 to work around the mariusmm CSRRS bug.) */
    unsigned tmp;
    __asm__ volatile("csrrc %0, mie, %1" : "=r"(tmp) : "r"(MIE_MTIE));
}

int main(void)
{
    uart_puts("Hello from custom SoC\n");

    __asm__ volatile("csrw mtvec, %0" ::"r"(trap_handler));

    /* Program mtimecmp = mtime + 100us. mtime high word is 0 this early. */
    unsigned hi = CLINT_MTIME_HI;
    unsigned lo = CLINT_MTIME_LO;
    CLINT_MTIMECMP_HI = hi;
    CLINT_MTIMECMP_LO = lo + 100u;

    unsigned tmp;
    __asm__ volatile("csrrs %0, mie, %1" : "=r"(tmp) : "r"(MIE_MTIE));
    __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE));

    while (!ticks) {
        /* wait for the timer interrupt */
    }

    uart_puts("done\n");
    for (;;) {
    }
    return 0;
}
