/*
 * soc_regs_i2c.h - I2C (OpenTitan-style host+target) register map for VP_FX1.
 *
 * Register OFFSETS from an I2C base (CDC_I2C0_BASE / CDC_I2C1_BASE).
 * Verified against the TLM model (components/i2c_tlm) by
 * tools/check_regs_drift.cpp. All accesses are 32-bit little-endian.
 */
#ifndef CDC_SOC_REGS_I2C_H
#define CDC_SOC_REGS_I2C_H

/* ---- Register offsets ------------------------------------------------- */
#define CDC_I2C_INTR_STATE          0x00u  /* interrupt state (W1C)         */
#define CDC_I2C_INTR_ENABLE         0x04u  /* interrupt enable              */
#define CDC_I2C_CTRL                0x10u  /* control                       */
#define CDC_I2C_STATUS              0x14u  /* status (RO)                   */
#define CDC_I2C_RDATA               0x18u  /* host RX FIFO read             */
#define CDC_I2C_FDATA               0x1Cu  /* host FMT FIFO write           */
#define CDC_I2C_FIFO_CTRL           0x20u  /* FIFO control/reset            */
#define CDC_I2C_TARGET_FIFO_CONFIG  0x28u
#define CDC_I2C_HOST_FIFO_STATUS    0x2Cu
#define CDC_I2C_TARGET_FIFO_STATUS  0x30u
#define CDC_I2C_TARGET_ID           0x54u
#define CDC_I2C_ACQDATA             0x58u  /* target acquired data          */
#define CDC_I2C_TXDATA              0x5Cu  /* target TX FIFO write          */
#define CDC_I2C_TARGET_ACK_CTRL     0x6Cu
#define CDC_I2C_TARGET_EVENTS       0x7Cu

/* ---- CTRL bit-fields -------------------------------------------------- */
#define CDC_I2C_CTRL_ENABLEHOST     (1u << 0)
#define CDC_I2C_CTRL_ENABLETARGET   (1u << 1)

/* ---- FDATA (host format FIFO word) fields ----------------------------- */
#define CDC_I2C_FDATA_BYTE_MASK     0xFFu
#define CDC_I2C_FDATA_START         (1u << 8)
#define CDC_I2C_FDATA_STOP          (1u << 9)
#define CDC_I2C_FDATA_READB         (1u << 10)

/* ---- STATUS bit-fields ------------------------------------------------ */
#define CDC_I2C_STATUS_FMTFULL      (1u << 0)
#define CDC_I2C_STATUS_RXFULL       (1u << 1)
#define CDC_I2C_STATUS_FMTEMPTY     (1u << 2)
#define CDC_I2C_STATUS_HOSTIDLE     (1u << 3)
#define CDC_I2C_STATUS_TARGETIDLE   (1u << 4)
#define CDC_I2C_STATUS_RXEMPTY      (1u << 5)
#define CDC_I2C_STATUS_TXFULL       (1u << 6)
#define CDC_I2C_STATUS_ACQFULL      (1u << 7)
#define CDC_I2C_STATUS_TXEMPTY      (1u << 8)
#define CDC_I2C_STATUS_ACQEMPTY     (1u << 9)

/* ---- INTR_STATE / INTR_ENABLE bit-fields ------------------------------ */
#define CDC_I2C_INTR_TX_THRESHOLD   (1u << 0)
#define CDC_I2C_INTR_ACQ_THRESHOLD  (1u << 1)
#define CDC_I2C_INTR_ACQ_OVERFLOW   (1u << 3)
#define CDC_I2C_INTR_TX_STRETCH     (1u << 6)
#define CDC_I2C_INTR_CMD_COMPLETE   (1u << 9)

#endif /* CDC_SOC_REGS_I2C_H */
