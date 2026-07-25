// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace cdc::components::npu_v4_reg {

// Common accelerator register window (docs/peripheral_memory_map.md).
inline constexpr std::uint32_t CTRL               = 0x0000;
inline constexpr std::uint32_t STATUS             = 0x0004;
inline constexpr std::uint32_t IRQ_ENABLE         = 0x0008;
inline constexpr std::uint32_t IRQ_STATUS         = 0x000C;
inline constexpr std::uint32_t SRC_ADDR           = 0x0010;
inline constexpr std::uint32_t DST_ADDR           = 0x0014;
inline constexpr std::uint32_t SCRATCH_ADDR       = 0x0018;
inline constexpr std::uint32_t SRC_SIZE_BYTES     = 0x001C;
inline constexpr std::uint32_t DST_SIZE_BYTES     = 0x0020;
inline constexpr std::uint32_t WIDTH              = 0x0024;
inline constexpr std::uint32_t HEIGHT             = 0x0028;
inline constexpr std::uint32_t SRC_STRIDE_BYTES   = 0x002C;
inline constexpr std::uint32_t FORMAT             = 0x0030;
inline constexpr std::uint32_t OP_MODE            = 0x0034;
inline constexpr std::uint32_t WEIGHTS_ADDR       = 0x0038;
inline constexpr std::uint32_t PARAM_ADDR         = 0x003C;
inline constexpr std::uint32_t WEIGHTS_SIZE_BYTES = 0x0040;

// SAURIA-specific parameter/debug bank.
inline constexpr std::uint32_t K_DIMENSION         = 0x1000;
inline constexpr std::uint32_t ZERO_THRESHOLD_FP32 = 0x1004;
inline constexpr std::uint32_t ROWS_ACTIVE         = 0x1008;
inline constexpr std::uint32_t DILATION_PATTERN    = 0x100C;
inline constexpr std::uint32_t CYCLE_COUNT         = 0x1010;
inline constexpr std::uint32_t BYTES_READ          = 0x1014;
inline constexpr std::uint32_t BYTES_WRITTEN       = 0x1018;
inline constexpr std::uint32_t LAST_ERROR          = 0x101C;
inline constexpr std::uint32_t CORE_ID             = 0x1020;

// SAURIA MP1 V1.1 v4.1 software parameters. These are optional for the
// legacy INT8/INT8/INT32 path and are consumed by the TFLM-style INT8 output
// path.
inline constexpr std::uint32_t INPUT_OFFSET        = 0x1100;
inline constexpr std::uint32_t WEIGHT_OFFSET       = 0x1104;
inline constexpr std::uint32_t OUTPUT_OFFSET       = 0x1108;
inline constexpr std::uint32_t ACTIVATION_MIN      = 0x110C;
inline constexpr std::uint32_t ACTIVATION_MAX      = 0x1110;
inline constexpr std::uint32_t BIAS_ADDR           = 0x1114;
inline constexpr std::uint32_t BIAS_SIZE_BYTES     = 0x1118;
inline constexpr std::uint32_t MULTIPLIER_ADDR     = 0x111C;
inline constexpr std::uint32_t MULTIPLIER_SIZE_BYTES = 0x1120;
inline constexpr std::uint32_t SHIFT_ADDR          = 0x1124;
inline constexpr std::uint32_t SHIFT_SIZE_BYTES    = 0x1128;

// Low 32-bit performance/evaluation counters reported by the v4.1 bridge.
inline constexpr std::uint32_t PERF_EXEC_CYCLES    = 0x1200;
inline constexpr std::uint32_t PERF_STALL_CYCLES   = 0x1204;
inline constexpr std::uint32_t PERF_MAC_OPS        = 0x1208;
inline constexpr std::uint32_t PERF_ACTIVE_PE_CYCLES = 0x120C;
inline constexpr std::uint32_t PERF_TOTAL_PE_CYCLES  = 0x1210;
inline constexpr std::uint32_t EVAL_TOTAL_CYCLES   = 0x1214;
inline constexpr std::uint32_t EVAL_PROCESSING_CYCLES = 0x1218;
inline constexpr std::uint32_t EVAL_TRANSFER_CYCLES = 0x121C;
inline constexpr std::uint32_t EVAL_DMA_STALL_CYCLES = 0x1220;
inline constexpr std::uint32_t EVAL_DDR_READ_BYTES = 0x1224;
inline constexpr std::uint32_t EVAL_DDR_WRITE_BYTES = 0x1228;

