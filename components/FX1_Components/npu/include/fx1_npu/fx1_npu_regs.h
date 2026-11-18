/*
 * FX1 NPU (SAURIA v4.7, fx1_npu) register map for C firmware (C99, no C++).
 *
 * Mirrors include/fx1_npu/fx1_npu_regmap.h (the wrapper's C++ definitions).
 * Every value is checked against it at compile time by
 * tests/test_regs_header.cpp, so the two cannot drift. Register ABI: VP_ABI
 * 1.0 (FX1_NPU_VP_ABI_VALUE); offsets and bits are frozen for ABI 1.x.
 *
 * Offsets are relative to the NPU aperture (FX1_NPU_VP_BASE in
 * fx1/fx1_memory_map.h) unless the comment says "local": local offsets of the
 * wrapper bank are added to FX1_NPU_WRAPPER_BASE (FX1_NPU_BANK()). All
 * registers are 32-bit and need naturally aligned 32-bit loads/stores; any
 * other access is an access fault.
 *
 * The behaviour behind these registers is the contract of
 * components/FX1_Components/npu/README.md.
 */
#ifndef FX1_NPU_REGS_H
#define FX1_NPU_REGS_H

/* ---- aperture layout --------------------------------------------------------- */
#define FX1_NPU_MMIO_SIZE           0x00100000u
#define FX1_NPU_WRAPPER_BASE        0x00030000u /* GEMM bank + VP bank */
#define FX1_NPU_WRAPPER_WINDOW_SIZE 0x00002000u
#define FX1_NPU_RICH_ALIAS_BASE     0x00010000u /* model host space 0x40000000.. */
#define FX1_NPU_RICH_ALIAS_SIZE     0x00001000u
#define FX1_NPU_BANK(local)         (FX1_NPU_WRAPPER_BASE + (local))
/* Aperture offset of a model host address 0x40000000 + o (rich interface). */
#define FX1_NPU_MODEL_RICH_BASE     0x40000000u
#define FX1_NPU_RICH_OF_MODEL(a)    (FX1_NPU_RICH_ALIAS_BASE + ((a) - FX1_NPU_MODEL_RICH_BASE))

/* ---- wrapper bank (local) -------------------------------------------------------- */
#define FX1_NPU_CTRL                0x0000u
#define FX1_NPU_STATUS              0x0004u /* W1C: DONE, ERROR */
#define FX1_NPU_IRQ_ENABLE          0x0008u
#define FX1_NPU_IRQ_STATUS          0x000Cu /* W1C */
#define FX1_NPU_SRC_ADDR            0x0010u
#define FX1_NPU_DST_ADDR            0x0014u
#define FX1_NPU_SRC_SIZE_BYTES      0x001Cu
#define FX1_NPU_DST_SIZE_BYTES      0x0020u
#define FX1_NPU_WIDTH               0x0024u
#define FX1_NPU_HEIGHT              0x0028u
#define FX1_NPU_FORMAT              0x0030u
#define FX1_NPU_OP_MODE             0x0034u
#define FX1_NPU_WEIGHTS_ADDR        0x0038u
#define FX1_NPU_WEIGHTS_SIZE_BYTES  0x0040u
#define FX1_NPU_K_DIMENSION         0x1000u
#define FX1_NPU_CYCLE_COUNT         0x1010u
#define FX1_NPU_BYTES_READ          0x1014u
#define FX1_NPU_BYTES_WRITTEN       0x1018u
#define FX1_NPU_LAST_ERROR          0x101Cu
#define FX1_NPU_CORE_ID             0x1020u
#define FX1_NPU_RICH_WINDOW_BASE    0x1130u
#define FX1_NPU_RICH_WINDOW_SIZE    0x1134u
#define FX1_NPU_PERF_EXEC_CYCLES    0x1200u
#define FX1_NPU_PERF_MAC_OPS        0x1208u
#define FX1_NPU_PERF_TOTAL_CYCLES   0x1214u
#define FX1_NPU_PERF_DDR_READ_BYTES  0x129Cu
#define FX1_NPU_PERF_DDR_WRITE_BYTES 0x12A0u

