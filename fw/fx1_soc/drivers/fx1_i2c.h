/*
 * FX1 bare-metal I2C host driver (CSI-2 M5b) for the I2C0 controller model
 * (components/i2c_tlm in functional host mode, register map in its
 * include/i2c.h). One transaction per call. The call waits for its end by
 * polling, bounded by its budget in simulated time (mtime): it never sleeps
 * (M5b review F1). With use_irq the end is taken from the I2C interrupt at the
 * PLIC (source FX1_IRQ_I2C0 pending), which the driver claims and completes
 * itself, so no trap handler is involved; use_irq is for a context that no
 * trap handler services (the claim takes the highest pending source; another
 * source is completed again at once and pends again while its line is high).
 * An RTOS driver would rather wait on a semaphore given by an ISR.
 *
 * Budget: a completion seen at or before the deadline is OK, also on the poll
 * that crosses it. At the deadline the transaction is aborted (FIFO_CTRL FMT
 * and RX reset: the controller lets the byte on the bus complete, then sends
 * STOP; M5b review F2) and the call waits up to FX1_I2C_ABORT_US for the host
 * to be idle: FX1_I2C_ETIMEOUT when it is, FX1_I2C_ERECOVER when it is not
 * yet. A call first waits for the host to be idle (an abort still ending),
 * up to FX1_I2C_ABORT_US and never longer than its budget, then EBUSY; that
 * wait counts against the budget. A call therefore returns within its budget
 * + FX1_I2C_ABORT_US (M6a re-review: callers with a deadline rely on it), and
 * no transaction starts after the budget: checked right before the first
 * FDATA write, FX1_I2C_ETIMEOUT with nothing sent on the bus otherwise (M6a
 * follow-up review R2).
 *
 * The controller pauses between transactions when disabled and ignores FDATA
 * while disabled; this driver keeps the host enabled from fx1_i2c_open on.
 */
#ifndef FX1_I2C_H
#define FX1_I2C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FX1_I2C_OK        0
#define FX1_I2C_EINVAL   -1 /* bad argument (length, address) */
#define FX1_I2C_ENACK    -2 /* address or data byte not acknowledged */
#define FX1_I2C_ETIMEOUT -3 /* no completion within the budget */
#define FX1_I2C_EBUSY    -4 /* the host is not idle (a transaction still queued or ending) */
#define FX1_I2C_ERECOVER -5 /* budget exhausted, transaction aborted, the bus not idle yet */

/* Bound of the wait for the end of an aborted transaction: one byte and a
 * STOP (10 bit times) at 100 kHz, the slowest standard SCL. */
#define FX1_I2C_ABORT_US 100u

/* Bytes of one write transaction (2 index + data) and of one read: bounded by
 * the 64-entry command FIFO and the READB count. */
#define FX1_I2C_MAX_WRITE 60u
#define FX1_I2C_MAX_READ  256u

typedef struct fx1_i2c {
    uintptr_t base;
    uint32_t irq_source; /* PLIC source, used when use_irq */
    uint32_t plic_context;
    int use_irq;
    uint32_t irq_claims;  /* completions taken from the PLIC (use_irq) */
} fx1_i2c_t;

/* Enables the host, clears the FIFOs and interrupt state. With use_irq the
 * command-complete and NACK interrupts are enabled and the PLIC source is
 * enabled for plic_context with priority 1. */
int fx1_i2c_open(fx1_i2c_t* bus, uintptr_t base, int use_irq, uint32_t irq_source, uint32_t plic_context);

/* START, address (write), len bytes, STOP. */
int fx1_i2c_write(fx1_i2c_t* bus, uint8_t addr7, const uint8_t* data, uint32_t len, uint32_t timeout_us);

/* START, address (write), wlen bytes, repeated START, address (read), rlen
 * bytes (the last one NACKed), STOP. The RX FIFO is drained while the read
 * runs, so rlen may exceed its depth. */
int fx1_i2c_write_read(fx1_i2c_t* bus, uint8_t addr7, const uint8_t* wdata, uint32_t wlen, uint8_t* rdata,
                       uint32_t rlen, uint32_t timeout_us);

#ifdef __cplusplus
}
#endif

#endif /* FX1_I2C_H */
