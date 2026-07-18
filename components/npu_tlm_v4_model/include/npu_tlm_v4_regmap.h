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

inline constexpr std::uint32_t MMIO_SIZE = 0x0001'0000;

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
inline constexpr std::uint32_t OP_GEMM                 = 0u;
inline constexpr std::uint32_t CORE_ID_VALUE           = 0x5341'5534u; // "SAU4"

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
