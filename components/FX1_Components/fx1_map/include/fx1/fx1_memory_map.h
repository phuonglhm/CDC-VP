/*
 * FX1 VP memory map, interrupt map and timer constants.
 *
 * VP PLACEHOLDER VALUES, PENDING THE FX1 HAS. Every address, IRQ number and
 * frequency below was chosen for the virtual platform (plan
 * FX1_PLATFORM_FIX_PLAN.md section 7) and must be replaced here, and only here,
 * when the HAS publishes the real values. Note: this map differs from the one
 * used by platforms/VP_FX1_Full_SoC (0x1001_0000 is I2C0 and 0x1002_0000 is
 * SPI0 there).
 *
 * Plain C preprocessor definitions so that C++ platform code, C firmware,
 * assembly and the preprocessed linker script share one source. Assembly and
 * linker scripts do not accept the C `u` suffix, so constants go through
 * FX1_U32(); define FX1_LINKER_SCRIPT when preprocessing a linker script.
 */
#ifndef FX1_MEMORY_MAP_H
#define FX1_MEMORY_MAP_H

#if defined(__ASSEMBLER__) || defined(FX1_LINKER_SCRIPT)
#define FX1_U32(value) value
#else
#define FX1_U32(value) value##u
#endif

/* ---- Address map: [BASE, BASE + SIZE) ------------------------------------ */
#define FX1_BOOTROM_BASE        FX1_U32(0x00000000)
#define FX1_BOOTROM_SIZE        FX1_U32(0x00200000) /* 2 MiB, unused at bring-up      */
#define FX1_CLINT_BASE          FX1_U32(0x02000000)
#define FX1_CLINT_SIZE          FX1_U32(0x00010000) /* 64 KiB                         */
#define FX1_PLIC_BASE           FX1_U32(0x0C000000)
#define FX1_PLIC_SIZE           FX1_U32(0x01000000) /* 16 MiB                         */

/* Every APB IP decodes a 64 KiB window (HAS: "All APB interfaces have a 64KB
 * address"); the registers actually used are defined by the IP. */
#define FX1_APB_SLOT_SIZE       FX1_U32(0x00010000)
#define FX1_UART_BASE           FX1_U32(0x10000000)
#define FX1_SYS_DMA_CSR_BASE    FX1_U32(0x10010000)
#define FX1_SIM_CTRL_BASE       FX1_U32(0x10020000) /* VP only, not on silicon        */
#define FX1_ISP_CSR_BASE        FX1_U32(0x11000000)
#define FX1_NPU_CSR_BASE        FX1_U32(0x11010000) /* reserved                       */
#define FX1_H264_H265_CSR_BASE  FX1_U32(0x11020000) /* reserved                       */
#define FX1_ETH_CSR_BASE        FX1_U32(0x11030000) /* reserved                       */
#define FX1_MIPI_CSR_BASE       FX1_U32(0x11040000) /* reserved                       */

#define FX1_DDR_BASE            FX1_U32(0x80000000)
#define FX1_DDR_SIZE            FX1_U32(0x20000000) /* 512 MiB                        */

/* ---- Harts ----------------------------------------------------------------- */
#define FX1_NUM_HARTS           FX1_U32(2)
/* LR/SC reservation granule, naturally aligned (plan C7). A write by any other
 * master (the other hart, SYS_DMA, ISP ODMA) into the granule cancels the
 * reservation. VP placeholder, pending HAS: firmware must not rely on its
 * value, only keep lock words out of DMA buffers' granules. */
#define FX1_RESERVATION_GRANULE FX1_U32(64)

/* ---- CLINT (SiFive layout) ------------------------------------------------- */
#define FX1_CLINT_MSIP_OFF      FX1_U32(0x0000) /* + 4 * hart, bit 0                  */
#define FX1_CLINT_MTIMECMP_OFF  FX1_U32(0x4000) /* + 8 * hart, 64-bit                 */
#define FX1_CLINT_MTIME_OFF     FX1_U32(0xBFF8) /* 64-bit, shared                     */
#define FX1_MTIME_HZ            FX1_U32(20000000) /* OSC 20 MHz (HAS clock table)     */

/* ---- PLIC (SiFive layout) -------------------------------------------------- */
#define FX1_PLIC_PRIORITY_OFF   FX1_U32(0x000000) /* + 4 * source                     */
#define FX1_PLIC_PENDING_OFF    FX1_U32(0x001000) /* bit per source                   */
#define FX1_PLIC_ENABLE_OFF     FX1_U32(0x002000) /* + 0x80 * context                 */
#define FX1_PLIC_ENABLE_STRIDE  FX1_U32(0x80)
#define FX1_PLIC_CONTEXT_OFF    FX1_U32(0x200000) /* + 0x1000 * context               */
#define FX1_PLIC_CONTEXT_STRIDE FX1_U32(0x1000)   /* +0 threshold, +4 claim/complete  */
#define FX1_PLIC_NUM_SOURCES    FX1_U32(8)        /* IDs 1..7 used/reserved; 0 = none */
#define FX1_PLIC_PRIORITY_MAX   FX1_U32(7)
/* Context n is hart n in M-mode. */
#define FX1_PLIC_CONTEXT_HART_M(hart) (hart)

/* Interrupt source IDs. */
#define FX1_IRQ_UART            FX1_U32(1)
#define FX1_IRQ_SYS_DMA         FX1_U32(2)
#define FX1_IRQ_ISP             FX1_U32(3)
#define FX1_IRQ_NPU             FX1_U32(4) /* reserved */
#define FX1_IRQ_H264_H265       FX1_U32(5) /* reserved */
#define FX1_IRQ_ETH             FX1_U32(6) /* reserved */
#define FX1_IRQ_MIPI            FX1_U32(7) /* reserved */

/* ---- VP sim-control (finisher) ---------------------------------------------- */
#define FX1_SIM_CTRL_FINISH_OFF FX1_U32(0x0)
#define FX1_SIM_CTRL_PASS       FX1_U32(0x5555)
#define FX1_SIM_CTRL_FAIL       FX1_U32(0x3333) /* written as (code << 16) | FAIL     */
#define FX1_SIM_CTRL_IRQ_OFF    FX1_U32(0x4)    /* bit i drives test line i       */
#define FX1_SIM_CTRL_IRQ_LINES  FX1_U32(4)      /* lines -> PLIC sources 4..7     */
#define FX1_SIM_CTRL_IRQ_FIRST_SOURCE FX1_U32(4)

/* ---- Default firmware DDR layout ------------------------------------------- */
#define FX1_FW_IMAGE_BASE       FX1_U32(0x80000000) /* code + data, one ELF           */
#define FX1_FW_IMAGE_SIZE       FX1_U32(0x00400000) /* 4 MiB                          */
#define FX1_FW_STACK_BASE       FX1_U32(0x80400000) /* hart n: [base + n*size, +size) */
#define FX1_FW_STACK_SIZE       FX1_U32(0x00010000) /* 64 KiB per hart                */
#define FX1_FW_SHARED_BASE      FX1_U32(0x80500000) /* shared data + descriptors      */
#define FX1_FW_SHARED_SIZE      FX1_U32(0x00B00000)
#define FX1_FW_RAW_BUF_BASE     FX1_U32(0x81000000)
#define FX1_FW_YUV_BUF_BASE     FX1_U32(0x88000000)
#define FX1_FW_TEST_AREA_BASE   FX1_U32(0x90000000)

#endif /* FX1_MEMORY_MAP_H */
