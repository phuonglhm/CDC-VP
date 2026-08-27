// SPDX-License-Identifier: Apache-2.0
//
// The frozen MB1-MB4 case tables of `NEO_CORE_MICROBENCH_DSE_PLAN.md` §5.
//
// They live in C++ rather than in a configuration file on purpose. The plan
// calls these shapes *frozen*, and a frozen list that is read from a file is
// one a run can quietly disagree with: a mistyped size would produce a result
// row that looks exactly like a legitimate one. Here a case index is validated
// against a table the compiler carries, and asking for an eleventh case is an
// error rather than an empty shape.
//
// All four benchmarks are tabulated here and their runnable state is explicit,
// so adding a workload cannot silently change the frozen shape table.

#ifndef CDC_COMPONENTS_TPU_V3_BENCH_BENCHMARK_CASE_H
#define CDC_COMPONENTS_TPU_V3_BENCH_BENCHMARK_CASE_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace cdc::components::tpu_v3::bench {

/// The five runnable workload/implementation pairs the plan names. GEMV is two
/// entries rather than one benchmark with an implementation flag because §5.4
/// runs *both* and compares them; they are separate experiments producing
/// separate rows.
enum class benchmark_id {
    relu,       ///< MB1, §5.2
    vector_dot, ///< MB2, §5.3
    gemv_rvv,   ///< MB3 over RVV, §5.4
    gemv_mxu,   ///< MB3 over the MXU, §5.4
    gemm,       ///< MB4, §5.5
};

/// §6. The two modes measure different machines and their numbers may never be
/// mixed: kernel-only excludes every byte the DMA moves, end-to-end includes
/// both legs.
enum class bench_mode {
    kernel_only,
    end_to_end,
};

/// How the arithmetic is performed inside the core.
enum class bench_impl {
    scalar, ///< RV32 scalar, the correctness and speedup baseline
    rvv,    ///< RVV 1.0 strip-mined
    mxu,    ///< the 64x64 INT8/INT32 engine
};

/// One case's shape. Which fields are meaningful depends on the benchmark, and
/// `case_at()` leaves the others zero rather than filling them with a
/// plausible-looking value that nothing computed.
struct case_shape {
    /// MB1/MB2: element count.
    std::uint32_t elements = 0;
    /// MB3/MB4: `A[m x k] * B[k x n] -> C[m x n]`. MB3 freezes `m = 1`.
    std::uint32_t m = 0;
    std::uint32_t n = 0;
    std::uint32_t k = 0;
};

/// Every benchmark has exactly ten cases (§5). A run that produced nine or
/// eleven rows would not be the frozen experiment.
inline constexpr std::size_t frozen_case_count = 10;

const char* to_string(benchmark_id benchmark) noexcept;
const char* to_string(bench_mode mode) noexcept;
const char* to_string(bench_impl implementation) noexcept;

/// Parse a command-line spelling. Returns false on anything unrecognised; the
/// caller reports the offending text. Deliberately not case-insensitive and
/// with no abbreviations: a result row records what was asked for, and
/// `--benchmark rel` resolving to `relu` would put a name in the row that the
/// operator never typed.
bool parse(const std::string& text, benchmark_id& out);
bool parse(const std::string& text, bench_mode& out);
bool parse(const std::string& text, bench_impl& out);

/// The frozen shape of one case.
///
/// Throws `std::out_of_range` for `index >= frozen_case_count`, naming the
/// benchmark and the bound.
case_shape case_at(benchmark_id benchmark, std::size_t index);

/// True when this benchmark accepts this implementation. MB1/MB2 are scalar or
/// RVV; `gemv_mxu` and `gemm` are the MXU; `gemv_rvv` is RVV. An unsupported
/// pair is refused rather than silently substituted.
bool implementation_supported(benchmark_id benchmark,
                              bench_impl implementation) noexcept;

/// True when the harness can actually run this benchmark today. Kept as a
/// separate predicate from the frozen table so an unfinished future benchmark
/// can be tabulated without being accepted by the runner.
bool benchmark_runnable(benchmark_id benchmark) noexcept;

/// Bytes the kernel is expected to move on the native local plane (G3 clause 1).
///
/// This is the algorithm's declared traffic, not the tensor footprint, and the
/// difference is the whole point. `local_plane_bytes` proves the fabric and
/// core SRAM agree on a total; it cannot notice a kernel that reads its input
/// twice, because both observers would report the larger number and the golden
/// would still be right. Tying the measurement to what the algorithm *should*
/// move is what closes that gap.
///
/// Element-wise and MXU kernels move exactly one input and one output:
///
/// ```text
/// relu, vector_dot, gemv_mxu, gemm    input_bytes + output_bytes
/// ```
///
/// GEMV over RVV does not, and the excess is a property of its loop rather
/// than an inefficiency to hide. It walks the output columns and re-reads the
/// whole of A inside that loop, so A is read `n` times:
///
/// ```text
/// gemv_rvv    k*n (B once) + m*k*n (A per column) + m*n*4 (C once)
/// ```
///
/// `executed_k` is the K the engine was actually programmed with, which the
/// arithmetic mutation reduces by one; pass `shape.k` for an unmutated run.
std::uint64_t expected_kernel_local_bytes(benchmark_id benchmark,
                                          const case_shape& shape,
                                          std::uint32_t executed_k);

/// Bytes the DMA is expected to move on the local plane.
///
/// One inbound leg writing the input and one outbound leg reading the result,
/// in end-to-end mode only. Zero in kernel-only mode, where the plan forbids
/// the DMA from being involved at all.
std::uint64_t expected_dma_local_bytes(benchmark_id benchmark,
                                       const case_shape& shape,
                                       bench_mode mode);

/// Bytes one case moves per tensor, for the buffer-size checks the harness
/// makes before it stages anything. MB1/MB2 elements are INT32.
std::uint64_t input_bytes(benchmark_id benchmark, const case_shape& shape);
std::uint64_t output_bytes(benchmark_id benchmark, const case_shape& shape);

} // namespace cdc::components::tpu_v3::bench

#endif // CDC_COMPONENTS_TPU_V3_BENCH_BENCHMARK_CASE_H
