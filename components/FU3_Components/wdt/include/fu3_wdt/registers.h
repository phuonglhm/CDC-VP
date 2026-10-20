#pragma once
/* WDT HDS V1.1 register offsets, local to the SoC WDT window. */
#define FU3_WDT0_BASE 0x44050000u
#define FU3_WDT1_BASE 0x44080000u
#define FU3_WDT_CNT_LD 0x1004u
#define FU3_WDT_CNT_VAL 0x1008u
#define FU3_WDT_CNT_CTRL 0x100cu
#define FU3_WDT_INT_RAW_STAT 0x1010u
#define FU3_WDT_INT_MASK_STAT 0x1014u
#define FU3_WDT_INT_CLR 0x1018u
#define FU3_WDT_DBG_STAT 0x101cu
#define FU3_WDT_RESERVED_BASE 0x1000u
#define FU3_WDT_WINDOW_END 0x10000u
#define FU3_WDT_RESEN 1u
#define FU3_WDT_INTEN 2u
#define FU3_WDT_DBGEN 4u
#define FU3_WDT_RESDIV_SHIFT 3u
#define FU3_WDT_RESDIV_MASK 0x78u
#define FU3_WDT_TIMEOUT_SEEN 1u
#define FU3_WDT_RESET_VALUE 0xffffffffu
