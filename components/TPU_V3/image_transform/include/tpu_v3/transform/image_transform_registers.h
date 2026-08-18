// SPDX-License-Identifier: Apache-2.0
// Phase 6 ImageTransform programming model.
#pragma once

#include <cstdint>

namespace cdc::components::tpu_v3::transform {

namespace reg {
inline constexpr std::uint64_t id = 0x000;
inline constexpr std::uint64_t version = 0x004;
inline constexpr std::uint64_t capability = 0x008;
inline constexpr std::uint64_t control = 0x00C;
inline constexpr std::uint64_t status = 0x010;
inline constexpr std::uint64_t operation = 0x014;
inline constexpr std::uint64_t src_addr_lo = 0x018;
inline constexpr std::uint64_t src_addr_hi = 0x01C;
inline constexpr std::uint64_t dst_addr_lo = 0x020;
inline constexpr std::uint64_t dst_addr_hi = 0x024;
inline constexpr std::uint64_t input_channels = 0x028;
inline constexpr std::uint64_t input_height = 0x02C;
inline constexpr std::uint64_t input_width = 0x030;
inline constexpr std::uint64_t kernel_height = 0x034;
inline constexpr std::uint64_t kernel_width = 0x038;
inline constexpr std::uint64_t stride_height = 0x03C;
inline constexpr std::uint64_t stride_width = 0x040;
inline constexpr std::uint64_t dilation_height = 0x044;
inline constexpr std::uint64_t dilation_width = 0x048;
inline constexpr std::uint64_t pad_top = 0x04C;
inline constexpr std::uint64_t pad_left = 0x050;
inline constexpr std::uint64_t pad_bottom = 0x054;
inline constexpr std::uint64_t pad_right = 0x058;
inline constexpr std::uint64_t datatype = 0x05C;
inline constexpr std::uint64_t irq_enable = 0x060;
inline constexpr std::uint64_t error_cause = 0x064;
inline constexpr std::uint64_t output_height = 0x068;
inline constexpr std::uint64_t output_width = 0x06C;
inline constexpr std::uint64_t matrix_rows = 0x070;
inline constexpr std::uint64_t matrix_columns = 0x074;
inline constexpr std::uint64_t bytes_done_lo = 0x078;
inline constexpr std::uint64_t bytes_done_hi = 0x07C;
inline constexpr std::uint64_t local_requests = 0x080;
inline constexpr std::uint64_t local_bytes_lo = 0x084;
inline constexpr std::uint64_t local_bytes_hi = 0x088;
inline constexpr std::uint64_t job_count = 0x08C;
inline constexpr std::uint64_t error_count = 0x090;
inline constexpr std::uint64_t abort_count = 0x094;
inline constexpr std::uint64_t overrun_count = 0x098;
inline constexpr std::uint64_t source_tag = 0x09C;
inline constexpr std::uint64_t implemented_end = 0x0A0;
} // namespace reg

namespace control_bit {
inline constexpr std::uint32_t start = 1u << 0;
inline constexpr std::uint32_t abort = 1u << 1;
inline constexpr std::uint32_t writable_mask = start | abort;
} // namespace control_bit

namespace status_bit {
inline constexpr std::uint32_t busy = 1u << 0;
inline constexpr std::uint32_t done = 1u << 1;
inline constexpr std::uint32_t error = 1u << 2;
inline constexpr std::uint32_t aborted = 1u << 3;
inline constexpr std::uint32_t w1c_mask = done | error | aborted;
} // namespace status_bit

namespace capability_bit {
inline constexpr std::uint32_t im2col = 1u << 0;
inline constexpr std::uint32_t col2im = 1u << 1;
inline constexpr std::uint32_t int8 = 1u << 8;
inline constexpr std::uint32_t chw_input = 1u << 9;
inline constexpr std::uint32_t matrix_row_major = 1u << 10;
inline constexpr std::uint32_t value =
    im2col | int8 | chw_input | matrix_row_major;
} // namespace capability_bit

namespace irq_enable_bit {
inline constexpr std::uint32_t completion = 1u << 0;
inline constexpr std::uint32_t writable_mask = completion;
} // namespace irq_enable_bit

namespace operation_value {
inline constexpr std::uint32_t im2col = 0;
inline constexpr std::uint32_t col2im = 1;
} // namespace operation_value

namespace datatype_value {
inline constexpr std::uint32_t int8 = 0;
} // namespace datatype_value

enum class error_cause : std::uint32_t {
    none = 0,
    unavailable_operation = 1,
    invalid_dimension = 2,
    invalid_kernel = 3,
    invalid_stride = 4,
    invalid_dilation = 5,
    unsupported_padding = 6,
    unsupported_datatype = 7,
    arithmetic_overflow = 8,
    source_out_of_range = 9,
    destination_out_of_range = 10,
    region_overlap = 11,
    local_read = 12,
    local_write = 13,
    internal = 14,
};

const char* to_string(error_cause cause) noexcept;

// "TP3" plus block code 5: DMA is 4 and this is the next core-local engine.
inline constexpr std::uint32_t identity = 0x5450'3305u;
inline constexpr std::uint32_t model_version = 0x0006'0000u;

} // namespace cdc::components::tpu_v3::transform
