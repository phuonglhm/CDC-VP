/* SPDX-License-Identifier: Apache-2.0
 *
 * The firmware view of one NEO-CORE.
 *
 * Hand-written for Phase 7 and deliberately narrow: it names the addresses and
 * register offsets this image uses and nothing else. Plan §8 says the real
 * firmware header is *generated* from `tpu_v3/address_map.h` in Phase 10, with
 * a test comparing the two so a divergence is a build failure rather than a
 * runtime mystery. Until that generator exists this file is the second copy the
 * plan warns about, and the pipeline test closes the gap the only other way
 * available: it asserts every constant here against the C++ map before it runs
 * the image, so the two cannot drift silently in the meantime.
 */

#ifndef TPU_V3_NEO_CORE_PIPELINE_MAP_H
#define TPU_V3_NEO_CORE_PIPELINE_MAP_H

/* ── platform ─────────────────────────────────────────────────────────────── */

#define GLOBAL_RAM_BASE   0x80000000u

/* Chip 1, core 1 — deliberately neither zero.
 *
 * `mhartid` is `chip * 2 + core`, so this hart is 3. Running on chip 0 core 0
 * would make the expected id 0, which is also what an uninitialised CSR reads:
 * an integration that ignored the configured identity entirely would pass the
 * check. Picking 1/1 rather than 0/1 also separates `chip * 2 + core` from
 * `chip + core` and from plain `core`, which would give 2 and 1.
 *
 * Written as arithmetic on the map's own strides rather than as literals, so a
 * changed position cannot leave a stale hard-coded address behind. The pipeline
 * test asserts every one of these against `address_map.h` before it runs the
 * image.
 */
#define CHIP_ID           1u
#define CORE_ID           1u
#define EXPECTED_MHARTID  (CHIP_ID * 2u + CORE_ID)

#define CHIP_BASE         (0xC0000000u + CHIP_ID * 0x08000000u)
#define CORE_BASE         (CHIP_BASE + CORE_ID * 0x02000000u)
#define CORE_SRAM_BASE    (CORE_BASE + 0x00000000u)
#define SA_CONTROL_BASE   (CORE_BASE + 0x01010000u)
#define DMA_CONTROL_BASE  (CORE_BASE + 0x01020000u)
#define TRF_CONTROL_BASE  (CORE_BASE + 0x01030000u)

/* ── NEO DMA ──────────────────────────────────────────────────────────────── */

#define DMA_REG_ID           0x000u
#define DMA_REG_CONTROL      0x008u
#define DMA_REG_STATUS       0x00Cu
#define DMA_REG_SRC_LO       0x010u
#define DMA_REG_SRC_HI       0x014u
#define DMA_REG_DST_LO       0x018u
#define DMA_REG_DST_HI       0x01Cu
#define DMA_REG_LENGTH       0x020u
#define DMA_REG_IRQ_ENABLE   0x024u
#define DMA_REG_ERROR_CAUSE  0x028u
#define DMA_REG_BYTES_DONE   0x02Cu

#define DMA_CTRL_START       (1u << 0)
#define DMA_STATUS_BUSY      (1u << 0)
#define DMA_STATUS_DONE      (1u << 1)
#define DMA_STATUS_ERROR     (1u << 2)

/* ── Transform (Im2Col) ───────────────────────────────────────────────────── */

#define TRF_REG_ID           0x000u
#define TRF_REG_CAPABILITY   0x008u
#define TRF_REG_CONTROL      0x00Cu
#define TRF_REG_STATUS       0x010u
#define TRF_REG_OPERATION    0x014u
#define TRF_REG_SRC_LO       0x018u
#define TRF_REG_SRC_HI       0x01Cu
#define TRF_REG_DST_LO       0x020u
#define TRF_REG_DST_HI       0x024u
#define TRF_REG_IN_CHANNELS  0x028u
#define TRF_REG_IN_HEIGHT    0x02Cu
#define TRF_REG_IN_WIDTH     0x030u
#define TRF_REG_KERNEL_H     0x034u
#define TRF_REG_KERNEL_W     0x038u
#define TRF_REG_STRIDE_H     0x03Cu
#define TRF_REG_STRIDE_W     0x040u
#define TRF_REG_DILATION_H   0x044u
#define TRF_REG_DILATION_W   0x048u
#define TRF_REG_PAD_TOP      0x04Cu
#define TRF_REG_PAD_LEFT     0x050u
#define TRF_REG_PAD_BOTTOM   0x054u
#define TRF_REG_PAD_RIGHT    0x058u
#define TRF_REG_DATATYPE     0x05Cu
#define TRF_REG_IRQ_ENABLE   0x060u
#define TRF_REG_ERROR_CAUSE  0x064u
#define TRF_REG_OUT_HEIGHT   0x068u
#define TRF_REG_OUT_WIDTH    0x06Cu
#define TRF_REG_MATRIX_ROWS  0x070u
#define TRF_REG_MATRIX_COLS  0x074u

#define TRF_CTRL_START       (1u << 0)
#define TRF_STATUS_BUSY      (1u << 0)
#define TRF_STATUS_DONE      (1u << 1)
#define TRF_STATUS_ERROR     (1u << 2)

#define TRF_OP_IM2COL        0u
#define TRF_OP_COL2IM        1u
#define TRF_DATATYPE_INT8    0u

#define TRF_CAP_IM2COL       (1u << 0)
#define TRF_CAP_COL2IM       (1u << 1)

/* ── MXU (SA_CONTROL is the retained implementation identifier, D20) ───────── */

