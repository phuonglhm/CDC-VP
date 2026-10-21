/*
 * FU3 PLIC IRQ test trên CVA6 (Common Spec v2.0 PLIC Connections):
 *   TIMER1 -> AP_PLIC Source ID 6 -> target 0 -> CVA6 irq_i[0] (mcause 11).
 * Handler claim, dừng TIMER1, complete. 
 * Kết quả in qua UART0.
 */
#include <stdint.h>

#include <fu3_timer/registers.h>
#include <fu3_plic/registers.h>

#define REG(addr)       (*(volatile uint32_t *)(addr))
#define UART_TX         (*(volatile unsigned char *)0x440D0000u)   /* UART0 */

#define MCAUSE_M_EXT    0x8000000Bu
#define MIE_MEIE        (1u << 11)
#define MSTATUS_MIE     (1u << 3)

static volatile unsigned claims = 0;
static volatile unsigned last_id = 0;
static volatile unsigned bad_trap = 0;

static void uart_puts(const char *s)
{
    while (*s) UART_TX = (unsigned char)*s++;
}

void __attribute__((interrupt("machine"), aligned(4))) trap_handler(void)
{
    uint32_t mcause;
    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));
    if (mcause != MCAUSE_M_EXT) {
        bad_trap = mcause;
        return;
    }
    const uint32_t id = REG(FU3_PLIC_BASE + FU3_PLIC_CLAIM_OFFSET);          // Claim 
    if (id == FU3_PLIC_ID_TIMER1) {
        REG(FU3_TIMER1_BASE + FU3_TIMER_CONTROL_OFFSET) = 0u;               // Stop TIMER1 
        REG(FU3_TIMER1_BASE + FU3_TIMER_COMPARE_OFFSET) = 0u;               // Hạ IRQ nguồn 
    }
    last_id = id;
    ++claims;
    REG(FU3_PLIC_BASE + FU3_PLIC_CLAIM_OFFSET) = id;                         // Complete 
}

int main(void)
{
    __asm__ volatile("csrw mtvec, %0" ::"r"(trap_handler));

    // PLIC: TIMER1 priority 3 > threshold 0, enable cho context 0 
    REG(FU3_PLIC_BASE + FU3_PLIC_PRIORITY_OFFSET(FU3_PLIC_ID_TIMER1)) = 3u;
    REG(FU3_PLIC_BASE + FU3_PLIC_THRESHOLD_OFFSET) = 0u;
    REG(FU3_PLIC_BASE + FU3_PLIC_ENABLE_OFFSET) = FU3_PLIC_SOURCE_BIT(FU3_PLIC_ID_TIMER1);

    unsigned tmp;
    __asm__ volatile("csrrs %0, mie, %1" : "=r"(tmp) : "r"(MIE_MEIE));
    __asm__ volatile("csrrs %0, mstatus, %1" : "=r"(tmp) : "r"(MSTATUS_MIE));

    REG(FU3_TIMER1_BASE + FU3_TIMER_COMPARE_OFFSET) = 3u;
    REG(FU3_TIMER1_BASE + FU3_TIMER_CONTROL_OFFSET) = FU3_TIMER_ENABLE;

    while (!claims && !bad_trap) {
    }
    for (volatile int i = 0; i < 2000; ++i) {
        // Thời gian sẽ lộ nếu ngắt bị gọi lại 
    }

    if (bad_trap)
        uart_puts("FAIL: unexpected trap\n");
    else if (claims != 1)
        uart_puts("FAIL: PLIC interrupt re-entered\n");
    else if (last_id != FU3_PLIC_ID_TIMER1)
        uart_puts("FAIL: wrong PLIC ID\n");
    else if (REG(FU3_PLIC_BASE + FU3_PLIC_CLAIM_OFFSET) != 0u)
        uart_puts("FAIL: PLIC still pending\n");
    else
        uart_puts("PLIC IRQ ID 6\ndone\n");

    for (;;) {
    }
    return 0;
}
