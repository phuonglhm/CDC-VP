/*
 * FX1 SYS_DMA register map for C firmware (C99, no C++ required).
 *
 * Mirrors include/dma/registers.h (the model's C++ definitions). Every value
 * is checked against it at compile time by tests/test_regs_header.cpp, so the
 * two cannot drift. Offsets are relative to the DMA CSR base
 * (FX1_SYS_DMA_CSR_BASE in fx1/fx1_memory_map.h). All registers are 32-bit and
 * must be accessed with naturally aligned 32-bit loads/stores.
 */
#ifndef FX1_DMA_REGS_H
#define FX1_DMA_REGS_H

/* ---- geometry -------------------------------------------------------------- */
#define FX1_DMA_CHANNEL_COUNT    8u
#define FX1_DMA_CHANNEL_STRIDE   0x100u
#define FX1_DMA_FIFO_BYTES       32u
#define FX1_DMA_MAX_BURST_BEATS  16u
#define FX1_DMA_MAX_TRANSFER     1023u   /* 10-bit byte count */
#define FX1_DMA_REGISTER_BYTES   0x1100u
#define FX1_DMA_AXI_MASTER_ID    4u

/* Channel register n at `offset`. */
#define FX1_DMA_CH(n, offset) ((n) * FX1_DMA_CHANNEL_STRIDE + (offset))

/* ---- per-channel registers --------------------------------------------------- */
#define FX1_DMA_CH_CMD_READ_ADDR        0x00u
#define FX1_DMA_CH_CMD_WRITE_ADDR       0x04u
#define FX1_DMA_CH_CMD_TRANSFER_SIZE    0x08u
#define FX1_DMA_CH_CMD_CONTROL          0x0cu
#define FX1_DMA_CH_READ_CONFIG          0x10u
#define FX1_DMA_CH_WRITE_CONFIG         0x14u
#define FX1_DMA_CH_MODE_CONFIG          0x18u
#define FX1_DMA_CH_SCHEDULE_CONFIG      0x1cu
#define FX1_DMA_CH_PERIPHERAL_CONFIG    0x20u
#define FX1_DMA_CH_AXI_ATTR_REG         0x24u
#define FX1_DMA_CH_RESTRICTION_STATUS   0x2cu
#define FX1_DMA_CH_READ_OFFSET_STATUS   0x30u
#define FX1_DMA_CH_WRITE_OFFSET_STATUS  0x34u
#define FX1_DMA_CH_FIFO_STATUS          0x38u
#define FX1_DMA_CH_OUTSTANDING_STATUS   0x3cu
#define FX1_DMA_CH_ENABLE               0x40u
#define FX1_DMA_CH_START                0x44u
#define FX1_DMA_CH_ACTIVE_STATUS        0x48u
#define FX1_DMA_CH_TRANSFER_COUNT       0x50u
#define FX1_DMA_CH_INTERRUPT_RAW_STATUS 0xa0u
#define FX1_DMA_CH_INTERRUPT_CLEAR      0xa4u
#define FX1_DMA_CH_INTERRUPT_ENABLE     0xa8u
#define FX1_DMA_CH_INTERRUPT_STATUS     0xacu

/* ---- global registers ---------------------------------------------------------- */
#define FX1_DMA_CORE_STATUS              0x1000u /* bit n: channel n has an enabled interrupt */
#define FX1_DMA_CORE_JOINT_CONFIG        0x1030u
#define FX1_DMA_CORE_PRIORITY_CONFIG     0x1038u
#define FX1_DMA_CORE_CLOCK_DIVIDER       0x1040u
#define FX1_DMA_CORE_CHANNEL_START       0x1048u /* bit n starts channel n */
#define FX1_DMA_PERIPHERAL_RX_REQUEST    0x1050u
#define FX1_DMA_PERIPHERAL_TX_REQUEST    0x1054u
#define FX1_DMA_CORE_IDLE_STATUS         0x10d0u
#define FX1_DMA_DESIGN_CAPABILITY_STATUS 0x10e0u
#define FX1_DMA_CORE_CAPABILITY_STATUS0  0x10f0u
#define FX1_DMA_CORE_CAPABILITY_STATUS1  0x10f4u