#define FX1_NPU_CTRL_ENABLE         (1u << 0)
#define FX1_NPU_CTRL_START          (1u << 1)
#define FX1_NPU_CTRL_SOFT_RESET     (1u << 2)
#define FX1_NPU_CTRL_IRQ_EN         (1u << 3)
#define FX1_NPU_STATUS_BUSY         (1u << 0)
#define FX1_NPU_STATUS_DONE         (1u << 1)
#define FX1_NPU_STATUS_ERROR        (1u << 2)
#define FX1_NPU_STATUS_IDLE         (1u << 3)
#define FX1_NPU_IRQ_DONE            (1u << 0)
#define FX1_NPU_IRQ_ERROR           (1u << 1)

#define FX1_NPU_FORMAT_INT8_INT8_INT32 1u
#define FX1_NPU_OP_GEMM             0u
#define FX1_NPU_CORE_ID_VALUE       0x53413432u /* "SA42" */
#define FX1_NPU_GEMM_ROWS           32u         /* GEMM bank: 32 x K by K x 32 */
#define FX1_NPU_GEMM_COLS           32u

/* LAST_ERROR */
#define FX1_NPU_ERR_NONE               0u
#define FX1_NPU_ERR_DISABLED           1u
#define FX1_NPU_ERR_BUSY               2u
#define FX1_NPU_ERR_INVALID_DIMENSIONS 3u
#define FX1_NPU_ERR_INVALID_FORMAT     4u
#define FX1_NPU_ERR_INVALID_OPERATION  5u
#define FX1_NPU_ERR_INVALID_ADDRESS    6u
#define FX1_NPU_ERR_INVALID_SIZE       7u
#define FX1_NPU_ERR_DMA_READ           8u
#define FX1_NPU_ERR_DMA_WRITE          9u
#define FX1_NPU_ERR_CORE_DEADLOCK      10u
#define FX1_NPU_ERR_CORE_TIMEOUT       11u
#define FX1_NPU_ERR_RESET_ABORTED      12u

/* ---- VP bank (local; FX1 VP only, not SAURIA / MAS / HAS) ------------------- */
#define FX1_NPU_VP_ID               0x1800u /* RO */
#define FX1_NPU_VP_ABI              0x1804u /* RO, major << 16 | minor */
#define FX1_NPU_VP_MODEL_HASH       0x1808u /* RO */
#define FX1_NPU_VP_FEATURE          0x180Cu /* RO */
#define FX1_NPU_VP_QUEUE_DEPTH      0x1810u /* RO */
#define FX1_NPU_VP_QUEUE_FREE       0x1814u /* RO */
#define FX1_NPU_VP_CMD              0x1818u /* WO, reads 0 */
#define FX1_NPU_VP_STATUS           0x181Cu /* RO */
#define FX1_NPU_VP_ERROR            0x1820u /* RO / W1C */
#define FX1_NPU_VP_ERROR_INFO       0x1824u /* RO */
#define FX1_NPU_VP_PUSH_WAIT_NS     0x1828u /* RW */
#define FX1_NPU_VP_ACCEPTED         0x182Cu /* RO */
#define FX1_NPU_VP_ABORT_DROPPED    0x1830u /* RO */
#define FX1_NPU_VP_WINDOW_MAX       0x1834u /* RO */

#define FX1_NPU_VP_ID_VALUE         0x46584E50u /* "FXNP" */
#define FX1_NPU_VP_ABI_VALUE        0x00010000u /* 1.0 */
#define FX1_NPU_VP_ABI_MAJOR(v)     ((v) >> 16)

#define FX1_NPU_FEATURE_NATIVE_GEMM  (1u << 0)
#define FX1_NPU_FEATURE_RICH_HAS     (1u << 1)
#define FX1_NPU_FEATURE_DFC_WINDOW   (1u << 2)
#define FX1_NPU_FEATURE_DMA_TENSOR   (1u << 3)
#define FX1_NPU_FEATURE_ABORT        (1u << 4)
#define FX1_NPU_FEATURE_BOUNDED_PUSH (1u << 5)
#define FX1_NPU_FEATURE_QUEUE        (1u << 6)
#define FX1_NPU_FEATURE_ATTN_LN      (1u << 7)

#define FX1_NPU_VP_CMD_ABORT        (1u << 0)
#define FX1_NPU_VP_CMD_CLEAR_ERROR  (1u << 1)

#define FX1_NPU_VP_STATUS_QUIESCENT       (1u << 0)
#define FX1_NPU_VP_STATUS_ENGINE_BUSY     (1u << 1)
#define FX1_NPU_VP_STATUS_ABORT_ACTIVE    (1u << 2)
#define FX1_NPU_VP_STATUS_COPYBACK_ACTIVE (1u << 3)
#define FX1_NPU_VP_STATUS_NATIVE_BUSY     (1u << 4)
#define FX1_NPU_VP_STATUS_QUEUED_SHIFT    8u
#define FX1_NPU_VP_STATUS_QUEUED_MASK     (0xFFu << 8)

