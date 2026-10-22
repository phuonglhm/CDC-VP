/*
 * CSI-2 M5b: the I2C0 driver in interrupt mode (fx1_soc --camera): the
 * sensor probed and programmed with every completion taken from PLIC source
 * 9 (claimed and completed by the driver, MIE off); a NACK of an absent
 * address reported as FX1_I2C_ENACK; source 9 low afterwards.
 */
#include "fx1_ccs_sensor.h"
#include "fx1_fw.h"
#include "fx1_i2c.h"

int main(void)
{
    const uint32_t c0 = FX1_PLIC_CONTEXT_HART_M(0);
    fx1_global_irq_disable();
    fx1_irq_enable(FX1_MIE_MEIE);
    fx1_i2c_t bus;
    fx1_ccs s;
    fx1_check(fx1_i2c_open(&bus, FX1_I2C0_BASE, 1, FX1_IRQ_I2C0, c0) == FX1_I2C_OK, 0x10, "i2c irq: open");
    const uint64_t t0 = fx1_mtime();
    fx1_check(fx1_ccs_probe(&s, &bus, FX1_CCS_ADDR, 2000) == FX1_CCS_OK, 0x11, "i2c irq: model_id 0xF1C5");
    const fx1_ccs_mode m = {32, 4, 1000, 8, 12, 2};
    fx1_check(fx1_ccs_set_mode(&s, &m) == FX1_CCS_OK, 0x12, "i2c irq: mode written and read back");
    uint16_t fmt = 0;
    fx1_check(fx1_ccs_read16(&s, 0x0112, &fmt) == FX1_CCS_OK && fmt == 0x0C0Cu, 0x13, "i2c irq: RAW12 format");
    fx1_log_hex("i2c irq: 8 transactions in us",
                (uint32_t)((fx1_mtime() - t0) / (FX1_MTIME_HZ / 1000000u)));
    const uint8_t b[3] = {0x01, 0x00, 0x01};
    fx1_check(fx1_i2c_write(&bus, 0x22, b, 3, 2000) == FX1_I2C_ENACK, 0x14, "i2c irq: NACK of address 0x22");
    fx1_check(fx1_ccs_probe(&s, &bus, FX1_CCS_ADDR, 2000) == FX1_CCS_OK, 0x15, "i2c irq: next transfer works");
    fx1_check(!(fx1_plic_pending() & (1u << FX1_IRQ_I2C0)), 0x16, "i2c irq: source 9 low");
    fx1_log_hex("i2c irq: completions claimed at the PLIC", bus.irq_claims);
    fx1_check(bus.irq_claims >= 10u, 0x17, "i2c irq: every transfer completed through source 9");
    fx1_log("i2c irq: driver transfers completed on PLIC source 9 interrupts");
    return 0;
}
