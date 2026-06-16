/* Gate 2 demo: take a real machine-timer interrupt on the RISC-V core.
 *
 * Memory map (must match platforms/riscv_cpu_eval):
 *   UART  TXDATA @ 0x10000000
 *   TIMER CONTROL @ 0x10010000 (write 1 = arm; fires after the platform's interval)
 */

#define UART_TX     (*(volatile unsigned char *)0x10000000u)
#define TIMER_CTRL  (*(volatile unsigned int  *)0x10010000u)

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
    uart_puts("TIMER IRQ\n");
    fired = 1;
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

    TIMER_CTRL = 1u;            /* arm the platform timer */

    while (!fired) {
        /* busy-wait; interrupt is taken between instructions */
    }

    uart_puts("done\n");

    for (;;) {
        /* park until the simulation's time budget ends */
    }
    return 0;
}