#define FX1_NPU_VP_ERROR_PUSH_TIMEOUT (1u << 0)
#define FX1_NPU_VP_ERROR_BAD_ADDR     (1u << 1)
#define FX1_NPU_VP_ERROR_ABORTED      (1u << 2)
#define FX1_NPU_VP_ERROR_DMA          (1u << 3)
#define FX1_NPU_VP_ERROR_REFUSED      (1u << 4)
#define FX1_NPU_VP_ERROR_UNSUPPORTED  (1u << 5)
#define FX1_NPU_VP_ERROR_MODEL        (1u << 6)
#define FX1_NPU_VP_ERROR_BUSY         (1u << 7)
#define FX1_NPU_VP_ERROR_ALL          0xFFu

/* ---- rich interface (model host addresses; aperture: FX1_NPU_RICH_OF_MODEL) --- */
#define FX1_NPU_M_PUSH_A        0x40000310u
#define FX1_NPU_M_PUSH_B        0x40000314u
#define FX1_NPU_M_STATUS        0x40000318u /* any write clears ERROR + code */
#define FX1_NPU_M_RETIRED       0x4000031Cu
/* parameters (sticky across pushes) */
#define FX1_NPU_M_IN_ADDR       0x40000400u
#define FX1_NPU_M_W_ADDR        0x40000404u
#define FX1_NPU_M_OUT_ADDR      0x40000408u
#define FX1_NPU_M_BIAS_ADDR     0x4000040Cu
#define FX1_NPU_M_M             0x40000410u
#define FX1_NPU_M_K             0x40000414u
#define FX1_NPU_M_N             0x40000418u
#define FX1_NPU_M_KERNEL_H      0x4000041Cu
#define FX1_NPU_M_KERNEL_W      0x40000420u
#define FX1_NPU_M_STRIDE        0x40000424u
#define FX1_NPU_M_PADDING       0x40000428u
#define FX1_NPU_M_ACT_TYPE      0x4000042Cu
#define FX1_NPU_M_HAS_SKIP      0x40000430u
#define FX1_NPU_M_SKIP_ADDR     0x40000434u
#define FX1_NPU_M_A_ADDR        0x40000444u /* Q / GAMMA / A */
#define FX1_NPU_M_B_ADDR        0x40000448u /* K / B */
#define FX1_NPU_M_SEQ_LEN       0x40000450u /* ADD: element count */
#define FX1_NPU_M_MODE          0x40000454u /* ELEM_WISE mode */
#define FX1_NPU_M_A_LEN         0x40000464u
#define FX1_NPU_M_B_LEN         0x40000468u
#define FX1_NPU_M_PARAM_FIRST   0x40000400u
#define FX1_NPU_M_PARAM_LAST    0x40000468u /* inclusive */
/* extension bank (one-shot: every lane-A push clears it) */
#define FX1_NPU_M_IN_C          0x4000046Cu
#define FX1_NPU_M_IN_H          0x40000470u
#define FX1_NPU_M_IN_W          0x40000474u
#define FX1_NPU_M_OUT_C         0x40000478u
#define FX1_NPU_M_OUT_H         0x4000047Cu
#define FX1_NPU_M_OUT_W         0x40000480u
#define FX1_NPU_M_TILE_COUT     0x40000484u
#define FX1_NPU_M_TILE_H        0x40000488u
#define FX1_NPU_M_TILE_W        0x4000048Cu
#define FX1_NPU_M_SCALE_ADDR    0x40000490u
#define FX1_NPU_M_SHIFT_ADDR    0x40000494u
#define FX1_NPU_M_LUT_ADDR      0x40000498u
#define FX1_NPU_M_ZP_OUT        0x4000049Cu
#define FX1_NPU_M_FLAGS         0x400004A0u
#define FX1_NPU_M_ZP_A          0x400004A4u
#define FX1_NPU_M_ZP_B          0x400004A8u
#define FX1_NPU_M_ZP_O          0x400004ACu
#define FX1_NPU_M_SA            0x400004B0u
#define FX1_NPU_M_SHA           0x400004B4u
#define FX1_NPU_M_SB            0x400004B8u
#define FX1_NPU_M_SHB           0x400004BCu
#define FX1_NPU_M_SO            0x400004C0u
#define FX1_NPU_M_SHO           0x400004C4u
#define FX1_NPU_M_POOL_K        0x400004C8u
#define FX1_NPU_M_POOL_P        0x400004CCu
#define FX1_NPU_M_TILE_CIN      0x400004D4u
#define FX1_NPU_M_Y_USED        0x400004D8u
#define FX1_NPU_M_EXT_FIRST     0x4000046Cu
#define FX1_NPU_M_EXT_END       0x40000504u /* exclusive */

