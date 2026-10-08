/* Gate 2 demo: take a real machine-timer interrupt on the RISC-V core.
 *
 * Memory map (must match platforms/riscv_cpu_eval):
 *   UART  TXDATA @ 0x10000000
 *   TIMER @ 0x10010000 (components/timer_tlm): CTRL +0x0, RELOAD +0x8,
 *         INTSTATUS +0xC (write 1 to clear). CTRL bit 0 = ENABLE, bit 3 = INTR_EN.
 *
 * The handler clears the interrupt at the timer, so the level-sensitive MTIP
 * drops before mret and the interrupt is taken once per timer event.
 */

#define UART_TX     (*(volatile unsigned char *)0x10000000u)
#define TIMER_CTRL      (*(volatile unsigned int  *)0x10010000u)
#define TIMER_RELOAD    (*(volatile unsigned int  *)0x10010008u)
#define TIMER_INTSTATUS (*(volatile unsigned int  *)0x1001000Cu)
#define TIMER_ENABLE    0x1u
#define TIMER_INTR_EN   0x8u

#define MIE_MTIE      (1u << 7)   /* machine timer interrupt enable */
#define MSTATUS_MIE   (1u << 3)   /* global machine interrupt enable */

static void uart_puts(const char *s)
{
    while (*s) {
        UART_TX = (unsigned char)*s++;
    }
}

static volatile int fired = 0;

/* The "interrupt" attribute makes GCC emit the register save/restore and mret. */
void __attribute__((interrupt("machine"))) trap_handler(void)
{
    TIMER_INTSTATUS = 1u;     /* clear at the device first (timer_tlm acts on a
                                 clear only while enabled): MTIP drops */
    TIMER_CTRL = 0u;          /* then stop: one event is all this demo needs */
    ++fired;
    if (fired == 1) uart_puts("TIMER IRQ\n");
}

int main(void)
{
    uart_puts("Hello from RISC-V\n");

    /* Install the trap vector (direct mode: low bits 0). */
    __asm__ volatile("csrw mtvec, %0" ::"r"(trap_handler));

    /* Enable machine timer interrupt and global interrupts.
     *
     * NOTE: this mariusmm core skips CSRRS entirely when rd==x0, so the usual
     * `csrs csr, rs` pseudo-instruction (= csrrs x0, csr, rs) is a no-op here.
     * Force a non-x0 destination register (the `=r` output) so the set is applied. */
    unsigned tmp;
    __asm__ volatile("csrrs %0, mie, %1" : "=r"(tmp) : "r"(MIE_MTIE));
    __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE));

    TIMER_RELOAD = 2u;
    TIMER_CTRL = TIMER_ENABLE | TIMER_INTR_EN;  /* arm the platform timer */

    while (!fired) {
        /* busy-wait; interrupt is taken between instructions */
    }
    for (volatile int i = 0; i < 2000; ++i) {
        /* give a stuck interrupt line time to re-enter the handler */
    }

    uart_puts(fired == 1 ? "done\n" : "FAIL: timer interrupt re-entered\n");

    for (;;) {
        /* park until the simulation's time budget ends */
    }
    return 0;
}
