/* Custom-SoC interrupt demo: takes a CLINT timer interrupt (MTIP) AND a PLIC
 * external interrupt (MEIP) sourced from the I2C peripheral.
 *
 * Memory map (must match platforms/riscv_custom_soc):
 *   UART  TXDATA   @ 0x10000000
 *   CLINT mtimecmp @ 0x02004000, mtime @ 0x0200BFF8
 *   PLIC  priority @ 0x0C000000+4*id, enable @ 0x0C002000,
 *         threshold @ 0x0C200000, claim/complete @ 0x0C200004
 *   I2C   regs     @ 0x10010000 (PLIC source id 1)
 */

#define UART_TX            (*(volatile unsigned char *)0x10000000u)

#define CLINT_MTIMECMP_LO  (*(volatile unsigned int *)0x02004000u)
#define CLINT_MTIMECMP_HI  (*(volatile unsigned int *)0x02004004u)
#define CLINT_MTIME_LO     (*(volatile unsigned int *)0x0200BFF8u)
#define CLINT_MTIME_HI     (*(volatile unsigned int *)0x0200BFFCu)

#define PLIC_PRIORITY1     (*(volatile unsigned int *)0x0C000004u)
#define PLIC_ENABLE        (*(volatile unsigned int *)0x0C002000u)
#define PLIC_THRESHOLD     (*(volatile unsigned int *)0x0C200000u)
#define PLIC_CLAIM         (*(volatile unsigned int *)0x0C200004u)

#define I2C_IER            (*(volatile unsigned int *)0x10010000u)  /* IP enable    */
#define I2C_ICTLR          (*(volatile unsigned int *)0x10010004u)  /* START        */
#define I2C_IIER           (*(volatile unsigned int *)0x1001004Cu)  /* irq enable   */
#define I2C_IICR           (*(volatile unsigned int *)0x10010050u)  /* irq clear    */

#define MIE_MTIE     (1u << 7)
#define MIE_MEIE     (1u << 11)
#define MSTATUS_MIE  (1u << 3)

static void uart_puts(const char *s)
{
    while (*s) {
        UART_TX = (unsigned char)*s++;
    }
}

static volatile int timer_done = 0;
static volatile int ext_done = 0;

void __attribute__((interrupt("machine"))) trap_handler(void)
{
    unsigned mcause;
    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));
    const unsigned code = mcause & 0x7FFFFFFFu;
    unsigned tmp;

    if (code == 7) {            /* machine timer interrupt (CLINT) */
        uart_puts("TIMER\n");
        timer_done = 1;
        __asm__ volatile("csrrc %0, mie, %1" : "=r"(tmp) : "r"(MIE_MTIE));
    } else if (code == 11) {    /* machine external interrupt (PLIC) */
        unsigned id = PLIC_CLAIM;           /* claim highest-priority source */
        if (id == 1) {
            I2C_IICR = 0x1;                 /* clear the I2C interrupt source */
            uart_puts("EXT IRQ\n");
            ext_done = 1;
        }
        PLIC_CLAIM = id;                    /* complete */
    }
}

int main(void)
{
    uart_puts("Hello from custom SoC\n");

    __asm__ volatile("csrw mtvec, %0" ::"r"(trap_handler));

    /* PLIC: source 1 priority=1, enabled, threshold 0. */
    PLIC_PRIORITY1 = 1;
    PLIC_ENABLE = (1u << 1);
    PLIC_THRESHOLD = 0;

    /* Enable machine timer + external + global interrupts (rd != x0). */
    unsigned tmp;
    __asm__ volatile("csrrs %0, mie, %1" : "=r"(tmp) : "r"(MIE_MTIE | MIE_MEIE));
    __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE));

    /* Arm CLINT timer for ~100us from now. */
    unsigned hi = CLINT_MTIME_HI;
    unsigned lo = CLINT_MTIME_LO;
    CLINT_MTIMECMP_HI = hi;
    CLINT_MTIMECMP_LO = lo + 100u;

    /* Trigger the I2C peripheral to raise an interrupt -> PLIC external. */
    I2C_IIER = 0x1;     /* enable TRANSFER_DONE interrupt */
    I2C_IER = 0x1;      /* enable the IP */
    I2C_ICTLR = 0x1;    /* START -> TRANSFER_DONE -> irq line high */

    while (!(timer_done && ext_done)) {
        /* wait for both interrupts */
    }

    uart_puts("done\n");
    for (;;) {
    }
    return 0;
}