#define FX1_NPU_OP_GEMM_FUSED   0x12u
#define FX1_NPU_OP_FUSED_ATTN   0x13u
#define FX1_NPU_OP_LAYERNORM    0x14u
#define FX1_NPU_OP_ELEM_WISE    0x15u
#define FX1_NPU_OP_DMA_TENSOR   0x16u
#define FX1_NPU_OP_NEO_DOORBELL 0x17u
#define FX1_NPU_ELEM_ADD        0u
#define FX1_NPU_ELEM_MAX_POOL   1u
#define FX1_NPU_ELEM_AVG_POOL   5u
#define FX1_NPU_ACT_NONE        0u
#define FX1_NPU_ACT_RELU        1u

#define FX1_NPU_RICH_STATUS_BUSY        (1u << 0)
#define FX1_NPU_RICH_STATUS_QUEUE_FULL  (1u << 1)
#define FX1_NPU_RICH_STATUS_ERROR       (1u << 2)
#define FX1_NPU_RICH_STATUS_CODE(v)     ((v) >> 16)

/* RICH_STATUS[31:16]: model codes 1..11, VP codes 0x100.. */
#define FX1_NPU_RICH_ERR_Q_OVF          1u
#define FX1_NPU_RICH_ERR_LANE_B         2u
#define FX1_NPU_RICH_ERR_LEGACY64       3u
#define FX1_NPU_RICH_ERR_UNSUPPORTED_OP 4u
#define FX1_NPU_RICH_ERR_UNSUPPORTED_MODE 5u
#define FX1_NPU_RICH_ERR_BROADCAST      6u
#define FX1_NPU_RICH_ERR_NEED_LUT       7u
#define FX1_NPU_RICH_ERR_BAD_GEOM       8u
#define FX1_NPU_RICH_ERR_SCALE_RANGE    9u
#define FX1_NPU_RICH_ERR_NO_TILING      10u
#define FX1_NPU_RICH_ERR_VP_BAD_ADDR    0x100u
#define FX1_NPU_RICH_ERR_VP_UNSUPPORTED 0x101u
#define FX1_NPU_RICH_ERR_VP_BUSY        0x102u
#define FX1_NPU_RICH_ERR_VP_MODEL       0x103u
#define FX1_NPU_RICH_ERR_VP_DMA         0x104u
#define FX1_NPU_RICH_ERR_VP_ABORTED     0x105u

/* ---- neo_lite register file (DFC) ------------------------------------------------ */
#define FX1_NPU_M_NEO_BASE      0x40000800u
#define FX1_NPU_M_NEO_MARKER    0x400004E4u /* write 0 before the words */
#define FX1_NPU_NEO_CTRL_OFF    0x000u      /* bit 0 START (arms), bit 1 DONE (W1C) */
#define FX1_NPU_NEO_IEN_OFF     0x008u
#define FX1_NPU_NEO_ISTATUS_OFF 0x00Cu      /* bit 0, W1C */
#define FX1_NPU_NEO_REG_OFF     0x010u      /* + 4 * word */
#define FX1_NPU_NEO_REG_COUNT   82u
#define FX1_NPU_NEO_CTRL_START  (1u << 0)
#define FX1_NPU_NEO_CTRL_DONE   (1u << 1)

/* ---- native SAURIA window (low level; see the README) -------------------------- */
#define FX1_NPU_NATIVE_CONTROL      0x00000000u /* raw START: value & 0xFF != 0 */
#define FX1_NPU_NATIVE_CFG_PROFILE  0x00000004u
#define FX1_NPU_NATIVE_SRAMA_OFFSET 0x00040000u
#define FX1_NPU_NATIVE_SRAMB_OFFSET 0x00080000u
#define FX1_NPU_NATIVE_SRAMC_OFFSET 0x000C0000u

#endif /* FX1_NPU_REGS_H */
