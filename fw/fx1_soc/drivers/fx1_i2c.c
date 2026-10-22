#include "fx1_i2c.h"

#include "fx1_fw.h"

/* Register map of components/i2c_tlm/include/i2c.h (a C++ header). */
#define I2C_INTR_STATE   0x00u
#define I2C_INTR_ENABLE  0x04u
#define I2C_CTRL         0x10u
#define I2C_STATUS       0x14u
#define I2C_RDATA        0x18u
#define I2C_FDATA        0x1cu
#define I2C_FIFO_CTRL    0x20u
#define FDATA_START      (1u << 8)
#define FDATA_STOP       (1u << 9)
#define FDATA_READB      (1u << 10)
#define FDATA_RCONT      (1u << 11)
#define CTRL_ENABLEHOST  (1u << 0)
#define STATUS_HOSTIDLE  (1u << 3)
#define STATUS_RXEMPTY   (1u << 5)
#define INTR_NAK         (1u << 4)
#define INTR_CMD_COMPLETE (1u << 9)
#define FIFO_CTRL_RXRST  (1u << 0)
#define FIFO_CTRL_FMTRST (1u << 1)

static uint32_t rd(const fx1_i2c_t* bus, uint32_t off) { return fx1_read32(bus->base + off); }
static void wr(const fx1_i2c_t* bus, uint32_t off, uint32_t v) { fx1_write32(bus->base + off, v); }

static uint64_t ticks(uint32_t us) { return (uint64_t)us * (FX1_MTIME_HZ / 1000000u); }

int fx1_i2c_open(fx1_i2c_t* bus, uintptr_t base, int use_irq, uint32_t irq_source, uint32_t plic_context)
{
    if (!bus) return FX1_I2C_EINVAL;
    bus->base = base;
    bus->use_irq = use_irq;
    bus->irq_source = irq_source;
    bus->plic_context = plic_context;
    bus->irq_claims = 0;
    wr(bus, I2C_CTRL, CTRL_ENABLEHOST);
    wr(bus, I2C_FIFO_CTRL, FIFO_CTRL_RXRST | FIFO_CTRL_FMTRST);
    wr(bus, I2C_INTR_STATE, 0xFFFFFFFFu);
    wr(bus, I2C_INTR_ENABLE, use_irq ? (INTR_CMD_COMPLETE | INTR_NAK) : 0u);
    if (use_irq) {
        fx1_plic_set_priority(irq_source, 1);
        fx1_plic_enable(plic_context, irq_source, 1);
    }
    return FX1_I2C_OK;
}

static int idle(const fx1_i2c_t* bus) { return (rd(bus, I2C_STATUS) & STATUS_HOSTIDLE) != 0; }

/* Waits up to `us` for the host to be idle. */
static int wait_idle(const fx1_i2c_t* bus, uint32_t us)
{
    const uint64_t start = fx1_mtime();
    while (!idle(bus))
        if (fx1_mtime() - start > ticks(us)) return 0;
    return 1;
}

/* Interrupt state cleared at the device, then the PLIC source claimed and
 * completed if it pends (use_irq). */
static void ack_irq(fx1_i2c_t* bus)
{
    wr(bus, I2C_INTR_STATE, INTR_CMD_COMPLETE | INTR_NAK);
    if (bus->use_irq && (fx1_plic_pending() & (1u << bus->irq_source))) {
        const uint32_t id = fx1_plic_claim(bus->plic_context);
        if (id == bus->irq_source) ++bus->irq_claims;
        if (id) fx1_plic_complete(bus->plic_context, id);
    }
}

/* One transaction has been queued: wait for its STOP, polling, bounded by the
 * budget. The end is the I2C interrupt at the PLIC with use_irq, the
 * controller's interrupt state otherwise; HOSTIDLE confirms the STOP. */
