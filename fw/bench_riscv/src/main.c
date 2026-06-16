/* Compute-bound benchmark firmware for quantum-keeper measurements.
 *
 * Runs a tight ALU loop forever (no WFI, no halt). The platform bounds it with a
 * fixed simulated time (--sim-ms) and measures host wall-clock + retired
 * instructions to derive MIPS. The volatile accumulator forces real work so the
 * loop is not optimized away.
 */

#define UART_TX (*(volatile unsigned char *)0x10000000u)

static void uart_puts(const char *s)
{
    while (*s) {
        UART_TX = (unsigned char)*s++;
    }
}

int main(void)
{
    uart_puts("BENCH START\n");

    volatile unsigned long acc = 0;
    unsigned long i = 0;
    for (;;) {
        acc += i * i + 7u;
        i++;
    }
    return 0;
}
