/*
 * CSI-2 M5b review F1/F2: budgets of the BSP I2C driver (fx1_soc --camera),
 * in polling and in interrupt mode, against the real I2C0 model and sensor:
 *   - a write whose bus time exceeds its budget returns FX1_I2C_ETIMEOUT
 *     within budget + one aborted byte and STOP (never OK late, never later);
 *   - the abort leaves the host idle: the next transfer works at once and a
 *     register written then reads back;
 *   - the same for a read cut while its bytes are received;
 *   - an abort that lands anywhere in a NACKed transaction, its STOP
 *     included (budgets 8..35 us to an absent address), never costs the
 *     next transaction (M5b re-review F2);
 *   - in interrupt mode completions are taken from PLIC source 9 and the
 *     source is left low.
 */
#include "fx1_ccs_sensor.h"
#include "fx1_fw.h"
#include "fx1_i2c.h"

static uint32_t us_since(uint64_t t) { return (uint32_t)((fx1_mtime() - t) / (FX1_MTIME_HZ / 1000000u)); }

static void run(int use_irq, uint32_t code)
{
    fx1_i2c_t bus;
    fx1_ccs s;
    fx1_check(fx1_i2c_open(&bus, FX1_I2C0_BASE, use_irq, FX1_IRQ_I2C0, FX1_PLIC_CONTEXT_HART_M(0)) == FX1_I2C_OK,
              code, "i2c timeout: open");
    /* 4-byte write: about 135 us at 400 kHz; budget 20 us. */
    const uint8_t w[4] = {0x03, 0x4C, 0x00, 0x20};
    uint64_t t = fx1_mtime();
    int rc = fx1_i2c_write(&bus, FX1_CCS_ADDR, w, 4, 20);
    uint32_t el = us_since(t);
    fx1_log_hex("  write: rc", (uint32_t)rc);
    fx1_log_hex("  write: elapsed us", el);
    fx1_check(rc == FX1_I2C_ETIMEOUT, code | 1u, "i2c timeout: write over budget -> ETIMEOUT");
    fx1_check(el >= 20u && el <= 20u + FX1_I2C_ABORT_US + 20u, code | 2u, "i2c timeout: returned at the budget");
    /* The next transfer at once: the abort left the bus idle. */
    t = fx1_mtime();
    fx1_check(fx1_ccs_probe(&s, &bus, FX1_CCS_ADDR, 2000) == FX1_CCS_OK, code | 3u, "i2c timeout: next transfer OK");
    fx1_log_hex("  next transfer us", us_since(t));
    fx1_check(fx1_ccs_write16(&s, 0x034E, 0x0123) == FX1_CCS_OK, code | 4u, "i2c timeout: register write");
    uint16_t v = 0;
    fx1_check(fx1_ccs_read16(&s, 0x034E, &v) == FX1_CCS_OK && v == 0x0123u, code | 5u, "i2c timeout: read back");
    /* A read cut while it receives: 64 bytes (~1.5 ms), budget 300 us. */
    const uint8_t idx[2] = {0x00, 0x00};
    uint8_t r[64];
    t = fx1_mtime();
    rc = fx1_i2c_write_read(&bus, FX1_CCS_ADDR, idx, 2, r, 64, 300);
    el = us_since(t);
    fx1_log_hex("  read: rc", (uint32_t)rc);
    fx1_log_hex("  read: elapsed us", el);
    fx1_check(rc == FX1_I2C_ETIMEOUT && el <= 300u + FX1_I2C_ABORT_US + 20u, code | 6u, "i2c timeout: read cut");
    fx1_check(fx1_ccs_probe(&s, &bus, FX1_CCS_ADDR, 2000) == FX1_CCS_OK, code | 7u, "i2c timeout: OK after the read");
    /* Absent address: START + address 25 us, NACK, STOP 25..27.5 us at 400
     * kHz. Every budget from 8 to 35 us puts the abort before, during or
     * after that STOP; the valid write that follows must always work. */
    const uint8_t nw[3] = {0x01, 0x00, 0x00};
    for (uint32_t b = 8; b <= 35u; ++b) {
        rc = fx1_i2c_write(&bus, 0x22, nw, 3, b);
        if (rc != FX1_I2C_ETIMEOUT && rc != FX1_I2C_ENACK) fx1_log_hex("  absent address rc", (uint32_t)rc);
        fx1_check(rc == FX1_I2C_ETIMEOUT || rc == FX1_I2C_ENACK, code | 0xAu, "i2c timeout: absent address");
        if (fx1_ccs_write16(&s, 0x034E, (uint16_t)b) != FX1_CCS_OK) fx1_log_hex("  failed after budget us", b);
        fx1_check(fx1_ccs_read16(&s, 0x034E, &v) == FX1_CCS_OK && v == b, code | 0xBu,
                  "i2c timeout: the transaction after an abort in a NACK runs");
    }
    fx1_check(!(fx1_plic_pending() & (1u << FX1_IRQ_I2C0)), code | 8u, "i2c timeout: source 9 low");
    if (use_irq) {
        fx1_log_hex("  completions claimed at the PLIC", bus.irq_claims);
        fx1_check(bus.irq_claims >= 4u, code | 9u, "i2c timeout: completions through PLIC source 9");
    }
}

int main(void)
{
    fx1_global_irq_disable();
    run(0, 0x100);
    fx1_log("i2c timeout: polling mode, write and read aborted at their budget, bus recovered");
    run(1, 0x200);
    fx1_log("i2c timeout: interrupt mode, same, completions through PLIC source 9");
    return 0;
}
