// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: virtual_platform.sw.eth_regs

// Changelog:
//   91 registers.
// =============================================================================
#ifndef ETH_REGS_H
#define ETH_REGS_H

/* Byte offsets from the IP base, field shifts and masks, reset values. */

#define ETH_ID                             0x0000u
#define ETH_ID_RESET                       0x45544801u
#define ETH_ID_VER_MINOR_SHIFT             0u
#define ETH_ID_VER_MINOR_MASK              0x0000000Fu
#define ETH_ID_VER_MAJOR_SHIFT             4u
#define ETH_ID_VER_MAJOR_MASK              0x000000F0u
#define ETH_ID_ID_SHIFT                    8u
#define ETH_ID_ID_MASK                     0xFFFFFF00u

#define ETH_CTRL                           0x0004u
#define ETH_CTRL_RESET                     0x00000000u
#define ETH_CTRL_RST_N_SHIFT               0u
#define ETH_CTRL_RST_N_MASK                0x00000001u
#define ETH_CTRL_RXCSUM_RST_N_SHIFT        1u
#define ETH_CTRL_RXCSUM_RST_N_MASK         0x00000002u
#define ETH_CTRL_TXCSUM_RST_N_SHIFT        2u
#define ETH_CTRL_TXCSUM_RST_N_MASK         0x00000004u
#define ETH_CTRL_MDIO_RST_N_SHIFT          3u
#define ETH_CTRL_MDIO_RST_N_MASK           0x00000008u
#define ETH_CTRL_DMA_RST_N_SHIFT           4u
#define ETH_CTRL_DMA_RST_N_MASK            0x00000010u
#define ETH_CTRL_TXFIFO_WR_RST_N_SHIFT     5u
#define ETH_CTRL_TXFIFO_WR_RST_N_MASK      0x00000020u
#define ETH_CTRL_RXFIFO_RD_RST_N_SHIFT     6u
#define ETH_CTRL_RXFIFO_RD_RST_N_MASK      0x00000040u
#define ETH_CTRL_RX_RMII_RST_N_SHIFT       7u
#define ETH_CTRL_RX_RMII_RST_N_MASK        0x00000080u
#define ETH_CTRL_TX_RMII_RST_N_SHIFT       8u
#define ETH_CTRL_TX_RMII_RST_N_MASK        0x00000100u
#define ETH_CTRL_RXMAC_RST_N_SHIFT         9u
#define ETH_CTRL_RXMAC_RST_N_MASK          0x00000200u
#define ETH_CTRL_TXMAC_RST_N_SHIFT         10u
#define ETH_CTRL_TXMAC_RST_N_MASK          0x00000400u
#define ETH_CTRL_RMON_RST_N_SHIFT          11u
#define ETH_CTRL_RMON_RST_N_MASK           0x00000800u
#define ETH_CTRL_TXFIFO_RD_RST_N_SHIFT     12u
#define ETH_CTRL_TXFIFO_RD_RST_N_MASK      0x00001000u
#define ETH_CTRL_RXFIFO_WR_RST_N_SHIFT     13u
#define ETH_CTRL_RXFIFO_WR_RST_N_MASK      0x00002000u

#define ETH_STATUS                         0x0008u
#define ETH_STATUS_RESET                   0x00000000u
#define ETH_STATUS_TX_BUSY_SHIFT           0u
#define ETH_STATUS_TX_BUSY_MASK            0x00000001u
#define ETH_STATUS_RX_BUSY_SHIFT           1u
#define ETH_STATUS_RX_BUSY_MASK            0x00000002u
#define ETH_STATUS_DMA_TX_BUSY_SHIFT       2u
#define ETH_STATUS_DMA_TX_BUSY_MASK        0x00000004u
#define ETH_STATUS_DMA_RX_BUSY_SHIFT       3u
#define ETH_STATUS_DMA_RX_BUSY_MASK        0x00000008u
#define ETH_STATUS_AXI_ERR_STICKY_SHIFT    4u
#define ETH_STATUS_AXI_ERR_STICKY_MASK     0x00000010u

#define ETH_CAP                            0x000Cu
#define ETH_CAP_RESET                      0x00000001u
#define ETH_CAP_FULL_DUPLEX_SHIFT          0u
#define ETH_CAP_FULL_DUPLEX_MASK           0x00000001u
#define ETH_CAP_JUMBO_EN_SHIFT             1u
#define ETH_CAP_JUMBO_EN_MASK              0x00000002u
#define ETH_CAP_AXI_DATA_WIDTH_SHIFT       8u
#define ETH_CAP_AXI_DATA_WIDTH_MASK        0x00000300u

