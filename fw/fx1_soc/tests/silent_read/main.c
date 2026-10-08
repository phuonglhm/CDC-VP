/*
 * Image for test_fx1_cpu_silent_target (review G2-R2), not a stand-alone test.
 * Reads a register whose target never answers, then claims PASS. A correct
 * platform stops at the read with an integration error, so the PASS write must
 * never happen. Uses raw MMIO only: the harness has no UART.
 */
#include "fx1_fw.h"

int main(void)
{
    (void)fx1_read32(FX1_SYS_DMA_CSR_BASE);
    fx1_write32(FX1_SIM_CTRL_BASE + FX1_SIM_CTRL_FINISH_OFF, FX1_SIM_CTRL_PASS);
    for (;;) fx1_wfi();
}