/* ---- reset values and writable masks -------------------------------------------- */
#define FX1_DMA_CONFIG_RESET      0x84010000u
#define FX1_DMA_CONFIG_MASK       0xcf3f007fu
#define FX1_DMA_PERIPHERAL_MASK   0x071f071fu
#define FX1_DMA_ATTR_MASK         0x0000bfbfu
#define FX1_DMA_MODE_MASK         0x30000000u
#define FX1_DMA_TRANSFER_SIZE_MASK 0x3ffu
#define FX1_DMA_INTERRUPT_MASK    0x1fffu
#define FX1_DMA_CAPABILITY0       0x0a602258u

/* ---- CH_CMD_CONTROL ------------------------------------------------------------- */
#define FX1_DMA_CMD_SET_INT  0x1u /* raise COMMAND_COMPLETE when this command ends */
#define FX1_DMA_CMD_LAST     0x2u /* last command; otherwise bits [31:2] = next 16-byte
                                     descriptor {src, dst, size, control}, little endian */

/* ---- CH_READ_CONFIG / CH_WRITE_CONFIG -------------------------------------------- */
#define FX1_DMA_ADDRESS_INCREMENT  (1u << 31) /* 0: FIXED address (peripheral FIFO) */
#define FX1_DMA_OUTSTANDING_ENABLE (1u << 30)
#define FX1_DMA_CFG_PENDING(n)     (((n) & 0xfu) << 24) /* max outstanding, clamped 1..4 */
#define FX1_DMA_CFG_TOKENS(n)      (((n) & 0x3fu) << 16) /* arbitration tokens */
#define FX1_DMA_CFG_BURST_BYTES(n) ((n) & 0x7fu)          /* max bytes per burst, 1..127 */

/* ---- CH_MODE_CONFIG: byte swap [29:28] ------------------------------------------- */
#define FX1_DMA_MODE_SWAP_NONE 0x00000000u
#define FX1_DMA_MODE_SWAP_16   0x10000000u
#define FX1_DMA_MODE_SWAP_32   0x20000000u

/* ---- CH_PERIPHERAL_CONFIG: IDs [4:0]/[20:16], post-service delay [10:8]/[26:24] -- */
#define FX1_DMA_PERIPH_READ(id)        ((id) & 0x1fu)
#define FX1_DMA_PERIPH_READ_DELAY(c)   (((c) & 0x7u) << 8)
#define FX1_DMA_PERIPH_WRITE(id)       (((id) & 0x1fu) << 16)
#define FX1_DMA_PERIPH_WRITE_DELAY(c)  (((c) & 0x7u) << 24)

/* ---- interrupt bits (RAW / CLEAR / ENABLE / STATUS) --------------------------- */
#define FX1_DMA_INT_COMMAND_COMPLETE       (1u << 0)
#define FX1_DMA_INT_READ_SLVERR            (1u << 1)
#define FX1_DMA_INT_WRITE_SLVERR           (1u << 2)
#define FX1_DMA_INT_READ_DECERR            (1u << 3)
#define FX1_DMA_INT_WRITE_DECERR           (1u << 4)
#define FX1_DMA_INT_FIFO_OVERFLOW          (1u << 5)
#define FX1_DMA_INT_FIFO_UNDERFLOW         (1u << 6)
#define FX1_DMA_INT_READ_DATA_TIMEOUT      (1u << 7)
#define FX1_DMA_INT_READ_ADDRESS_TIMEOUT   (1u << 8)
#define FX1_DMA_INT_WRITE_RESPONSE_TIMEOUT (1u << 9)
#define FX1_DMA_INT_WRITE_DATA_TIMEOUT     (1u << 10)
#define FX1_DMA_INT_WRITE_ADDRESS_TIMEOUT  (1u << 11)
#define FX1_DMA_INT_WATCHDOG_TIMEOUT       (1u << 12)

#endif /* FX1_DMA_REGS_H */