#define ETH_INT_RAW                        0x0010u
#define ETH_INT_RAW_RESET                  0x00000000u
#define ETH_INT_RAW_TX_DONE_SHIFT          0u
#define ETH_INT_RAW_TX_DONE_MASK           0x00000001u
#define ETH_INT_RAW_RX_DONE_SHIFT          1u
#define ETH_INT_RAW_RX_DONE_MASK           0x00000002u
#define ETH_INT_RAW_TX_ERR_SHIFT           2u
#define ETH_INT_RAW_TX_ERR_MASK            0x00000004u
#define ETH_INT_RAW_RX_ERR_SHIFT           3u
#define ETH_INT_RAW_RX_ERR_MASK            0x00000008u
#define ETH_INT_RAW_TX_BUF_UNAVAIL_SHIFT   4u
#define ETH_INT_RAW_TX_BUF_UNAVAIL_MASK    0x00000010u
#define ETH_INT_RAW_RX_BUF_UNAVAIL_SHIFT   5u
#define ETH_INT_RAW_RX_BUF_UNAVAIL_MASK    0x00000020u
#define ETH_INT_RAW_MDIO_DONE_SHIFT        7u
#define ETH_INT_RAW_MDIO_DONE_MASK         0x00000080u

#define ETH_INT_EN                         0x0014u
#define ETH_INT_EN_RESET                   0x00000000u
#define ETH_INT_EN_EN_SHIFT                0u
#define ETH_INT_EN_EN_MASK                 0x0000003Fu
#define ETH_INT_EN_EN_MDIO_DONE_SHIFT      7u
#define ETH_INT_EN_EN_MDIO_DONE_MASK       0x00000080u

#define ETH_INT_INJ                        0x0018u
#define ETH_INT_INJ_RESET                  0x00000000u
#define ETH_INT_INJ_INJ_SHIFT              0u
#define ETH_INT_INJ_INJ_MASK               0x0000003Fu
#define ETH_INT_INJ_INJ_MDIO_DONE_SHIFT    7u
#define ETH_INT_INJ_INJ_MDIO_DONE_MASK     0x00000080u

#define ETH_INT_STATUS                     0x001Cu
#define ETH_INT_STATUS_RESET               0x00000000u
#define ETH_INT_STATUS_STATUS_SHIFT        0u
#define ETH_INT_STATUS_STATUS_MASK         0x0000003Fu
#define ETH_INT_STATUS_STATUS_MDIO_DONE_SHIFT 7u
#define ETH_INT_STATUS_STATUS_MDIO_DONE_MASK 0x00000080u

#define ETH_FIFO_STAT                      0x0020u
#define ETH_FIFO_STAT_RESET                0x00000000u
#define ETH_FIFO_STAT_RXFIFO_OVF_SHIFT     0u
#define ETH_FIFO_STAT_RXFIFO_OVF_MASK      0x00000001u
#define ETH_FIFO_STAT_RXFIFO_UDF_SHIFT     1u
#define ETH_FIFO_STAT_RXFIFO_UDF_MASK      0x00000002u
#define ETH_FIFO_STAT_TXFIFO_OVF_SHIFT     2u
#define ETH_FIFO_STAT_TXFIFO_OVF_MASK      0x00000004u
#define ETH_FIFO_STAT_TXFIFO_UDF_SHIFT     3u
#define ETH_FIFO_STAT_TXFIFO_UDF_MASK      0x00000008u
#define ETH_FIFO_STAT_TXMETA_OVF_SHIFT     4u
#define ETH_FIFO_STAT_TXMETA_OVF_MASK      0x00000010u
#define ETH_FIFO_STAT_TXMETA_UDF_SHIFT     5u
#define ETH_FIFO_STAT_TXMETA_UDF_MASK      0x00000020u

#define ETH_RMII_CFG                       0x1000u
#define ETH_RMII_CFG_RESET                 0x00000000u
#define ETH_RMII_CFG_SPEED_SHIFT           0u
#define ETH_RMII_CFG_SPEED_MASK            0x00000001u
#define ETH_RMII_CFG_TXD_BIT_ORDER_SHIFT   1u
#define ETH_RMII_CFG_TXD_BIT_ORDER_MASK    0x00000002u
#define ETH_RMII_CFG_RXD_BIT_ORDER_SHIFT   2u
#define ETH_RMII_CFG_RXD_BIT_ORDER_MASK    0x00000004u

#define ETH_RX_MAC_CFG                     0x2000u
#define ETH_RX_MAC_CFG_RESET               0x00000000u
#define ETH_RX_MAC_CFG_RX_EN_SHIFT         0u
#define ETH_RX_MAC_CFG_RX_EN_MASK          0x00000001u

#define ETH_RX_MAX_FRAME                   0x2004u
#define ETH_RX_MAX_FRAME_RESET             0x000005EEu
#define ETH_RX_MAX_FRAME_MAX_FRAME_SHIFT   0u
#define ETH_RX_MAX_FRAME_MAX_FRAME_MASK    0x0000FFFFu

#define ETH_RX_MIN_FRAME                   0x2008u
#define ETH_RX_MIN_FRAME_RESET             0x00000040u
#define ETH_RX_MIN_FRAME_MIN_FRAME_SHIFT   0u
#define ETH_RX_MIN_FRAME_MIN_FRAME_MASK    0x0000FFFFu

