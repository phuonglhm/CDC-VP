/*
 * CSI-2 M5a: fx1_soc without --camera. The platform is unchanged except for
 * I2C0, which is always present:
 *   A  a load from MIPI_CSR is an access fault (disabled bus target, DECERR)
 *   B  I2C0 answers; a transfer to the sensor address 0x10 is not acknowledged
 *      (no device on the bus), reported on PLIC source 9
 *   C  PLIC source 7 (csi2_irq) never pends
 */
#include "fx1_fw.h"

/* I2C0 registers (components/i2c_tlm/include/i2c.h, a C++ header). */
#define I2C_INTR_STATE   0x00u
#define I2C_INTR_ENABLE  0x04u
#define I2C_CTRL         0x10u
#define I2C_STATUS       0x14u
#define I2C_FDATA        0x1cu
#define FDATA_START      (1u << 8)
#define FDATA_STOP       (1u << 9)
#define CTRL_ENABLEHOST  (1u << 0)
#define STATUS_HOSTIDLE  (1u << 3)
#define INTR_NAK         (1u << 4)
#define INTR_CMD_COMPLETE (1u << 9)

static volatile uint32_t got_cause, got_tval, traps, resume_at;

static uint32_t handler(uint32_t mcause, uint32_t mepc, uint32_t mtval)
{
    (void)mepc;
    got_cause = mcause;
    got_tval = mtval;
    ++traps;
    return resume_at;
}

int main(void)
{
    const uint32_t c0 = FX1_PLIC_CONTEXT_HART_M(0);
    fx1_global_irq_disable();
    fx1_set_trap_handler(handler);

    /* A */
    uint32_t value = 0;
    __asm__ volatile("la   t0, 1f\n"
                     "sw   t0, 0(%1)\n"
                     "lw   %0, 0(%2)\n"
                     "1:\n"
                     : "=&r"(value)
                     : "r"(&resume_at), "r"(FX1_MIPI_CSR_BASE)
                     : "t0", "memory");
    (void)value;
    fx1_check(traps == 1 && got_cause == 5u && got_tval == FX1_MIPI_CSR_BASE, 0x10,
              "A: MIPI_CSR load faults (no CSI-2 RX without --camera)");
    fx1_log("A: MIPI_CSR is a disabled target");

    /* B */
    fx1_plic_set_priority(FX1_IRQ_I2C0, 1);
    fx1_plic_enable(c0, FX1_IRQ_I2C0, 1);
    fx1_write32(FX1_I2C0_BASE + I2C_CTRL, CTRL_ENABLEHOST);
    fx1_write32(FX1_I2C0_BASE + I2C_INTR_STATE, 0xFFFFFFFFu);
    fx1_write32(FX1_I2C0_BASE + I2C_INTR_ENABLE, INTR_NAK);
    fx1_write32(FX1_I2C0_BASE + I2C_FDATA, FDATA_START | (0x10u << 1));
    fx1_write32(FX1_I2C0_BASE + I2C_FDATA, 0x01u);
    fx1_write32(FX1_I2C0_BASE + I2C_FDATA, 0x00u | FDATA_STOP);
    const uint64_t start = fx1_mtime();
    while (!(fx1_plic_pending() & (1u << FX1_IRQ_I2C0)) && fx1_mtime() - start < FX1_MTIME_HZ / 500u) {
    }
    fx1_check(fx1_plic_claim(c0) == FX1_IRQ_I2C0, 0x20, "B: NACK reported on PLIC source 9");
    fx1_check(fx1_read32(FX1_I2C0_BASE + I2C_INTR_STATE) & INTR_NAK, 0x21, "B: address 0x10 not acknowledged");
    while (!(fx1_read32(FX1_I2C0_BASE + I2C_STATUS) & STATUS_HOSTIDLE)) {
    }
    fx1_write32(FX1_I2C0_BASE + I2C_INTR_STATE, 0xFFFFFFFFu);
    fx1_plic_complete(c0, FX1_IRQ_I2C0);
    fx1_plic_enable(c0, FX1_IRQ_I2C0, 0);
    fx1_log("B: I2C0 present, no device at 0x10");

    /* C */
    fx1_plic_set_priority(FX1_IRQ_MIPI, 1);
    fx1_plic_enable(c0, FX1_IRQ_MIPI, 1);
    const uint64_t t0 = fx1_mtime();
    while (fx1_mtime() - t0 < FX1_MTIME_HZ / 10000u) { /* 100 us */
    }
    fx1_check(!(fx1_plic_pending() & (1u << FX1_IRQ_MIPI)) && fx1_plic_claim(c0) == 0, 0x30,
              "C: source 7 never pends");
    fx1_plic_enable(c0, FX1_IRQ_MIPI, 0);
    fx1_log("C: csi2_irq undriven");
    return 0;
}
