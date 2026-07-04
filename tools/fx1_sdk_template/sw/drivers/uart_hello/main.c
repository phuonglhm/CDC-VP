/*
 * uart_hello - smoke-test driver for the VP_FX1 SDK.
 *
 * Proves the full firmware path works end to end:
 *   BSP headers -> HAL -> MMIO -> uart2_tlm console -> VP stdout.
 *
 * Build:  make            (needs riscv-none-elf- on PATH; see bsp/toolchain.md)
 * Run:    ../../../vp/run_vp.sh uart_hello.elf
 * Expect: "VP_FX1 SDK: UART console up" on the VP console.
 */
#include "soc_memory_map.h"
#include "uart.h"

int main(void)
{
    uart_puts(CDC_UART0_BASE, "VP_FX1 SDK: UART console up\n");
    uart_puts(CDC_UART0_BASE, "driver template OK\n");
    return 0;
}