#define ETH_FILT_CTRL                      0x2010u
#define ETH_FILT_CTRL_RESET                0x00000001u
#define ETH_FILT_CTRL_BCAST_ACCEPT_SHIFT   0u
#define ETH_FILT_CTRL_BCAST_ACCEPT_MASK    0x00000001u
#define ETH_FILT_CTRL_PASS_ALL_MCAST_SHIFT 1u
#define ETH_FILT_CTRL_PASS_ALL_MCAST_MASK  0x00000002u
#define ETH_FILT_CTRL_HASH_EN_SHIFT        2u
#define ETH_FILT_CTRL_HASH_EN_MASK         0x00000004u

#define ETH_HASH_L                         0x2014u
#define ETH_HASH_L_RESET                   0x00000000u
#define ETH_HASH_L_HASH_L_SHIFT            0u
#define ETH_HASH_L_HASH_L_MASK             0xFFFFFFFFu

#define ETH_HASH_H                         0x2018u
#define ETH_HASH_H_RESET                   0x00000000u
#define ETH_HASH_H_HASH_H_SHIFT            0u
#define ETH_HASH_H_HASH_H_MASK             0xFFFFFFFFu

#define ETH_UC_ADDR_L0                     0x2020u
#define ETH_UC_ADDR_L0_RESET               0x00000000u
#define ETH_UC_ADDR_L0_ADDR_L_SHIFT        0u
#define ETH_UC_ADDR_L0_ADDR_L_MASK         0xFFFFFFFFu

#define ETH_UC_ADDR_H0                     0x2024u
#define ETH_UC_ADDR_H0_RESET               0x00000000u
#define ETH_UC_ADDR_H0_ADDR_H_SHIFT        0u
#define ETH_UC_ADDR_H0_ADDR_H_MASK         0x0000FFFFu

#define ETH_UC_CTRL0                       0x2028u
#define ETH_UC_CTRL0_RESET                 0x00000000u
#define ETH_UC_CTRL0_ENABLE_SHIFT          0u
#define ETH_UC_CTRL0_ENABLE_MASK           0x00000001u

#define ETH_UC_ADDR_L1                     0x2030u
#define ETH_UC_ADDR_L1_RESET               0x00000000u
#define ETH_UC_ADDR_L1_ADDR_L_SHIFT        0u
#define ETH_UC_ADDR_L1_ADDR_L_MASK         0xFFFFFFFFu

#define ETH_UC_ADDR_H1                     0x2034u
#define ETH_UC_ADDR_H1_RESET               0x00000000u
#define ETH_UC_ADDR_H1_ADDR_H_SHIFT        0u
#define ETH_UC_ADDR_H1_ADDR_H_MASK         0x0000FFFFu

#define ETH_UC_CTRL1                       0x2038u
#define ETH_UC_CTRL1_RESET                 0x00000000u
#define ETH_UC_CTRL1_ENABLE_SHIFT          0u
#define ETH_UC_CTRL1_ENABLE_MASK           0x00000001u

#define ETH_UC_ADDR_L2                     0x2040u
#define ETH_UC_ADDR_L2_RESET               0x00000000u
#define ETH_UC_ADDR_L2_ADDR_L_SHIFT        0u
#define ETH_UC_ADDR_L2_ADDR_L_MASK         0xFFFFFFFFu

#define ETH_UC_ADDR_H2                     0x2044u
#define ETH_UC_ADDR_H2_RESET               0x00000000u
#define ETH_UC_ADDR_H2_ADDR_H_SHIFT        0u
#define ETH_UC_ADDR_H2_ADDR_H_MASK         0x0000FFFFu

#define ETH_UC_CTRL2                       0x2048u
#define ETH_UC_CTRL2_RESET                 0x00000000u
#define ETH_UC_CTRL2_ENABLE_SHIFT          0u
#define ETH_UC_CTRL2_ENABLE_MASK           0x00000001u

#define ETH_UC_ADDR_L3                     0x2050u
#define ETH_UC_ADDR_L3_RESET               0x00000000u
#define ETH_UC_ADDR_L3_ADDR_L_SHIFT        0u
#define ETH_UC_ADDR_L3_ADDR_L_MASK         0xFFFFFFFFu

#define ETH_UC_ADDR_H3                     0x2054u
#define ETH_UC_ADDR_H3_RESET               0x00000000u
#define ETH_UC_ADDR_H3_ADDR_H_SHIFT        0u
#define ETH_UC_ADDR_H3_ADDR_H_MASK         0x0000FFFFu

#define ETH_UC_CTRL3                       0x2058u
#define ETH_UC_CTRL3_RESET                 0x00000000u
#define ETH_UC_CTRL3_ENABLE_SHIFT          0u
#define ETH_UC_CTRL3_ENABLE_MASK           0x00000001u

#define ETH_TX_MAC_CFG                     0x3000u
#define ETH_TX_MAC_CFG_RESET               0x00000000u
#define ETH_TX_MAC_CFG_TX_EN_SHIFT         0u
#define ETH_TX_MAC_CFG_TX_EN_MASK          0x00000001u

#define ETH_IPG                            0x3004u
#define ETH_IPG_RESET                      0x0000000Cu
#define ETH_IPG_IPG_SHIFT                  0u
#define ETH_IPG_IPG_MASK                   0x000000FFu