#define SA_REG_ID            0x000u
#define SA_REG_CONTROL       0x008u
#define SA_REG_STATUS        0x00Cu
#define SA_REG_DIM_M         0x010u
#define SA_REG_DIM_N         0x014u
#define SA_REG_DIM_K         0x018u
#define SA_REG_A_LO          0x020u
#define SA_REG_A_HI          0x024u
#define SA_REG_B_LO          0x028u
#define SA_REG_B_HI          0x02Cu
#define SA_REG_C_LO          0x030u
#define SA_REG_C_HI          0x034u
#define SA_REG_A_STRIDE      0x038u
#define SA_REG_B_STRIDE      0x03Cu
#define SA_REG_C_STRIDE      0x040u
#define SA_REG_DATATYPE      0x044u
#define SA_REG_IRQ_ENABLE    0x048u
#define SA_REG_ERROR_CAUSE   0x04Cu

#define SA_CTRL_START        (1u << 0)
#define SA_STATUS_BUSY       (1u << 0)
#define SA_STATUS_DONE       (1u << 1)
#define SA_STATUS_ERROR      (1u << 2)

#define SA_DATATYPE_INT8_INT32 2u

/* ── the workload ─────────────────────────────────────────────────────────────
 *
 * Chosen so every dimension stays inside the frozen limits and the golden
 * result is small enough to check exactly: the MXU accepts one tile with
 * M, N <= 64 (Phase 5), and Im2Col is the no-padding INT8 CHW subset (D18).
 *
 *   input   1 x 5 x 5 INT8, CHW
 *   kernel  3 x 3, stride 1, dilation 1, no padding
 *   ->      OH = OW = 3, so A is [9][9]
 *   weights B is [9][4] INT8
 *   ->      C is [9][4] INT32
 */
#define IN_CHANNELS   1u
#define IN_HEIGHT     5u
#define IN_WIDTH      5u
#define KERNEL_H      3u
#define KERNEL_W      3u
#define OUT_HEIGHT    3u
#define OUT_WIDTH     3u

#define MAT_M         (OUT_HEIGHT * OUT_WIDTH)              /* 9 */
#define MAT_K         (IN_CHANNELS * KERNEL_H * KERNEL_W)   /* 9 */
#define MAT_N         4u

#define INPUT_BYTES   (IN_CHANNELS * IN_HEIGHT * IN_WIDTH)  /* 25 */
#define WEIGHT_BYTES  (MAT_K * MAT_N)                       /* 36 */
#define A_BYTES       (MAT_M * MAT_K)                       /* 81 */
#define C_BYTES       (MAT_M * MAT_N * 4u)                  /* 144 */

/* Where the host stages the operands, and where the core works.
 *
 * The input and the weights start in global RAM precisely so the DMA has
 * something to do: the pipeline this image exists to prove is
 * `DMA -> Transform -> MXU -> RVV`, and staging them straight into core SRAM
 * would skip its first stage.
 */
#define HOST_INPUT_ADDR   (GLOBAL_RAM_BASE + 0x1000u)
#define HOST_WEIGHT_ADDR  (GLOBAL_RAM_BASE + 0x1100u)

#define SRAM_INPUT_ADDR   (CORE_SRAM_BASE + 0x0000u)
#define SRAM_WEIGHT_ADDR  (CORE_SRAM_BASE + 0x0200u)
#define SRAM_A_ADDR       (CORE_SRAM_BASE + 0x0400u)
#define SRAM_C_ADDR       (CORE_SRAM_BASE + 0x0800u)

/* ── simulator-only host I/O ──────────────────────────────────────────────────
 *
 * The same window Phase 2 and Phase 4.5 use, in the unmapped low-address hole.
 * Not a TPU_V3 architectural peripheral and it must not leak into the SoC
 * firmware ABI.
 */
#define SIM_BASE          0x000F0000u
#define SIM_EXIT_KIND     (SIM_BASE + 0u)
#define SIM_EXIT_STATUS   (SIM_BASE + 4u)
#define SIM_EXIT_MCAUSE   (SIM_BASE + 8u)
#define SIM_EXIT_MEPC     (SIM_BASE + 12u)
#define SIM_STAGE_MARK    (SIM_BASE + 16u)
#define SIM_RVV_CHECKSUM  (SIM_BASE + 20u)
#define SIM_RVV_MAXIMUM   (SIM_BASE + 24u)
#define SIM_RVV_NEGATIVES (SIM_BASE + 28u)
/* `mhartid` as the firmware reads it.
 *
 * The identity is only useful if *firmware* can see it: ARCHITECTURE.md §2
 * says chip and core index are derived from `mhartid`, never supplied
 * separately, so a C++ test asserting the model's own accessor proves the
 * model agrees with itself and not that the value reached the guest. */
#define SIM_MHARTID       (SIM_BASE + 32u)

#define SIM_EXIT_KIND_NORMAL  0u
#define SIM_EXIT_KIND_TRAPPED 1u
#define SIM_EXIT_PASS         0u

/* Stage markers, so a failure says which stage of the pipeline stopped. */
#define STAGE_START     1u
#define STAGE_DMA_IN    2u
#define STAGE_DMA_WEI   3u
#define STAGE_IM2COL    4u
#define STAGE_MXU       5u
#define STAGE_RVV       6u
#define STAGE_DONE      7u

#define TRAP_ACTION_ABORT  0u
#define TRAP_ACTION_RESUME 1u
#define TRAP_ACTION_SKIP   2u
#define TRAP_ACTION_RESUME_AT 3u

#endif /* TPU_V3_NEO_CORE_PIPELINE_MAP_H */
