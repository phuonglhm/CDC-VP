// SPDX-License-Identifier: Apache-2.0
// Differentially factor the NPU team's convolution golden through TPU_V3
// Im2Col, then GEMM the resulting matrix. This checks layout, stride and
// dilation against source semantics rather than against another local copy of
// the same indexing loop.

#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

#include <systemc>

#include "driver/sauria_golden.h"
#include "sauria_targets.h"
#include "tpu_v3/architecture_config.h"
#include "tpu_v3/transform/im2col.h"
#include "tpu_v3/transform/transform_provenance.h"

namespace tr = cdc::components::tpu_v3::transform;
namespace tpu = cdc::components::tpu_v3;

namespace {
int failures = 0;
#define CHECK(x) do { if (!(x)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " #x "\n"; ++failures; \
} } while (false)

void source_golden_case(std::uint32_t channels, std::uint32_t height,
                        std::uint32_t width, std::uint32_t kernel,
                        std::uint32_t stride, std::uint32_t dilation,
                        std::uint32_t outputs)
{
    tr::descriptor work;
    work.channels = channels;
    work.input_height = height;
    work.input_width = width;
    work.kernel_height = kernel;
    work.kernel_width = kernel;
    work.stride_height = stride;
    work.stride_width = stride;
    work.dilation_height = dilation;
    work.dilation_width = dilation;

    tr::matrix_shape matrix_shape;
    CHECK(tr::derive_shape(work, matrix_shape) == tr::error_cause::none);
    std::vector<std::int8_t> input(matrix_shape.source_bytes);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = static_cast<std::int8_t>(
            static_cast<int>((i * 29 + 7) % 127) - 63);
    }
    const auto matrix = tr::im2col_reference(work, input);

    const std::uint64_t k = matrix_shape.columns;
    std::vector<std::int8_t> weights(outputs * k);
    for (std::size_t i = 0; i < weights.size(); ++i) {
        weights[i] = static_cast<std::int8_t>(
            static_cast<int>((i * 11 + 3) % 17) - 8);
    }

    std::vector<std::int64_t> factored(outputs * matrix_shape.rows, 0);
    for (std::uint32_t output = 0; output < outputs; ++output) {
        for (std::uint64_t row = 0; row < matrix_shape.rows; ++row) {
            std::int64_t sum = 0;
            for (std::uint64_t column = 0; column < k; ++column) {
                sum += std::int64_t(matrix[row * k + column])
                    * weights[std::uint64_t(output) * k + column];
            }
            factored[std::uint64_t(output) * matrix_shape.rows + row] = sum;
        }
    }

    sauria::SauriaConvShape source_shape{};
    source_shape.B_w = static_cast<int>(kernel);
    source_shape.B_h = static_cast<int>(kernel);
    source_shape.d = static_cast<int>(dilation);
    source_shape.s = static_cast<int>(stride);
    source_shape.C_in = static_cast<int>(channels);
    source_shape.C_w = static_cast<int>(matrix_shape.output_width);
    source_shape.C_h = static_cast<int>(matrix_shape.output_height);
    source_shape.C_out = static_cast<int>(outputs);
    source_shape.A_h = static_cast<int>(height);
    source_shape.A_w = static_cast<int>(width);
    source_shape.preload_en = 0;

    std::vector<double> source_input(input.begin(), input.end());
    std::vector<double> source_weights(weights.begin(), weights.end());
    std::vector<double> preloads(outputs * matrix_shape.rows, 0.0);
    const auto* target = sauria::sauria_find_target("int8_64x64");
    CHECK(target != nullptr);
    const auto golden = sauria::sauria_reference_conv(
        source_input.data(), source_weights.data(), preloads.data(),
        source_shape, *target);
    CHECK(golden.size() == factored.size());
    for (std::size_t i = 0; i < golden.size(); ++i) {
        CHECK(static_cast<std::int64_t>(golden[i]) == factored[i]);
    }
}
} // namespace

int sc_main(int, char*[])
{
    // Explicit small order check: each output row contains c,ky,kx in that
    // order, while rows walk oy,ox.
    tr::descriptor tiny;
    tiny.channels = 1;
    tiny.input_height = 3;
    tiny.input_width = 4;
    tiny.kernel_height = 2;
    tiny.kernel_width = 2;
    tiny.stride_height = tiny.stride_width = 1;
    tiny.dilation_height = tiny.dilation_width = 1;
    std::vector<std::int8_t> values{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    const std::vector<std::int8_t> expected{
        0,1,4,5, 1,2,5,6, 2,3,6,7,
        4,5,8,9, 5,6,9,10, 6,7,10,11};
    CHECK(tr::im2col_reference(tiny, values) == expected);

    source_golden_case(/*C=*/3, /*H=*/9, /*W=*/9, /*K=*/3,
                       /*stride=*/2, /*dilation=*/2, /*Cout=*/5);
    source_golden_case(/*C=*/2, /*H=*/8, /*W=*/8, /*K=*/5,
                       /*stride=*/1, /*dilation=*/1, /*Cout=*/3);

    tr::matrix_shape shape;
    tiny.operation = tr::operation_value::col2im;
    CHECK(tr::derive_shape(tiny, shape)
          == tr::error_cause::unavailable_operation);
    tiny.operation = tr::operation_value::im2col;
    tiny.pad_left = 1;
    CHECK(tr::derive_shape(tiny, shape)
          == tr::error_cause::unsupported_padding);
    tiny.pad_left = 0;
    tiny.datatype = 99;
    CHECK(tr::derive_shape(tiny, shape)
          == tr::error_cause::unsupported_datatype);
    tiny.datatype = tr::datatype_value::int8;
    tiny.channels = 0;
    CHECK(tr::derive_shape(tiny, shape)
          == tr::error_cause::invalid_dimension);
    tiny.channels = 1;
    tiny.kernel_height = 0;
    CHECK(tr::derive_shape(tiny, shape)
          == tr::error_cause::invalid_kernel);
    tiny.kernel_height = 2;
    tiny.stride_width = 0;
    CHECK(tr::derive_shape(tiny, shape)
          == tr::error_cause::invalid_stride);
    tiny.stride_width = 1;
    tiny.dilation_height = 0;
    CHECK(tr::derive_shape(tiny, shape)
          == tr::error_cause::invalid_dilation);
    tiny.dilation_height = 4;
    CHECK(tr::derive_shape(tiny, shape)
          == tr::error_cause::invalid_kernel);
    tiny = {};
    tiny.channels = UINT32_MAX;
    tiny.input_height = UINT32_MAX;
    tiny.input_width = UINT32_MAX;
    tiny.kernel_height = tiny.kernel_width = 1;
    tiny.stride_height = tiny.stride_width = 1;
    tiny.dilation_height = tiny.dilation_width = 1;
    CHECK(tr::derive_shape(tiny, shape)
          == tr::error_cause::arithmetic_overflow);

    CHECK((tr::capability_bit::value & tr::capability_bit::im2col) != 0);
    CHECK((tr::capability_bit::value & tr::capability_bit::col2im) == 0);
    CHECK(tr::provenance::source_tag == 0x15633417u);
    CHECK(tr::provenance::ifmap_sha256[0] == '1');
    CHECK(std::string_view(tr::provenance::source_revision)
          == tpu::im2col_source_revision);

    if (failures != 0) {
        std::cerr << failures << " Im2Col contract check(s) failed\n";
        return 1;
    }
    std::cout << "TPU_V3 Im2Col vs pinned NPU convolution golden: PASS\n";
    return 0;
}
