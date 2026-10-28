// Plan T20 console demo on Ibex (VP verification firmware, not a Boot ROM),
// run by vp_fu3_soc from the command line: ROM stub (ibex_rom.S) -> this image
// in IBEX_ISRAM. It shows what the CLI gives firmware:
//   - console on UART0 (dies 48/49, select b00): banner and results;
//   - straps, DEBUG_EN and reset cause from FU3_VP_ABI_1 BOOT_STATUS/RESET_CAUSE;
//   - one console input line (--uart-rx-file), echoed back;
//   - the first word of the QSPI NOR (--flash) read through XIP;
//   - a signature word in DSRAM at 0x0103_F000 (--dump);
//   - "PASS" at the end (--expect-uart PASS).
#include "fw_common.h"
#include "fu3_vp_abi.h"

struct fw_mailbox *const fw_box = FW_MAILBOX_IBEX;

#define REG(addr) (*(volatile uint32_t *)(uintptr_t)(addr))
#define REG8(addr) (*(volatile uint8_t *)(uintptr_t)(addr))
#define UART0 0x440d0000u
#define UART_THR (UART0 + 0)
#define UART_RBR (UART0 + 0)
#define UART_DLL (UART0 + 0)
#define UART_DLM (UART0 + 1)
#define UART_FCR (UART0 + 2)
#define UART_LCR (UART0 + 3)
#define UART_LSR (UART0 + 5)
#define QSPI 0x44000000u
#define XIP 0x30000000u
#define SIGNATURE REG(0x0103f000u)

static void putc_(char c)
{
    while (!(REG8(UART_LSR) & 0x20))
        ;
    REG8(UART_THR) = (uint8_t)c;
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

static int getc_(unsigned limit)
{
    for (unsigned i = 0; i < limit; ++i)
        if (REG8(UART_LSR) & 0x01)
            return REG8(UART_RBR);
    return -1;
}

int main(void)
{
    // UART0: divisor 1 (one bit = 16 bus clocks), 8N1, FIFOs. The boot
    // console format (8P1 parity) is still to be agreed with FW/SW.
    REG8(UART_LCR) = 0x80;
    REG8(UART_DLL) = 1;
    REG8(UART_DLM) = 0;
    REG8(UART_LCR) = 0x03;
    REG8(UART_FCR) = 0x07;
    puts_("FU3 VP T20 console\n");

    const uint32_t boot = REG(FU3_VP_ABI_SYSCSR_BASE + FU3_VP_BOOT_STATUS_OFFSET);
    puts_("straps MODE=");
    putc_((boot & FU3_VP_BOOT_MODE_1) ? '1' : '0');
    putc_((boot & FU3_VP_BOOT_MODE_0) ? '1' : '0');
    puts_(" BOOT=");
    putc_((boot & FU3_VP_BOOT_BOOT) ? '1' : '0');
    puts_(" DEBUG_EN=");
    putc_((boot & FU3_VP_BOOT_DEBUG_EN) ? '1' : '0');
    puts_(" valid=");
    putc_((boot & FU3_VP_BOOT_STRAPS_VALID) ? '1' : '0');
    puts_("\nreset cause ");
    puthex(REG(FU3_VP_ABI_SYSCSR_BASE + FU3_VP_RESET_CAUSE_OFFSET));
    putc_('\n');

    // One console line, echoed back (empty if nothing arrives within about
    // 5 ms of simulated time per character).
    char line[64];
    unsigned n = 0;
    for (int c; n + 1 < sizeof line && (c = getc_(4000)) >= 0 && c != '\n';)
        line[n++] = (char)c;
    line[n] = 0;
    puts_("rx: ");
    puts_(line);
    putc_('\n');

    // QSPI NOR through XIP: 24-bit address, single line READ (0x03), memory-mapped.
    REG(QSPI + 0x04) = 23u << 16;   // DCR: FSIZE 2^(23+1) bytes
    REG(QSPI + 0x00) = 1;           // CR: EN
    REG(QSPI + 0x14) = (3u << 26) | (1u << 24) | (2u << 12) | (1u << 10) | (1u << 8) | 0x03;
    puts_("flash ");
    puthex(REG(XIP));
    putc_('\n');

    SIGNATURE = 0xc0ffee20u;
    puts_("PASS\n");
    for (;;)
        __asm__ volatile("wfi");
    return 0;
}