#define ETH_TX_MAX_FRAME                   0x3008u
#define ETH_TX_MAX_FRAME_RESET             0x000005EEu
#define ETH_TX_MAX_FRAME_MAX_FRAME_SHIFT   0u
#define ETH_TX_MAX_FRAME_MAX_FRAME_MASK    0x0000FFFFu

#define ETH_TX_MIN_FRAME                   0x300Cu
#define ETH_TX_MIN_FRAME_RESET             0x00000040u
#define ETH_TX_MIN_FRAME_MIN_FRAME_SHIFT   0u
#define ETH_TX_MIN_FRAME_MIN_FRAME_MASK    0x0000FFFFu

#define ETH_CSUM_RX_CFG                    0x4000u
#define ETH_CSUM_RX_CFG_RESET              0x00000003u
#define ETH_CSUM_RX_CFG_CSUM_RX_EN_SHIFT   0u
#define ETH_CSUM_RX_CFG_CSUM_RX_EN_MASK    0x00000001u
#define ETH_CSUM_RX_CFG_CSUM_RX_L4_EN_SHIFT 1u
#define ETH_CSUM_RX_CFG_CSUM_RX_L4_EN_MASK 0x00000002u

#define ETH_CSUM_TX_CFG                    0x5000u
#define ETH_CSUM_TX_CFG_RESET              0x00000003u
#define ETH_CSUM_TX_CFG_CSUM_TX_EN_SHIFT   0u
#define ETH_CSUM_TX_CFG_CSUM_TX_EN_MASK    0x00000001u
#define ETH_CSUM_TX_CFG_CSUM_TX_L4_EN_SHIFT 1u
#define ETH_CSUM_TX_CFG_CSUM_TX_L4_EN_MASK 0x00000002u

#define ETH_DMA_BUILDCFG                   0x6000u
#define ETH_DMA_BUILDCFG_RESET             0x29AC2405u
#define ETH_DMA_BUILDCFG_DATA_WIDTH_LOG2_SHIFT 0u
#define ETH_DMA_BUILDCFG_DATA_WIDTH_LOG2_MASK 0x0000001Fu
#define ETH_DMA_BUILDCFG_ADDR_WIDTH_SHIFT  5u
#define ETH_DMA_BUILDCFG_ADDR_WIDTH_MASK   0x000007E0u
#define ETH_DMA_BUILDCFG_MAX_BURST_LOG2_SHIFT 11u
#define ETH_DMA_BUILDCFG_MAX_BURST_LOG2_MASK 0x0000F800u
#define ETH_DMA_BUILDCFG_TX_FIFO_DEPTH_LOG2_SHIFT 16u
#define ETH_DMA_BUILDCFG_TX_FIFO_DEPTH_LOG2_MASK 0x001F0000u
#define ETH_DMA_BUILDCFG_RX_FIFO_DEPTH_LOG2_SHIFT 21u
#define ETH_DMA_BUILDCFG_RX_FIFO_DEPTH_LOG2_MASK 0x03E00000u
#define ETH_DMA_BUILDCFG_POLL_CYCLES_LOG2_SHIFT 26u
#define ETH_DMA_BUILDCFG_POLL_CYCLES_LOG2_MASK 0xFC000000u

#define ETH_DMA_AXI_CFG                    0x6004u
#define ETH_DMA_AXI_CFG_RESET              0x00000000u
#define ETH_DMA_AXI_CFG_AWCACHE_SHIFT      0u
#define ETH_DMA_AXI_CFG_AWCACHE_MASK       0x0000000Fu
#define ETH_DMA_AXI_CFG_ARCACHE_SHIFT      4u
#define ETH_DMA_AXI_CFG_ARCACHE_MASK       0x000000F0u

#define ETH_DMA_TX_CTRL                    0x6010u
#define ETH_DMA_TX_CTRL_RESET              0x00000000u
#define ETH_DMA_TX_CTRL_START_SHIFT        0u
#define ETH_DMA_TX_CTRL_START_MASK         0x00000001u
#define ETH_DMA_TX_CTRL_HALT_REQ_SHIFT     1u
#define ETH_DMA_TX_CTRL_HALT_REQ_MASK      0x00000002u
#define ETH_DMA_TX_CTRL_SRST_SHIFT         2u
#define ETH_DMA_TX_CTRL_SRST_MASK          0x00000004u

#define ETH_DMA_TX_STAT                    0x6014u
#define ETH_DMA_TX_STAT_RESET              0x00000004u
#define ETH_DMA_TX_STAT_RUNNING_SHIFT      0u
#define ETH_DMA_TX_STAT_RUNNING_MASK       0x00000001u
#define ETH_DMA_TX_STAT_HALTED_SHIFT       1u
#define ETH_DMA_TX_STAT_HALTED_MASK        0x00000002u
#define ETH_DMA_TX_STAT_IDLE_SHIFT         2u
#define ETH_DMA_TX_STAT_IDLE_MASK          0x00000004u
#define ETH_DMA_TX_STAT_SUSPENDED_SHIFT    3u
#define ETH_DMA_TX_STAT_SUSPENDED_MASK     0x00000008u
#define ETH_DMA_TX_STAT_ERR_SHIFT          4u
#define ETH_DMA_TX_STAT_ERR_MASK           0x00000010u
#define ETH_DMA_TX_STAT_DONE_SHIFT         5u
#define ETH_DMA_TX_STAT_DONE_MASK          0x00000020u

