/*
 * CSI-2 M5a: camera integration of fx1_soc, run with --camera. Checks the
 * wiring firmware relies on, through the real bus, PLIC and DDR:
 *   A  CSI-2 RX CSR at FX1_MIPI_CSR_BASE: the reference driver probes it
 *   B  I2C0 at FX1_I2C0_BASE, PLIC source 9: read the sensor's model_id
 *      (CCS 0x0000) over I2C, completion signalled through the PLIC
 *   C  CSI-2 RX DMA (bus initiator MIPI_DMA) and csi2_irq on PLIC source 7:
 *      one TPG frame 32x8 RAW10 to DDR, BUF_DONE_OK claimed at the PLIC, the
 *      image equal to the TPG colour bars (MAS 6.5)
 * Polled claims with MIE off, as in plic_irq B. The sensor's own frames go
 * through the D-PHY and are covered by the M5b camera smokes.
 */
#include "fx1_fw.h"
#include "fx1_csi2/fx1_csi2_drv.h"
#include "fx1_csi2/fx1_csi2_regs.h"

/* I2C0 registers (components/i2c_tlm/include/i2c.h, a C++ header). */
#define I2C_INTR_STATE   0x00u
#define I2C_INTR_ENABLE  0x04u
#define I2C_CTRL         0x10u
#define I2C_STATUS       0x14u
#define I2C_RDATA        0x18u
#define I2C_FDATA        0x1cu
#define FDATA_START      (1u << 8)
#define FDATA_STOP       (1u << 9)
#define FDATA_READB      (1u << 10)
#define CTRL_ENABLEHOST  (1u << 0)
#define STATUS_HOSTIDLE  (1u << 3)
#define STATUS_RXEMPTY   (1u << 5)
#define INTR_NAK         (1u << 4)
#define INTR_CMD_COMPLETE (1u << 9)

#define SENSOR_ADDR 0x10u   /* 7-bit */
#define CCS_MODEL_ID 0xF1C5u

#define TPG_W 32u
#define TPG_H 8u
#define BPL 40u              /* RAW10 packed: 5 bytes per 4 pixels */
#define STRIDE 48u
#define BUF0 FX1_FW_RAW_BUF_BASE
#define BUF1 (FX1_FW_RAW_BUF_BASE + 0x1000u)

static uint32_t csi_rd(void* ctx, uint32_t off)
{
    (void)ctx;
    return fx1_read32(FX1_MIPI_CSR_BASE + off);
}
static void csi_wr(void* ctx, uint32_t off, uint32_t value)
{
    (void)ctx;
    fx1_write32(FX1_MIPI_CSR_BASE + off, value);
}
static void delay_us(void* ctx, uint32_t us)
{
    (void)ctx;
    const uint64_t start = fx1_mtime();
    while (fx1_mtime() - start < (uint64_t)us * (FX1_MTIME_HZ / 1000000u)) {
    }
}

static void i2c_wr(uint32_t off, uint32_t v) { fx1_write32(FX1_I2C0_BASE + off, v); }
static uint32_t i2c_rd(uint32_t off) { return fx1_read32(FX1_I2C0_BASE + off); }

/* Polled claim of `source` on hart 0, at most `ms` of simulated time. */
static uint32_t claim_source(uint32_t source, uint32_t ms)
{
    const uint64_t start = fx1_mtime();
    while (fx1_mtime() - start < (uint64_t)ms * (FX1_MTIME_HZ / 1000u)) {
        if (fx1_plic_pending() & (1u << source)) return fx1_plic_claim(FX1_PLIC_CONTEXT_HART_M(0));
    }
    return 0;
}

/* TPG colour bars of MAS Table 625, RGGB, 10-bit 0x3FF/0, packed RAW10. */
static uint8_t tpg_byte(uint32_t y, uint32_t i)
{
    static const uint8_t R[8] = {1, 1, 0, 0, 1, 1, 0, 0}, G[8] = {1, 1, 1, 1, 0, 0, 0, 0},
                         B[8] = {1, 0, 1, 0, 1, 0, 1, 0};
    const uint32_t group = i / 5u, k = i % 5u;
    uint8_t lsb = 0;
    for (uint32_t p = 0; p < 4u; ++p) {
        const uint32_t x = group * 4u + p;
        uint32_t band = x / (TPG_W >> 3);
        if (band > 7u) band = 7u;
        const uint8_t on = (y % 2u == 0u && x % 2u == 0u) ? R[band] : (y % 2u == 1u && x % 2u == 1u) ? B[band] : G[band];
        if (k == p) return on ? 0xFFu : 0x00u;
        if (on) lsb |= (uint8_t)(3u << (2u * p));
    }
    return lsb;
}

