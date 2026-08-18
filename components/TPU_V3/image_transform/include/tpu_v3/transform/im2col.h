// SPDX-License-Identifier: Apache-2.0
// Pure Im2Col descriptor/layout contract, independent of SystemC.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "tpu_v3/transform/image_transform_registers.h"

namespace cdc::components::tpu_v3::transform {

/// One tensor transform. Revision 1 accepts only Im2Col, INT8, CHW input,
/// no padding (all padding fields zero) and row-major
/// [OH*OW][C*KH*KW] output.
struct descriptor {
    std::uint32_t operation = operation_value::im2col;
    std::uint64_t source = 0;
    std::uint64_t destination = 0;
    std::uint32_t channels = 0;
    std::uint32_t input_height = 0;
    std::uint32_t input_width = 0;
    std::uint32_t kernel_height = 0;
    std::uint32_t kernel_width = 0;
    std::uint32_t stride_height = 0;
    std::uint32_t stride_width = 0;
    std::uint32_t dilation_height = 0;
    std::uint32_t dilation_width = 0;
    std::uint32_t pad_top = 0;
    std::uint32_t pad_left = 0;
    std::uint32_t pad_bottom = 0;
    std::uint32_t pad_right = 0;
    std::uint32_t datatype = datatype_value::int8;
};

struct matrix_shape {
    std::uint32_t output_height = 0;
    std::uint32_t output_width = 0;
    std::uint64_t rows = 0;
    std::uint64_t columns = 0;
    std::uint64_t source_bytes = 0;
    std::uint64_t destination_bytes = 0;
};

/// Validate operation-independent arithmetic and derive the output shape.
/// Address containment and overlap belong to the adapter, whose SRAM window is
/// a construction-time property.
error_cause derive_shape(const descriptor& work, matrix_shape& shape) noexcept;

/// Source element for one matrix coordinate. Call only after derive_shape().
std::uint64_t source_element_index(const descriptor& work,
                                   const matrix_shape& shape,
                                   std::uint64_t matrix_row,
                                   std::uint64_t matrix_column) noexcept;

/// Host-side/reference helper used by the golden gate. The running model uses
/// the same index function but all bytes still cross its native SRAM port.
std::vector<std::int8_t> im2col_reference(
    const descriptor& work, const std::vector<std::int8_t>& chw);

} // namespace cdc::components::tpu_v3::transform