#define ETH_DMA_TX_DESC_BASE               0x6018u
#define ETH_DMA_TX_DESC_BASE_RESET         0x00000000u
#define ETH_DMA_TX_DESC_BASE_DESC_BASE_SHIFT 4u
#define ETH_DMA_TX_DESC_BASE_DESC_BASE_MASK 0xFFFFFFF0u

#define ETH_DMA_TX_DESC_LEN                0x601Cu
#define ETH_DMA_TX_DESC_LEN_RESET          0x00000000u
#define ETH_DMA_TX_DESC_LEN_DESC_LEN_SHIFT 0u
#define ETH_DMA_TX_DESC_LEN_DESC_LEN_MASK  0x0000FFFFu

#define ETH_DMA_TX_POLL                    0x6020u
#define ETH_DMA_TX_POLL_RESET              0x00000000u
#define ETH_DMA_TX_POLL_POLL_SHIFT         0u
#define ETH_DMA_TX_POLL_POLL_MASK          0x00000001u

#define ETH_DMA_TX_COAL                    0x6024u
#define ETH_DMA_TX_COAL_RESET              0x00000000u
#define ETH_DMA_TX_COAL_COAL_CNT_SHIFT     0u
#define ETH_DMA_TX_COAL_COAL_CNT_MASK      0x000000FFu
#define ETH_DMA_TX_COAL_COAL_TIMER_SHIFT   8u
#define ETH_DMA_TX_COAL_COAL_TIMER_MASK    0x00FFFF00u

#define ETH_DMA_TX_ERRINFO                 0x6028u
#define ETH_DMA_TX_ERRINFO_RESET           0x00000000u
#define ETH_DMA_TX_ERRINFO_ERR_CLASS_SHIFT 0u
#define ETH_DMA_TX_ERRINFO_ERR_CLASS_MASK  0x0000000Fu
#define ETH_DMA_TX_ERRINFO_DECERR_SHIFT    4u
#define ETH_DMA_TX_ERRINFO_DECERR_MASK     0x00000010u

#define ETH_DMA_TX_CUR_DESC                0x602Cu
#define ETH_DMA_TX_CUR_DESC_RESET          0x00000000u
#define ETH_DMA_TX_CUR_DESC_CUR_DESC_SHIFT 0u
#define ETH_DMA_TX_CUR_DESC_CUR_DESC_MASK  0x0000FFFFu

#define ETH_DMA_RX_CTRL                    0x6800u
#define ETH_DMA_RX_CTRL_RESET              0x00000000u
#define ETH_DMA_RX_CTRL_START_SHIFT        0u
#define ETH_DMA_RX_CTRL_START_MASK         0x00000001u
#define ETH_DMA_RX_CTRL_HALT_REQ_SHIFT     1u
#define ETH_DMA_RX_CTRL_HALT_REQ_MASK      0x00000002u
#define ETH_DMA_RX_CTRL_SRST_SHIFT         2u
#define ETH_DMA_RX_CTRL_SRST_MASK          0x00000004u

#define ETH_DMA_RX_STAT                    0x6804u
#define ETH_DMA_RX_STAT_RESET              0x00000004u
#define ETH_DMA_RX_STAT_RUNNING_SHIFT      0u
#define ETH_DMA_RX_STAT_RUNNING_MASK       0x00000001u
#define ETH_DMA_RX_STAT_HALTED_SHIFT       1u
#define ETH_DMA_RX_STAT_HALTED_MASK        0x00000002u
#define ETH_DMA_RX_STAT_IDLE_SHIFT         2u
#define ETH_DMA_RX_STAT_IDLE_MASK          0x00000004u
#define ETH_DMA_RX_STAT_SUSPENDED_SHIFT    3u
#define ETH_DMA_RX_STAT_SUSPENDED_MASK     0x00000008u
#define ETH_DMA_RX_STAT_ERR_SHIFT          4u
#define ETH_DMA_RX_STAT_ERR_MASK           0x00000010u
#define ETH_DMA_RX_STAT_DONE_SHIFT         5u
#define ETH_DMA_RX_STAT_DONE_MASK          0x00000020u

#define ETH_DMA_RX_DESC_BASE               0x6808u
#define ETH_DMA_RX_DESC_BASE_RESET         0x00000000u
#define ETH_DMA_RX_DESC_BASE_DESC_BASE_SHIFT 4u
#define ETH_DMA_RX_DESC_BASE_DESC_BASE_MASK 0xFFFFFFF0u

#define ETH_DMA_RX_DESC_LEN                0x680Cu
#define ETH_DMA_RX_DESC_LEN_RESET          0x00000000u
#define ETH_DMA_RX_DESC_LEN_DESC_LEN_SHIFT 0u
#define ETH_DMA_RX_DESC_LEN_DESC_LEN_MASK  0x0000FFFFu