int main(void)
{
    const uint32_t c0 = FX1_PLIC_CONTEXT_HART_M(0);
    fx1_global_irq_disable();

    /* A: CSR */
    fx1_csi2_hal hal = {csi_rd, csi_wr, delay_us, 0};
    fx1_csi2_dev dev;
    fx1_check(fx1_csi2_probe(&dev, &hal) == FX1_CSI2_OK, 0x10, "A: CSI-2 RX probed at FX1_MIPI_CSR_BASE");
    fx1_log_hex("A: IP_VERSION", fx1_read32(FX1_MIPI_CSR_BASE + FX1_CSI2_IP_VERSION));

    /* B: I2C0, source 9 */
    fx1_plic_set_priority(FX1_IRQ_I2C0, 1);
    fx1_plic_enable(c0, FX1_IRQ_I2C0, 1);
    i2c_wr(I2C_CTRL, CTRL_ENABLEHOST);
    i2c_wr(I2C_INTR_STATE, 0xFFFFFFFFu);
    i2c_wr(I2C_INTR_ENABLE, INTR_CMD_COMPLETE | INTR_NAK);
    fx1_check(fx1_plic_pending() == 0, 0x20, "B: nothing pending before the transfer");
    i2c_wr(I2C_FDATA, FDATA_START | (SENSOR_ADDR << 1));
    i2c_wr(I2C_FDATA, 0x00u);
    i2c_wr(I2C_FDATA, 0x00u);
    i2c_wr(I2C_FDATA, FDATA_START | (SENSOR_ADDR << 1) | 1u);
    i2c_wr(I2C_FDATA, FDATA_READB | FDATA_STOP | 2u);
    const uint32_t id9 = claim_source(FX1_IRQ_I2C0, 2);
    fx1_check(id9 == FX1_IRQ_I2C0, 0x21, "B: I2C0 completion claimed at PLIC source 9");
    const uint32_t intr = i2c_rd(I2C_INTR_STATE);
    fx1_check((intr & INTR_CMD_COMPLETE) && !(intr & INTR_NAK), 0x22, "B: transfer complete, acknowledged");
    while (!(i2c_rd(I2C_STATUS) & STATUS_HOSTIDLE)) {
    }
    uint32_t model = 0, n = 0;
    while (!(i2c_rd(I2C_STATUS) & STATUS_RXEMPTY) && n < 4u) {
        model = (model << 8) | (i2c_rd(I2C_RDATA) & 0xFFu);
        ++n;
    }
    fx1_check(n == 2u && model == CCS_MODEL_ID, 0x23, "B: sensor model_id 0xF1C5 read over I2C");
    i2c_wr(I2C_INTR_STATE, 0xFFFFFFFFu); /* clear at the device, then complete */
    fx1_plic_complete(c0, FX1_IRQ_I2C0);
    fx1_check(!(fx1_plic_pending() & (1u << FX1_IRQ_I2C0)), 0x24, "B: source 9 low after the clear");
    fx1_plic_enable(c0, FX1_IRQ_I2C0, 0);
    fx1_log("B: I2C0 transfer to the camera sensor, interrupt on source 9");

    /* C: TPG frame -> DMA (MIPI_DMA) -> DDR, csi2_irq on source 7 */
    for (uint32_t i = 0; i < 0x2000u; i += 4u) fx1_write32(BUF0 + i, 0xA5A5A5A5u);
    fx1_dma_prepare((const volatile void*)BUF0, 0x2000u, FX1_DMA_FROM_DEVICE);
    fx1_csi2_dma_cfg cfg = {{BUF0, BUF1}, 0x1000u, BPL, STRIDE - BPL, 255u, 0u, 0u, 0u, 0u, 1u};
    fx1_check(fx1_csi2_stream_config(&dev, 0, 0) == FX1_CSI2_OK, 0x30, "C: stream path off");
    fx1_check(fx1_csi2_dma_config(&dev, &cfg) == FX1_CSI2_OK, 0x31, "C: DMA configured");
    fx1_check(fx1_csi2_dma_start(&dev) == FX1_CSI2_OK, 0x32, "C: DMA started");
    fx1_check(fx1_csi2_irq_enable(&dev, 1u << 16) == FX1_CSI2_OK, 0x33, "C: BUF_DONE_OK enabled"); /* IRQ_BUF_DONE_OK */
    fx1_plic_set_priority(FX1_IRQ_MIPI, 2);
    fx1_plic_enable(c0, FX1_IRQ_MIPI, 1);
    csi_wr(0, FX1_CSI2_TPG_RESOLUTION, (TPG_H << 16) | TPG_W);
    csi_wr(0, FX1_CSI2_TPG_CONTROL, FX1_CSI2_TPG_CONTROL_TPG_ENABLE_MASK | FX1_CSI2_TPG_CONTROL_TPG_PIXEL_FORMAT_MASK);
    csi_wr(0, FX1_CSI2_TPG_CONTROL, 0); /* the frame in progress completes: exactly one */
    const uint32_t id7 = claim_source(FX1_IRQ_MIPI, 2);
    fx1_check(id7 == FX1_IRQ_MIPI, 0x34, "C: csi2_irq claimed at PLIC source 7");
    const uint32_t pending = fx1_csi2_irq_handle(&dev);
    fx1_check(pending & (1u << 16), 0x35, "C: BUF_DONE_OK pending");
    fx1_plic_complete(c0, FX1_IRQ_MIPI);
    fx1_csi2_buf_info bi;
    fx1_csi2_get_buffer(&dev, 0, &bi);
    fx1_check(bi.state == FX1_CSI2_BUF_SW_OWNED_VALID, 0x36, "C: side 0 handed to software, valid");
    fx1_dma_complete((const volatile void*)BUF0, 0x1000u, FX1_DMA_FROM_DEVICE);
    uint32_t bad = 0;
    for (uint32_t y = 0; y < TPG_H; ++y) {
        for (uint32_t i = 0; i < BPL; ++i)
            bad += *(volatile uint8_t*)(BUF0 + y * STRIDE + i) != tpg_byte(y, i);
        for (uint32_t i = BPL; i < STRIDE; ++i) bad += *(volatile uint8_t*)(BUF0 + y * STRIDE + i) != 0xA5u;
    }
    fx1_check(bad == 0, 0x37, "C: DDR image = TPG colour bars, line padding untouched");
    fx1_check(fx1_csi2_buffer_release(&dev, 0) == FX1_CSI2_OK, 0x38, "C: side 0 released");
    fx1_check(fx1_csi2_dma_stop(&dev, 100) == FX1_CSI2_OK, 0x39, "C: DMA stopped");
    fx1_plic_enable(c0, FX1_IRQ_MIPI, 0);
    fx1_log("C: TPG frame through MIPI_DMA to DDR, interrupt on source 7");
    return 0;
}
