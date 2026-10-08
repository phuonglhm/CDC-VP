/*
 * G1-R3: sub-word guest accesses to the UART are refused with an access fault
 * (the platform guard), never forwarded to a model that copies 4 bytes.
 * lb/lh must raise a load access fault (mcause 5), sb/sh a store/AMO access
 * fault (mcause 7), with mtval = the address. 32-bit console output keeps
 * working afterwards.
 */
#include "fx1_fw.h"

#define UART_FR (FX1_UART_BASE + 0x18u)
#define UART_DR (FX1_UART_BASE + 0x00u)

static volatile uint32_t faults, last_cause, last_tval;

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    if (mcause != 5u && mcause != 7u) {
        fx1_log_hex("unexpected mcause", mcause);
        fx1_fail(0x0E0);
    }
    ++faults;
    last_cause = mcause;
    last_tval = mtval;
    return mepc + 4; /* lb/lh/sb/sh are 32-bit encodings (no Zcb) */
}

static void expect_fault(uint32_t n, uint32_t cause, uint32_t address, uint32_t code)
{
    fx1_check(faults == n && last_cause == cause && last_tval == address, code,
              "sub-word UART access faulted with the right cause and address");
}

int main(void)
{
    fx1_set_trap_handler(handler);
    uint32_t value = 0;
    const uint32_t fr = UART_FR, dr = UART_DR;

    __asm__ volatile("lb %0, 0(%1)" : "=r"(value) : "r"(fr) : "memory");
    expect_fault(1, 5, fr, 0x01);
    __asm__ volatile("lh %0, 0(%1)" : "=r"(value) : "r"(fr) : "memory");
    expect_fault(2, 5, fr, 0x02);
    __asm__ volatile("sb %0, 0(%1)" ::"r"('X'), "r"(dr) : "memory");
    expect_fault(3, 7, dr, 0x03);
    __asm__ volatile("sh %0, 0(%1)" ::"r"('Y'), "r"(dr) : "memory");
    expect_fault(4, 7, dr, 0x04);
    (void)value;

    fx1_log("sub-word UART accesses trapped; word access still works");
    fx1_check(faults == 4, 0x05, "no further faults from 32-bit console output");
    return 0;
}