#define ETH_DMA_RX_POLL                    0x6810u
#define ETH_DMA_RX_POLL_RESET              0x00000000u
#define ETH_DMA_RX_POLL_POLL_SHIFT         0u
#define ETH_DMA_RX_POLL_POLL_MASK          0x00000001u

#define ETH_DMA_RX_COAL                    0x6814u
#define ETH_DMA_RX_COAL_RESET              0x00000000u
#define ETH_DMA_RX_COAL_COAL_CNT_SHIFT     0u
#define ETH_DMA_RX_COAL_COAL_CNT_MASK      0x000000FFu
#define ETH_DMA_RX_COAL_COAL_TIMER_SHIFT   8u
#define ETH_DMA_RX_COAL_COAL_TIMER_MASK    0x00FFFF00u

#define ETH_DMA_RX_ERRINFO                 0x6818u
#define ETH_DMA_RX_ERRINFO_RESET           0x00000000u
#define ETH_DMA_RX_ERRINFO_ERR_CLASS_SHIFT 0u
#define ETH_DMA_RX_ERRINFO_ERR_CLASS_MASK  0x0000000Fu
#define ETH_DMA_RX_ERRINFO_DECERR_SHIFT    4u
#define ETH_DMA_RX_ERRINFO_DECERR_MASK     0x00000010u

#define ETH_DMA_RX_CUR_DESC                0x681Cu
#define ETH_DMA_RX_CUR_DESC_RESET          0x00000000u
#define ETH_DMA_RX_CUR_DESC_CUR_DESC_SHIFT 0u
#define ETH_DMA_RX_CUR_DESC_CUR_DESC_MASK  0x0000FFFFu

#define ETH_DMA_RX_BUF_SIZE                0x6820u
#define ETH_DMA_RX_BUF_SIZE_RESET          0x000005EAu
#define ETH_DMA_RX_BUF_SIZE_BUF_SIZE_SHIFT 0u
#define ETH_DMA_RX_BUF_SIZE_BUF_SIZE_MASK  0x0000FFFFu

#define ETH_DMA_RX_DROP_CNT                0x6824u
#define ETH_DMA_RX_DROP_CNT_RESET          0x00000000u
#define ETH_DMA_RX_DROP_CNT_COUNT_SHIFT    0u
#define ETH_DMA_RX_DROP_CNT_COUNT_MASK     0xFFFFFFFFu

#define RMON_CTRL                          0x7000u
#define RMON_CTRL_RESET                    0x00000000u
#define RMON_CTRL_RMON_EN_SHIFT            0u
#define RMON_CTRL_RMON_EN_MASK             0x00000001u
#define RMON_CTRL_READ_CLEAR_EN_SHIFT      1u
#define RMON_CTRL_READ_CLEAR_EN_MASK       0x00000002u
#define RMON_CTRL_CLEAR_ALL_SHIFT          2u
#define RMON_CTRL_CLEAR_ALL_MASK           0x00000004u
#define RMON_CTRL_FREEZE_SHIFT             3u
#define RMON_CTRL_FREEZE_MASK              0x00000008u

#define RMON_OVF_STAT_TX                   0x7004u
#define RMON_OVF_STAT_TX_RESET             0x00000000u
#define RMON_OVF_STAT_TX_OVF_TX_SHIFT      0u
#define RMON_OVF_STAT_TX_OVF_TX_MASK       0x00001FFFu

#define RMON_OVF_STAT_RX                   0x7008u
#define RMON_OVF_STAT_RX_RESET             0x00000000u
#define RMON_OVF_STAT_RX_OVF_RX_SHIFT      0u
#define RMON_OVF_STAT_RX_OVF_RX_MASK       0x0003FFFFu

#define RMON_TX_PKTS                       0x7010u
#define RMON_TX_PKTS_RESET                 0x00000000u
#define RMON_TX_PKTS_COUNT_SHIFT           0u
#define RMON_TX_PKTS_COUNT_MASK            0xFFFFFFFFu

#define RMON_TX_OCTETS                     0x7014u
#define RMON_TX_OCTETS_RESET               0x00000000u
#define RMON_TX_OCTETS_COUNT_SHIFT         0u
#define RMON_TX_OCTETS_COUNT_MASK          0xFFFFFFFFu

#define RMON_TX_BCAST                      0x7018u
#define RMON_TX_BCAST_RESET                0x00000000u
#define RMON_TX_BCAST_COUNT_SHIFT          0u
#define RMON_TX_BCAST_COUNT_MASK           0xFFFFFFFFu

#define RMON_TX_MCAST                      0x701Cu
#define RMON_TX_MCAST_RESET                0x00000000u
#define RMON_TX_MCAST_COUNT_SHIFT          0u
#define RMON_TX_MCAST_COUNT_MASK           0xFFFFFFFFu

#define RMON_TX_UNDERSIZE                  0x7020u
#define RMON_TX_UNDERSIZE_RESET            0x00000000u
#define RMON_TX_UNDERSIZE_COUNT_SHIFT      0u
#define RMON_TX_UNDERSIZE_COUNT_MASK       0xFFFFFFFFu