// Raw/native SAURIA MP1 V1.1 register/SRAM window. These offsets mirror
// SAURIA's sauria_types.h so firmware can drive the v4.1+ model through the
// low-level bridge path when needed. They are relative to the NPU base.
inline constexpr std::uint32_t NATIVE_SAURIA_MEM_ADDR_MASK = 0x003C'0000;
inline constexpr std::uint32_t NATIVE_CFG_REGS_OFFSET      = 0x0000'0000;
inline constexpr std::uint32_t NATIVE_SRAMA_OFFSET         = 0x0004'0000;
inline constexpr std::uint32_t NATIVE_SRAMB_OFFSET         = 0x0008'0000;
inline constexpr std::uint32_t NATIVE_SRAMC_OFFSET         = 0x000C'0000;

inline constexpr std::uint32_t NATIVE_CFG_CON_OFFSET       = 0x0000'0200;
inline constexpr std::uint32_t NATIVE_CFG_ACT_OFFSET       = 0x0000'0400;
inline constexpr std::uint32_t NATIVE_CFG_WEI_OFFSET       = 0x0000'0600;
inline constexpr std::uint32_t NATIVE_CFG_OUT_OFFSET       = 0x0000'0800;
inline constexpr std::uint32_t NATIVE_CFG_LAYER_OFFSET     = 0x0000'0A00;

inline constexpr std::uint32_t NATIVE_CFG_ACT_BASE_ADDR =
    NATIVE_CFG_ACT_OFFSET + 0x80;
inline constexpr std::uint32_t NATIVE_CFG_WEI_BASE_ADDR =
    NATIVE_CFG_WEI_OFFSET + 0x80;
inline constexpr std::uint32_t NATIVE_CFG_OUT_BASE_ADDR =
    NATIVE_CFG_OUT_OFFSET + 0x80;

inline constexpr std::uint32_t NATIVE_WEI_WLIM       = NATIVE_CFG_WEI_OFFSET + 0x10;
inline constexpr std::uint32_t NATIVE_WEI_WSTEP      = NATIVE_CFG_WEI_OFFSET + 0x14;
inline constexpr std::uint32_t NATIVE_WEI_KLIM       = NATIVE_CFG_WEI_OFFSET + 0x18;
inline constexpr std::uint32_t NATIVE_WEI_KSTEP      = NATIVE_CFG_WEI_OFFSET + 0x1C;
inline constexpr std::uint32_t NATIVE_WEI_TIL_XLIM   = NATIVE_CFG_WEI_OFFSET + 0x20;
inline constexpr std::uint32_t NATIVE_WEI_TIL_XSTEP  = NATIVE_CFG_WEI_OFFSET + 0x24;
inline constexpr std::uint32_t NATIVE_WEI_COLS_ACTIVE = NATIVE_CFG_WEI_OFFSET + 0x28;
inline constexpr std::uint32_t NATIVE_WEI_WALIGNED   = NATIVE_CFG_WEI_OFFSET + 0x2C;

inline constexpr std::uint32_t NATIVE_ACT_XLIM       = NATIVE_CFG_ACT_OFFSET + 0x14;
inline constexpr std::uint32_t NATIVE_ACT_XSTEP      = NATIVE_CFG_ACT_OFFSET + 0x18;
inline constexpr std::uint32_t NATIVE_ACT_YLIM       = NATIVE_CFG_ACT_OFFSET + 0x1C;
inline constexpr std::uint32_t NATIVE_ACT_YSTEP      = NATIVE_CFG_ACT_OFFSET + 0x20;
inline constexpr std::uint32_t NATIVE_ACT_CHLIM      = NATIVE_CFG_ACT_OFFSET + 0x24;
inline constexpr std::uint32_t NATIVE_ACT_CHSTEP     = NATIVE_CFG_ACT_OFFSET + 0x2C;
inline constexpr std::uint32_t NATIVE_ACT_TIL_XLIM   = NATIVE_CFG_ACT_OFFSET + 0x30;
inline constexpr std::uint32_t NATIVE_ACT_TIL_XSTEP  = NATIVE_CFG_ACT_OFFSET + 0x34;
inline constexpr std::uint32_t NATIVE_ACT_TIL_YLIM   = NATIVE_CFG_ACT_OFFSET + 0x38;
inline constexpr std::uint32_t NATIVE_ACT_TIL_YSTEP  = NATIVE_CFG_ACT_OFFSET + 0x3C;

