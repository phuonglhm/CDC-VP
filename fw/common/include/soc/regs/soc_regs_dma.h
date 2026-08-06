/*
 * soc_regs_dma.h - DMA (PL330-style) register map for VP_FX1.
 *
 * Register OFFSETS from CDC_DMA0_BASE. Verified against the TLM model
 * (components/dma_tlm) by tools/check_regs_drift.cpp. 32-bit LE accesses.
 *
 * Per-channel registers use a fixed stride; use the CDC_DMA_*(n) helpers for
 * channel n. The DMA also owns a master socket into the system bus, so any
 * SAR/DAR you program is a physical RAM0 address (see soc_memory_map.h).
 */
#ifndef CDC_SOC_REGS_DMA_H
#define CDC_SOC_REGS_DMA_H

/* ---- Manager / global registers --------------------------------------- */
#define CDC_DMA_DSR           0x000u  /* DMA manager status                 */
#define CDC_DMA_DPC           0x004u  /* DMA program counter                */
#define CDC_DMA_INTEN         0x020u  /* interrupt enable                   */
#define CDC_DMA_INT_EVENT_RIS 0x024u  /* raw interrupt status               */
#define CDC_DMA_INTMIS        0x028u  /* masked interrupt status            */
#define CDC_DMA_INTCLR        0x02Cu  /* interrupt clear (W1C)              */
#define CDC_DMA_FSRD          0x030u  /* fault status - manager             */
#define CDC_DMA_FSRC          0x034u  /* fault status - channels            */
#define CDC_DMA_FTRD          0x038u  /* fault type - manager               */

/* ---- Per-channel status block (stride 0x008 from channel 0) ----------- */
#define CDC_DMA_CSR0          0x100u  /* channel status                     */
#define CDC_DMA_CPC0          0x104u  /* channel PC                         */
#define CDC_DMA_CH_STAT_STRIDE 0x008u
#define CDC_DMA_CSR(n)  (CDC_DMA_CSR0 + (n) * CDC_DMA_CH_STAT_STRIDE)
#define CDC_DMA_CPC(n)  (CDC_DMA_CPC0 + (n) * CDC_DMA_CH_STAT_STRIDE)

/* ---- Per-channel address/control block (stride 0x020 from channel 0) -- */
#define CDC_DMA_SAR0          0x400u  /* source address                     */
#define CDC_DMA_DAR0          0x404u  /* destination address                */
#define CDC_DMA_CCR0          0x408u  /* channel control                    */
#define CDC_DMA_LC0_0         0x40Cu  /* loop counter 0                     */
#define CDC_DMA_LC1_0         0x410u  /* loop counter 1                     */
#define CDC_DMA_CH_AXI_STRIDE 0x020u
#define CDC_DMA_SAR(n)  (CDC_DMA_SAR0 + (n) * CDC_DMA_CH_AXI_STRIDE)
#define CDC_DMA_DAR(n)  (CDC_DMA_DAR0 + (n) * CDC_DMA_CH_AXI_STRIDE)
#define CDC_DMA_CCR(n)  (CDC_DMA_CCR0 + (n) * CDC_DMA_CH_AXI_STRIDE)

/* ---- CCR transfer-shape fields --------------------------------------- */
#define CDC_DMA_CCR_SRC_INC              (1u << 0)
#define CDC_DMA_CCR_SRC_BURST_SIZE_SHIFT 1u
#define CDC_DMA_CCR_SRC_BURST_LEN_SHIFT  4u
#define CDC_DMA_CCR_DST_INC              (1u << 14)
#define CDC_DMA_CCR_DST_BURST_SIZE_SHIFT 15u
#define CDC_DMA_CCR_DST_BURST_LEN_SHIFT  18u

/* ---- Debug interface -------------------------------------------------- */
#define CDC_DMA_DBGSTATUS     0xD00u
#define CDC_DMA_DBGCMD        0xD04u
#define CDC_DMA_DBGINST0      0xD08u
#define CDC_DMA_DBGINST1      0xD0Cu

/* ---- Reset value ------------------------------------------------------ */
#define CDC_DMA_CCR_RESET_VALUE 0x00800200u
#define CDC_DMA_STATUS_STOPPED   0x0u

#endif /* CDC_SOC_REGS_DMA_H */