#define RMON_TX_OVERSIZE                   0x7024u
#define RMON_TX_OVERSIZE_RESET             0x00000000u
#define RMON_TX_OVERSIZE_COUNT_SHIFT       0u
#define RMON_TX_OVERSIZE_COUNT_MASK        0xFFFFFFFFu

#define RMON_TX_DROP                       0x7028u
#define RMON_TX_DROP_RESET                 0x00000000u
#define RMON_TX_DROP_COUNT_SHIFT           0u
#define RMON_TX_DROP_COUNT_MASK            0xFFFFFFFFu

#define RMON_TX_PKTS_64                    0x702Cu
#define RMON_TX_PKTS_64_RESET              0x00000000u
#define RMON_TX_PKTS_64_COUNT_SHIFT        0u
#define RMON_TX_PKTS_64_COUNT_MASK         0xFFFFFFFFu

#define RMON_TX_PKTS_65_127                0x7030u
#define RMON_TX_PKTS_65_127_RESET          0x00000000u
#define RMON_TX_PKTS_65_127_COUNT_SHIFT    0u
#define RMON_TX_PKTS_65_127_COUNT_MASK     0xFFFFFFFFu

#define RMON_TX_PKTS_128_255               0x7034u
#define RMON_TX_PKTS_128_255_RESET         0x00000000u
#define RMON_TX_PKTS_128_255_COUNT_SHIFT   0u
#define RMON_TX_PKTS_128_255_COUNT_MASK    0xFFFFFFFFu

#define RMON_TX_PKTS_256_511               0x7038u
#define RMON_TX_PKTS_256_511_RESET         0x00000000u
#define RMON_TX_PKTS_256_511_COUNT_SHIFT   0u
#define RMON_TX_PKTS_256_511_COUNT_MASK    0xFFFFFFFFu

#define RMON_TX_PKTS_512_1023              0x703Cu
#define RMON_TX_PKTS_512_1023_RESET        0x00000000u
#define RMON_TX_PKTS_512_1023_COUNT_SHIFT  0u
#define RMON_TX_PKTS_512_1023_COUNT_MASK   0xFFFFFFFFu

#define RMON_TX_PKTS_1024_1518             0x7040u
#define RMON_TX_PKTS_1024_1518_RESET       0x00000000u
#define RMON_TX_PKTS_1024_1518_COUNT_SHIFT 0u
#define RMON_TX_PKTS_1024_1518_COUNT_MASK  0xFFFFFFFFu

#define RMON_RX_DROP_EVENTS                0x7800u
#define RMON_RX_DROP_EVENTS_RESET          0x00000000u
#define RMON_RX_DROP_EVENTS_COUNT_SHIFT    0u
#define RMON_RX_DROP_EVENTS_COUNT_MASK     0xFFFFFFFFu

#define RMON_RX_OCTETS                     0x7804u
#define RMON_RX_OCTETS_RESET               0x00000000u
#define RMON_RX_OCTETS_COUNT_SHIFT         0u
#define RMON_RX_OCTETS_COUNT_MASK          0xFFFFFFFFu

#define RMON_RX_PKTS                       0x7808u
#define RMON_RX_PKTS_RESET                 0x00000000u
#define RMON_RX_PKTS_COUNT_SHIFT           0u
#define RMON_RX_PKTS_COUNT_MASK            0xFFFFFFFFu

#define RMON_RX_BCAST_PKTS                 0x780Cu
#define RMON_RX_BCAST_PKTS_RESET           0x00000000u
#define RMON_RX_BCAST_PKTS_COUNT_SHIFT     0u
#define RMON_RX_BCAST_PKTS_COUNT_MASK      0xFFFFFFFFu

#define RMON_RX_MCAST_PKTS                 0x7810u
#define RMON_RX_MCAST_PKTS_RESET           0x00000000u
#define RMON_RX_MCAST_PKTS_COUNT_SHIFT     0u
#define RMON_RX_MCAST_PKTS_COUNT_MASK      0xFFFFFFFFu

#define RMON_RX_CRC_ALIGN_ERR              0x7814u
#define RMON_RX_CRC_ALIGN_ERR_RESET        0x00000000u
#define RMON_RX_CRC_ALIGN_ERR_COUNT_SHIFT  0u
#define RMON_RX_CRC_ALIGN_ERR_COUNT_MASK   0xFFFFFFFFu

#define RMON_RX_UNDERSIZE                  0x7818u
#define RMON_RX_UNDERSIZE_RESET            0x00000000u
#define RMON_RX_UNDERSIZE_COUNT_SHIFT      0u
#define RMON_RX_UNDERSIZE_COUNT_MASK       0xFFFFFFFFu

#define RMON_RX_OVERSIZE                   0x781Cu
#define RMON_RX_OVERSIZE_RESET             0x00000000u
#define RMON_RX_OVERSIZE_COUNT_SHIFT       0u
#define RMON_RX_OVERSIZE_COUNT_MASK        0xFFFFFFFFu

#define RMON_RX_FRAGMENTS                  0x7820u
#define RMON_RX_FRAGMENTS_RESET            0x00000000u
#define RMON_RX_FRAGMENTS_COUNT_SHIFT      0u
#define RMON_RX_FRAGMENTS_COUNT_MASK       0xFFFFFFFFu