inline constexpr std::uint32_t NATIVE_NCONTEXTS      = NATIVE_CFG_OUT_OFFSET + 0x00;
inline constexpr std::uint32_t NATIVE_TIL_CYLIM      = NATIVE_CFG_OUT_OFFSET + 0x14;
inline constexpr std::uint32_t NATIVE_TIL_CYSTEP     = NATIVE_CFG_OUT_OFFSET + 0x18;
inline constexpr std::uint32_t NATIVE_TIL_CKLIM      = NATIVE_CFG_OUT_OFFSET + 0x1C;
inline constexpr std::uint32_t NATIVE_TIL_CKSTEP     = NATIVE_CFG_OUT_OFFSET + 0x20;
inline constexpr std::uint32_t NATIVE_INACTIVE_COLS  = NATIVE_CFG_OUT_OFFSET + 0x24;
inline constexpr std::uint32_t NATIVE_PRELOAD_EN     = NATIVE_CFG_OUT_OFFSET + 0x28;

inline constexpr std::uint32_t NATIVE_IN_H           = NATIVE_CFG_LAYER_OFFSET + 0x00;
inline constexpr std::uint32_t NATIVE_IN_W           = NATIVE_CFG_LAYER_OFFSET + 0x04;
inline constexpr std::uint32_t NATIVE_IN_C           = NATIVE_CFG_LAYER_OFFSET + 0x08;
inline constexpr std::uint32_t NATIVE_OUT_H          = NATIVE_CFG_LAYER_OFFSET + 0x0C;
inline constexpr std::uint32_t NATIVE_OUT_W          = NATIVE_CFG_LAYER_OFFSET + 0x10;
inline constexpr std::uint32_t NATIVE_OUT_C          = NATIVE_CFG_LAYER_OFFSET + 0x14;
inline constexpr std::uint32_t NATIVE_KERNEL_H       = NATIVE_CFG_LAYER_OFFSET + 0x18;
inline constexpr std::uint32_t NATIVE_KERNEL_W       = NATIVE_CFG_LAYER_OFFSET + 0x1C;
inline constexpr std::uint32_t NATIVE_STRIDE         = NATIVE_CFG_LAYER_OFFSET + 0x20;
inline constexpr std::uint32_t NATIVE_PADDING        = NATIVE_CFG_LAYER_OFFSET + 0x24;
inline constexpr std::uint32_t NATIVE_DILATION       = NATIVE_CFG_LAYER_OFFSET + 0x28;
inline constexpr std::uint32_t NATIVE_DIL_PAT        = NATIVE_CFG_LAYER_OFFSET + 0x2C;
inline constexpr std::uint32_t NATIVE_TILE_X         = NATIVE_CFG_LAYER_OFFSET + 0x30;
inline constexpr std::uint32_t NATIVE_TILE_Y         = NATIVE_CFG_LAYER_OFFSET + 0x34;
inline constexpr std::uint32_t NATIVE_TILE_K         = NATIVE_CFG_LAYER_OFFSET + 0x38;
inline constexpr std::uint32_t NATIVE_TILE_C         = NATIVE_CFG_LAYER_OFFSET + 0x3C;
inline constexpr std::uint32_t NATIVE_X_USED         = NATIVE_CFG_LAYER_OFFSET + 0x40;
inline constexpr std::uint32_t NATIVE_Y_USED         = NATIVE_CFG_LAYER_OFFSET + 0x44;

// 1 MiB covers the native CFG area plus SRAMA/SRAMB/SRAMC host windows.
inline constexpr std::uint32_t MMIO_SIZE = 0x0010'0000;

inline constexpr std::uint32_t CTRL_ENABLE     = 1u << 0;
inline constexpr std::uint32_t CTRL_START      = 1u << 1;
inline constexpr std::uint32_t CTRL_SOFT_RESET = 1u << 2;
inline constexpr std::uint32_t CTRL_IRQ_EN     = 1u << 3;

inline constexpr std::uint32_t STATUS_BUSY  = 1u << 0;
inline constexpr std::uint32_t STATUS_DONE  = 1u << 1;
inline constexpr std::uint32_t STATUS_ERROR = 1u << 2;
inline constexpr std::uint32_t STATUS_IDLE  = 1u << 3;

inline constexpr std::uint32_t IRQ_DONE  = 1u << 0;
inline constexpr std::uint32_t IRQ_ERROR = 1u << 1;

inline constexpr std::uint32_t FORMAT_INT8_INT8_INT32 = 1u;
inline constexpr std::uint32_t FORMAT_INT8_INT8_INT8  = 2u;
inline constexpr std::uint32_t OP_GEMM                 = 0u;
inline constexpr std::uint32_t CORE_ID_VALUE           = 0x5341'3431u; // "SA41"

enum class error_code : std::uint32_t {
    none = 0,
    disabled,
    busy,
    invalid_dimensions,
    invalid_format,
    invalid_operation,
    invalid_address,
    invalid_size,
    dma_read,
    dma_write,
    core_deadlock,
    core_timeout,
    reset_aborted,
};

} // namespace cdc::components::npu_v4_reg