static int finish(fx1_i2c_t* bus, uint8_t* rdata, uint32_t rlen, uint64_t start, uint32_t timeout_us)
{
    uint32_t got = 0;
    for (;;) {
        while (rdata && got < rlen && !(rd(bus, I2C_STATUS) & STATUS_RXEMPTY))
            rdata[got++] = (uint8_t)rd(bus, I2C_RDATA);
        const int signalled = bus->use_irq ? (fx1_plic_pending() & (1u << bus->irq_source)) != 0 : 1;
        const uint32_t st = rd(bus, I2C_INTR_STATE);
        if (signalled && (st & INTR_CMD_COMPLETE) && idle(bus)) {
            while (rdata && got < rlen && !(rd(bus, I2C_STATUS) & STATUS_RXEMPTY))
                rdata[got++] = (uint8_t)rd(bus, I2C_RDATA);
            ack_irq(bus);
            if (st & INTR_NAK) return FX1_I2C_ENACK;
            return got == rlen ? FX1_I2C_OK : FX1_I2C_ETIMEOUT;
        }
        if (fx1_mtime() - start > ticks(timeout_us)) {
            /* Abort: drop what is left; the controller ends the transaction
             * with STOP after the byte on the bus. */
            wr(bus, I2C_FIFO_CTRL, FIFO_CTRL_RXRST | FIFO_CTRL_FMTRST);
            const int clean = wait_idle(bus, FX1_I2C_ABORT_US);
            wr(bus, I2C_FIFO_CTRL, FIFO_CTRL_RXRST); /* bytes read before the STOP */
            ack_irq(bus);
            return clean ? FX1_I2C_ETIMEOUT : FX1_I2C_ERECOVER;
        }
    }
}

/* Budget left at the first FDATA write: no transaction starts once the
 * call's budget is spent; the idle wait and the interrupt acknowledge of
 * begin() count against it (M6a follow-up review R2). */
static int budget_left(uint64_t start, uint32_t timeout_us) { return fx1_mtime() - start <= ticks(timeout_us); }

/* The idle wait is part of the call's budget (start: the call's start). */
static int begin(fx1_i2c_t* bus, uint32_t timeout_us)
{
    const uint32_t us = timeout_us < FX1_I2C_ABORT_US ? timeout_us : FX1_I2C_ABORT_US;
    if (!wait_idle(bus, us)) return FX1_I2C_EBUSY; /* e.g. an abort still ending */
    ack_irq(bus);
    return FX1_I2C_OK;
}

int fx1_i2c_write(fx1_i2c_t* bus, uint8_t addr7, const uint8_t* data, uint32_t len, uint32_t timeout_us)
{
    if (!bus || addr7 > 0x7Fu || len == 0 || len > FX1_I2C_MAX_WRITE || !data) return FX1_I2C_EINVAL;
    const uint64_t start = fx1_mtime();
    const int rc = begin(bus, timeout_us);
    if (rc) return rc;
    if (!budget_left(start, timeout_us)) return FX1_I2C_ETIMEOUT; /* nothing sent */
    wr(bus, I2C_FDATA, FDATA_START | ((uint32_t)addr7 << 1));
    for (uint32_t i = 0; i < len; ++i) wr(bus, I2C_FDATA, data[i] | (i + 1 == len ? FDATA_STOP : 0u));
    return finish(bus, 0, 0, start, timeout_us);
}

int fx1_i2c_write_read(fx1_i2c_t* bus, uint8_t addr7, const uint8_t* wdata, uint32_t wlen, uint8_t* rdata,
                       uint32_t rlen, uint32_t timeout_us)
{
    if (!bus || addr7 > 0x7Fu || wlen == 0 || wlen > FX1_I2C_MAX_WRITE || !wdata || rlen == 0 ||
        rlen > FX1_I2C_MAX_READ || !rdata)
        return FX1_I2C_EINVAL;
    const uint64_t start = fx1_mtime();
    const int rc = begin(bus, timeout_us);
    if (rc) return rc;
    if (!budget_left(start, timeout_us)) return FX1_I2C_ETIMEOUT; /* nothing sent */
    wr(bus, I2C_FDATA, FDATA_START | ((uint32_t)addr7 << 1));
    for (uint32_t i = 0; i < wlen; ++i) wr(bus, I2C_FDATA, wdata[i]);
    wr(bus, I2C_FDATA, FDATA_START | ((uint32_t)addr7 << 1) | 1u);
    wr(bus, I2C_FDATA, FDATA_READB | FDATA_STOP | (rlen & 0xFFu)); /* 256 encodes as 0 */
    return finish(bus, rdata, rlen, start, timeout_us);
}