#define RMON_RX_JABBERS                    0x7824u
#define RMON_RX_JABBERS_RESET              0x00000000u
#define RMON_RX_JABBERS_COUNT_SHIFT        0u
#define RMON_RX_JABBERS_COUNT_MASK         0xFFFFFFFFu

#define RMON_RX_COLLISIONS                 0x7828u
#define RMON_RX_COLLISIONS_RESET           0x00000000u
#define RMON_RX_COLLISIONS_COUNT_SHIFT     0u
#define RMON_RX_COLLISIONS_COUNT_MASK      0xFFFFFFFFu

#define RMON_RX_PKTS_64                    0x782Cu
#define RMON_RX_PKTS_64_RESET              0x00000000u
#define RMON_RX_PKTS_64_COUNT_SHIFT        0u
#define RMON_RX_PKTS_64_COUNT_MASK         0xFFFFFFFFu

#define RMON_RX_PKTS_65_127                0x7830u
#define RMON_RX_PKTS_65_127_RESET          0x00000000u
#define RMON_RX_PKTS_65_127_COUNT_SHIFT    0u
#define RMON_RX_PKTS_65_127_COUNT_MASK     0xFFFFFFFFu

#define RMON_RX_PKTS_128_255               0x7834u
#define RMON_RX_PKTS_128_255_RESET         0x00000000u
#define RMON_RX_PKTS_128_255_COUNT_SHIFT   0u
#define RMON_RX_PKTS_128_255_COUNT_MASK    0xFFFFFFFFu

#define RMON_RX_PKTS_256_511               0x7838u
#define RMON_RX_PKTS_256_511_RESET         0x00000000u
#define RMON_RX_PKTS_256_511_COUNT_SHIFT   0u
#define RMON_RX_PKTS_256_511_COUNT_MASK    0xFFFFFFFFu

#define RMON_RX_PKTS_512_1023              0x783Cu
#define RMON_RX_PKTS_512_1023_RESET        0x00000000u
#define RMON_RX_PKTS_512_1023_COUNT_SHIFT  0u
#define RMON_RX_PKTS_512_1023_COUNT_MASK   0xFFFFFFFFu

#define RMON_RX_PKTS_1024_1518             0x7840u
#define RMON_RX_PKTS_1024_1518_RESET       0x00000000u
#define RMON_RX_PKTS_1024_1518_COUNT_SHIFT 0u
#define RMON_RX_PKTS_1024_1518_COUNT_MASK  0xFFFFFFFFu

#define RMON_RX_FILTER_DROP                0x7844u
#define RMON_RX_FILTER_DROP_RESET          0x00000000u
#define RMON_RX_FILTER_DROP_COUNT_SHIFT    0u
#define RMON_RX_FILTER_DROP_COUNT_MASK     0xFFFFFFFFu

#define ETH_MDIO_CTRL                      0x8000u
#define ETH_MDIO_CTRL_RESET                0x00028000u
#define ETH_MDIO_CTRL_START_SHIFT          0u
#define ETH_MDIO_CTRL_START_MASK           0x00000001u
#define ETH_MDIO_CTRL_OP_SHIFT             1u
#define ETH_MDIO_CTRL_OP_MASK              0x00000002u
#define ETH_MDIO_CTRL_REGAD_SHIFT          2u
#define ETH_MDIO_CTRL_REGAD_MASK           0x0000007Cu
#define ETH_MDIO_CTRL_PHYAD_SHIFT          7u
#define ETH_MDIO_CTRL_PHYAD_MASK           0x00000F80u
#define ETH_MDIO_CTRL_CLK_DIV_SHIFT        12u
#define ETH_MDIO_CTRL_CLK_DIV_MASK         0x000FF000u
#define ETH_MDIO_CTRL_PRE_SUPPRESS_SHIFT   20u
#define ETH_MDIO_CTRL_PRE_SUPPRESS_MASK    0x00100000u

#define ETH_MDIO_DATA                      0x8004u
#define ETH_MDIO_DATA_RESET                0x00000000u
#define ETH_MDIO_DATA_DATA_SHIFT           0u
#define ETH_MDIO_DATA_DATA_MASK            0x0000FFFFu

#define ETH_MDIO_STAT                      0x8008u
#define ETH_MDIO_STAT_RESET                0x00000000u
#define ETH_MDIO_STAT_BUSY_SHIFT           0u
#define ETH_MDIO_STAT_BUSY_MASK            0x00000001u
#define ETH_MDIO_STAT_NO_RESPONSE_SHIFT    1u
#define ETH_MDIO_STAT_NO_RESPONSE_MASK     0x00000002u
#define ETH_MDIO_STAT_START_WHILE_BUSY_SHIFT 2u
#define ETH_MDIO_STAT_START_WHILE_BUSY_MASK 0x00000004u
#define ETH_MDIO_STAT_DONE_SHIFT           3u
#define ETH_MDIO_STAT_DONE_MASK            0x00000008u

#endif /* ETH_REGS_H */
