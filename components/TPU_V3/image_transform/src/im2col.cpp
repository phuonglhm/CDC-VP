// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/transform/im2col.h"

#include <limits>
#include <stdexcept>

namespace cdc::components::tpu_v3::transform {
namespace {

bool checked_mul(std::uint64_t a, std::uint64_t b,
                 std::uint64_t& result) noexcept
{
    if (a != 0 && b > std::numeric_limits<std::uint64_t>::max() / a) {
        return false;
    }
    result = a * b;
    return true;
}

} // namespace

const char* to_string(error_cause cause) noexcept
{
    switch (cause) {
    case error_cause::none: return "none";
    case error_cause::unavailable_operation: return "unavailable_operation";
    case error_cause::invalid_dimension: return "invalid_dimension";
    case error_cause::invalid_kernel: return "invalid_kernel";
    case error_cause::invalid_stride: return "invalid_stride";
    case error_cause::invalid_dilation: return "invalid_dilation";
    case error_cause::unsupported_padding: return "unsupported_padding";
    case error_cause::unsupported_datatype: return "unsupported_datatype";
    case error_cause::arithmetic_overflow: return "arithmetic_overflow";
    case error_cause::source_out_of_range: return "source_out_of_range";
    case error_cause::destination_out_of_range:
        return "destination_out_of_range";
    case error_cause::region_overlap: return "region_overlap";
    case error_cause::local_read: return "local_read";
    case error_cause::local_write: return "local_write";
    case error_cause::internal: return "internal";
    }
    return "unknown";
}

error_cause derive_shape(const descriptor& work, matrix_shape& shape) noexcept
{
    shape = {};
    if (work.operation != operation_value::im2col) {
        return error_cause::unavailable_operation;
    }
    if (work.datatype != datatype_value::int8) {
        return error_cause::unsupported_datatype;
    }
    if (work.channels == 0 || work.input_height == 0
        || work.input_width == 0) {
        return error_cause::invalid_dimension;
    }
    if (work.kernel_height == 0 || work.kernel_width == 0) {
        return error_cause::invalid_kernel;
    }
    if (work.stride_height == 0 || work.stride_width == 0) {
        return error_cause::invalid_stride;
    }
    if (work.dilation_height == 0 || work.dilation_width == 0) {
        return error_cause::invalid_dilation;
    }
    if (work.pad_top != 0 || work.pad_left != 0 || work.pad_bottom != 0
        || work.pad_right != 0) {
        // The pinned v4.2 descriptor/golden has no padding field. Supporting
        // zero-fill here would be a plausible convention, but still invented.
        return error_cause::unsupported_padding;
    }

    std::uint64_t effective_h = 0;
    std::uint64_t effective_w = 0;
    if (!checked_mul(work.kernel_height - 1u, work.dilation_height,
                     effective_h)
        || !checked_mul(work.kernel_width - 1u, work.dilation_width,
                        effective_w)) {
        return error_cause::arithmetic_overflow;
    }
    ++effective_h;
    ++effective_w;
    if (effective_h > work.input_height || effective_w > work.input_width) {
        return error_cause::invalid_kernel;
    }

    const std::uint64_t out_h =
        1 + (work.input_height - effective_h) / work.stride_height;
    const std::uint64_t out_w =
        1 + (work.input_width - effective_w) / work.stride_width;
    std::uint64_t rows = 0;
    std::uint64_t kernel_elements = 0;
    std::uint64_t columns = 0;
    std::uint64_t source_plane = 0;
    std::uint64_t source_bytes = 0;
    std::uint64_t destination_bytes = 0;
    if (!checked_mul(out_h, out_w, rows)
        || !checked_mul(work.kernel_height, work.kernel_width,
                        kernel_elements)
        || !checked_mul(work.channels, kernel_elements, columns)
        || !checked_mul(work.input_height, work.input_width, source_plane)
        || !checked_mul(work.channels, source_plane, source_bytes)
        || !checked_mul(rows, columns, destination_bytes)
        || out_h > std::numeric_limits<std::uint32_t>::max()
        || out_w > std::numeric_limits<std::uint32_t>::max()) {
        return error_cause::arithmetic_overflow;
    }

    shape.output_height = static_cast<std::uint32_t>(out_h);
    shape.output_width = static_cast<std::uint32_t>(out_w);
    shape.rows = rows;
    shape.columns = columns;
    shape.source_bytes = source_bytes;
    shape.destination_bytes = destination_bytes;
    return error_cause::none;
}

std::uint64_t source_element_index(const descriptor& work,
                                   const matrix_shape& shape,
                                   std::uint64_t matrix_row,
                                   std::uint64_t matrix_column) noexcept
{
    const std::uint64_t oy = matrix_row / shape.output_width;
    const std::uint64_t ox = matrix_row % shape.output_width;
    const std::uint64_t kernel_plane =
        std::uint64_t(work.kernel_height) * work.kernel_width;
    const std::uint64_t channel = matrix_column / kernel_plane;
    const std::uint64_t kernel_offset = matrix_column % kernel_plane;
    const std::uint64_t ky = kernel_offset / work.kernel_width;
    const std::uint64_t kx = kernel_offset % work.kernel_width;
    const std::uint64_t iy = oy * work.stride_height
        + ky * work.dilation_height;
    const std::uint64_t ix = ox * work.stride_width
        + kx * work.dilation_width;
    return (channel * work.input_height + iy) * work.input_width + ix;
}

std::vector<std::int8_t> im2col_reference(
    const descriptor& work, const std::vector<std::int8_t>& chw)
{
    matrix_shape shape;
    const error_cause error = derive_shape(work, shape);
    if (error != error_cause::none) {
        throw std::invalid_argument(std::string("invalid Im2Col descriptor: ")
                                    + to_string(error));
    }
    if (chw.size() != shape.source_bytes) {
        throw std::invalid_argument("Im2Col CHW input size does not match descriptor");
    }
    std::vector<std::int8_t> output(
        static_cast<std::size_t>(shape.destination_bytes));
    for (std::uint64_t row = 0; row < shape.rows; ++row) {
        for (std::uint64_t column = 0; column < shape.columns; ++column) {
            output[static_cast<std::size_t>(row * shape.columns + column)] =
                chw[static_cast<std::size_t>(
                    source_element_index(work, shape, row, column))];
        }
    }
    return output;
}

} // namespace cdc::components::tpu_v3::transform
