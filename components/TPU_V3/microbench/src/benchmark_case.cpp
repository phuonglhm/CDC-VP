// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/bench/benchmark_case.h"

#include <array>
#include <stdexcept>

namespace cdc::components::tpu_v3::bench {
namespace {

// §5.2 and §5.3. The ten counts deliberately surround the SEW=32 lane
// boundaries for VLEN 128, 256 and 512, which is why they are not round
// numbers: 15/16/17 straddle the VLEN=512 boundary of 16 lanes at e32, and
// 1024 is the long case where the tail stops mattering.
constexpr std::array<std::uint32_t, frozen_case_count> element_cases{
    3, 4, 5, 7, 8, 9, 15, 16, 17, 1024};

// §5.4. `m` is 1 for every GEMV case: that is the benchmark. The MXU uses one
// of its 64 rows to run it, which is the under-filled comparison point the
// plan asks for rather than a defect to correct.
constexpr std::array<case_shape, frozen_case_count> gemv_cases{{
    {0, 1, 4, 4},
    {0, 1, 8, 8},
    {0, 1, 16, 16},
    {0, 1, 32, 32},
    {0, 1, 64, 64},
    {0, 1, 16, 128},
    {0, 1, 64, 128},
    {0, 1, 16, 256},
    {0, 1, 64, 256},
    {0, 1, 63, 255},
}};

// §5.5. `M, N <= 64` throughout, because the verified engine accepts one tile
// and the adapter must keep refusing anything larger rather than truncating
// it. K is not bounded by the array geometry and the last two cases exercise
// that deliberately.
constexpr std::array<case_shape, frozen_case_count> gemm_cases{{
    {0, 4, 4, 4},
    {0, 8, 8, 8},
    {0, 16, 16, 16},
    {0, 32, 32, 32},
    {0, 64, 64, 64},
    {0, 16, 64, 64},
    {0, 64, 16, 64},
    {0, 63, 63, 64},
    {0, 64, 64, 128},
    {0, 64, 64, 256},
}};

void check_index(benchmark_id benchmark, std::size_t index)
{
    if (index >= frozen_case_count) {
        throw std::out_of_range(
            std::string("benchmark '") + to_string(benchmark) + "' has "
            + std::to_string(frozen_case_count) + " frozen cases (0.."
            + std::to_string(frozen_case_count - 1) + "); asked for "
            + std::to_string(index));
    }
}

} // namespace

const char* to_string(benchmark_id benchmark) noexcept
{
    switch (benchmark) {
    case benchmark_id::relu: return "relu";
    case benchmark_id::vector_dot: return "vector_dot";
    case benchmark_id::gemv_rvv: return "gemv_rvv";
    case benchmark_id::gemv_mxu: return "gemv_mxu";
    case benchmark_id::gemm: return "gemm";
    }
    return "unknown";
}

const char* to_string(bench_mode mode) noexcept
{
    switch (mode) {
    case bench_mode::kernel_only: return "kernel";
    case bench_mode::end_to_end: return "end_to_end";
    }
    return "unknown";
}

const char* to_string(bench_impl implementation) noexcept
{
    switch (implementation) {
    case bench_impl::scalar: return "scalar";
    case bench_impl::rvv: return "rvv";
    case bench_impl::mxu: return "mxu";
    }
    return "unknown";
}

bool parse(const std::string& text, benchmark_id& out)
{
    for (auto candidate : {benchmark_id::relu, benchmark_id::vector_dot,
                           benchmark_id::gemv_rvv, benchmark_id::gemv_mxu,
                           benchmark_id::gemm}) {
        if (text == to_string(candidate)) {
            out = candidate;
            return true;
        }
    }
    return false;
}

bool parse(const std::string& text, bench_mode& out)
{
    for (auto candidate : {bench_mode::kernel_only, bench_mode::end_to_end}) {
        if (text == to_string(candidate)) {
            out = candidate;
            return true;
        }
    }
    return false;
}

bool parse(const std::string& text, bench_impl& out)
{
    for (auto candidate :
         {bench_impl::scalar, bench_impl::rvv, bench_impl::mxu}) {
        if (text == to_string(candidate)) {
            out = candidate;
            return true;
        }
    }
    return false;
}

case_shape case_at(benchmark_id benchmark, std::size_t index)
{
    check_index(benchmark, index);
    switch (benchmark) {
    case benchmark_id::relu:
    case benchmark_id::vector_dot: {
        case_shape shape;
        shape.elements = element_cases[index];
        return shape;
    }
    case benchmark_id::gemv_rvv:
    case benchmark_id::gemv_mxu:
        return gemv_cases[index];
    case benchmark_id::gemm:
        return gemm_cases[index];
    }
    throw std::out_of_range("unknown benchmark id");
}

bool implementation_supported(benchmark_id benchmark,
                              bench_impl implementation) noexcept
{
    switch (benchmark) {
    case benchmark_id::relu:
    case benchmark_id::vector_dot:
        return implementation == bench_impl::scalar
            || implementation == bench_impl::rvv;
    case benchmark_id::gemv_rvv:
        return implementation == bench_impl::rvv;
    case benchmark_id::gemv_mxu:
    case benchmark_id::gemm:
        return implementation == bench_impl::mxu;
    }
    return false;
}

bool benchmark_runnable(benchmark_id benchmark) noexcept
{
    switch (benchmark) {
    case benchmark_id::relu:
    case benchmark_id::vector_dot:
    case benchmark_id::gemv_rvv:
    case benchmark_id::gemv_mxu:
    case benchmark_id::gemm:
        return true;
    }
    return false;
}

std::uint64_t expected_kernel_local_bytes(benchmark_id benchmark,
                                          const case_shape& shape,
                                          std::uint32_t executed_k)
{
    if (benchmark == benchmark_id::gemv_rvv) {
        // B once, A once per output column, C once. See the header: this is
        // the loop the kernel actually writes, checked at every frozen case in
        // `test_benchmark_case.cpp`.
        const std::uint64_t m = shape.m;
        const std::uint64_t n = shape.n;
        const std::uint64_t k = executed_k;
        return k * n + m * k * n + m * n * 4u;
    }

    case_shape executed = shape;
    executed.k = executed_k;
    return input_bytes(benchmark, executed) + output_bytes(benchmark, executed);
}

std::uint64_t expected_dma_local_bytes(benchmark_id benchmark,
                                       const case_shape& shape,
                                       bench_mode mode)
{
    if (mode != bench_mode::end_to_end) {
        return 0;
    }
    return input_bytes(benchmark, shape) + output_bytes(benchmark, shape);
}

std::uint64_t input_bytes(benchmark_id benchmark, const case_shape& shape)
{
    switch (benchmark) {
    case benchmark_id::relu:
        return static_cast<std::uint64_t>(shape.elements) * 4u;
    case benchmark_id::vector_dot:
        // Two operand vectors.
        return static_cast<std::uint64_t>(shape.elements) * 4u * 2u;
    case benchmark_id::gemv_rvv:
    case benchmark_id::gemv_mxu:
    case benchmark_id::gemm:
        // INT8 operands: A is m x k, B is k x n.
        return static_cast<std::uint64_t>(shape.m) * shape.k
            + static_cast<std::uint64_t>(shape.k) * shape.n;
    }
    return 0;
}

std::uint64_t output_bytes(benchmark_id benchmark, const case_shape& shape)
{
    switch (benchmark) {
    case benchmark_id::relu:
        return static_cast<std::uint64_t>(shape.elements) * 4u;
    case benchmark_id::vector_dot:
        // One INT32 scalar.
        return 4u;
    case benchmark_id::gemv_rvv:
    case benchmark_id::gemv_mxu:
    case benchmark_id::gemm:
        return static_cast<std::uint64_t>(shape.m) * shape.n * 4u;
    }
    return 0;
}

} // namespace cdc::components::tpu_v3::bench
