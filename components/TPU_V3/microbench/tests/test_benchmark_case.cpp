// SPDX-License-Identifier: Apache-2.0
//
// The frozen case tables (§5) and the golden arithmetic (§5.1, §5.2).
//
// Plain `main()`: none of this needs an elaboration, and a golden that needed
// SystemC to run would be a golden that could reach the model.

#include "tpu_v3/bench/benchmark_case.h"
#include "tpu_v3/bench/golden.h"

extern "C" {
#include "bench_map.h"
}

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace bench = cdc::components::tpu_v3::bench;

namespace {

int failures = 0;

#define CHECK(cond)                                                           \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " #cond " @ " << __FILE__ << ':'      \
                      << __LINE__ << '\n';                                    \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

#define CHECK_MSG(cond, msg)                                                  \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " << (msg) << " @ " << __FILE__       \
                      << ':' << __LINE__ << '\n';                             \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

using bench::bench_impl;
using bench::benchmark_id;
using bench::bench_mode;
using bench::input_pattern;

constexpr benchmark_id all_benchmarks[] = {
    benchmark_id::relu, benchmark_id::vector_dot, benchmark_id::gemv_rvv,
    benchmark_id::gemv_mxu, benchmark_id::gemm};

/// The exact ten counts §5.2 freezes. Written out again here rather than
/// referenced from the table under test: a test that reads its expectation
/// from the code it is testing agrees with a typo.
void mb1_and_mb2_use_the_ten_frozen_element_counts()
{
    const std::vector<std::uint32_t> expected{3, 4, 5, 7, 8, 9, 15, 16, 17,
                                              1024};
    for (auto benchmark : {benchmark_id::relu, benchmark_id::vector_dot}) {
        std::vector<std::uint32_t> actual;
        for (std::size_t i = 0; i < bench::frozen_case_count; ++i) {
            actual.push_back(bench::case_at(benchmark, i).elements);
        }
        CHECK_MSG(actual == expected,
                  std::string("the frozen element counts for '")
                      + bench::to_string(benchmark) + "' have drifted");
    }
}

void gemv_freezes_m_to_one_and_stays_inside_the_array()
{
    for (std::size_t i = 0; i < bench::frozen_case_count; ++i) {
        const auto shape = bench::case_at(benchmark_id::gemv_mxu, i);
        CHECK_MSG(shape.m == 1,
                  "GEMV case " + std::to_string(i)
                      + " does not have M = 1, which is the benchmark");
        CHECK_MSG(shape.n <= 64,
                  "GEMV case " + std::to_string(i)
                      + " exceeds the 64-column array; the adapter must refuse "
                        "it, so it cannot be a frozen case");
    }
}

void gemm_cases_stay_inside_the_one_verified_tile()
{
    // Phase 5 accepts one tile with M, N <= 64 and does not bound K. Both
    // halves matter: a case with N = 65 would be refused by the adapter and a
    // case with K = 256 must *not* be, which is why the last two are here.
    bool has_long_k = false;
    for (std::size_t i = 0; i < bench::frozen_case_count; ++i) {
        const auto shape = bench::case_at(benchmark_id::gemm, i);
        CHECK_MSG(shape.m <= 64 && shape.n <= 64,
                  "GEMM case " + std::to_string(i)
                      + " exceeds the verified 64x64 tile");
        CHECK(shape.k > 0);
        if (shape.k > 64) {
            has_long_k = true;
        }
    }
    CHECK_MSG(has_long_k,
              "no GEMM case has K > 64, so the frozen set never exercises the "
              "K dimension the array geometry does not bound");
}

void every_benchmark_has_exactly_ten_cases_and_refuses_an_eleventh()
{
    for (auto benchmark : all_benchmarks) {
        for (std::size_t i = 0; i < bench::frozen_case_count; ++i) {
            bench::case_at(benchmark, i); // must not throw
        }
        bool threw = false;
        try {
            bench::case_at(benchmark, bench::frozen_case_count);
        } catch (const std::out_of_range&) {
            threw = true;
        }
        CHECK_MSG(threw,
                  std::string("benchmark '") + bench::to_string(benchmark)
                      + "' accepted an out-of-range case index instead of "
                        "refusing it");
    }
}

void implementations_are_refused_rather_than_substituted()
{
    CHECK(bench::implementation_supported(benchmark_id::relu,
                                          bench_impl::scalar));
    CHECK(bench::implementation_supported(benchmark_id::relu, bench_impl::rvv));
    CHECK(!bench::implementation_supported(benchmark_id::relu,
                                           bench_impl::mxu));
    CHECK(!bench::implementation_supported(benchmark_id::gemm,
                                           bench_impl::rvv));
    CHECK(bench::implementation_supported(benchmark_id::gemv_mxu,
                                          bench_impl::mxu));
    CHECK(!bench::implementation_supported(benchmark_id::gemv_rvv,
                                           bench_impl::mxu));
}

void every_g2_benchmark_is_runnable()
{
    for (auto benchmark : all_benchmarks) {
        CHECK_MSG(bench::benchmark_runnable(benchmark),
                  std::string("G2 benchmark '") + bench::to_string(benchmark)
                      + "' is still refused");
    }
}

void matrix_golden_is_independent_and_exact()
{
    const std::vector<std::int8_t> a{1, 2, 3, -1, 0, 2};
    const std::vector<std::int8_t> b{4, 5, 6, 7, 8, 9};
    const std::vector<std::int32_t> expected{40, 46, 12, 13};
    CHECK(bench::matrix_multiply_int8_golden(a, b, 2, 2, 3) == expected);

    bool threw = false;
    try {
        bench::matrix_multiply_int8_golden({1}, {1}, 0, 1, 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw, "the matrix golden accepted a zero dimension");

    threw = false;
    try {
        bench::matrix_multiply_int8_golden({1}, {1}, 1, 2, 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw, "the matrix golden accepted a wrong operand length");
}

void matrix_inputs_are_deterministic_and_non_vacuous()
{
    const auto first
        = bench::generate_int8_inputs(1234, 64, input_pattern::mixed);
    const auto again
        = bench::generate_int8_inputs(1234, 64, input_pattern::mixed);
    const auto other
        = bench::generate_int8_inputs(1235, 64, input_pattern::mixed);
    CHECK(first == again);
    CHECK(first != other);
    const auto profile = bench::profile_of(first);
    CHECK(profile.negatives > 0 && profile.positives > 0 && profile.zeros > 0);

    const auto boundary
        = bench::generate_int8_inputs(0, 5, input_pattern::boundary);
    CHECK(boundary
          == std::vector<std::int8_t>({std::numeric_limits<std::int8_t>::min(),
                                       std::numeric_limits<std::int8_t>::max(),
                                       -1, 0, 1}));

    for (auto benchmark : {benchmark_id::gemv_rvv, benchmark_id::gemv_mxu,
                           benchmark_id::gemm}) {
        for (std::size_t i = 0; i < bench::frozen_case_count; ++i) {
            const auto shape = bench::case_at(benchmark, i);
            const std::int64_t worst
                = static_cast<std::int64_t>(shape.k) * 128 * 128;
            CHECK_MSG(worst <= std::numeric_limits<std::int32_t>::max(),
                      std::string(bench::to_string(benchmark)) + " case "
                          + std::to_string(i)
                          + " can overflow INT32 accumulation");
        }
    }
}

void names_round_trip()
{
    for (auto benchmark : all_benchmarks) {
        benchmark_id parsed{};
        CHECK(bench::parse(bench::to_string(benchmark), parsed));
        CHECK(parsed == benchmark);
    }
    for (auto mode : {bench_mode::kernel_only, bench_mode::end_to_end}) {
        bench_mode parsed{};
        CHECK(bench::parse(bench::to_string(mode), parsed));
        CHECK(parsed == mode);
    }
    for (auto pattern :
         {input_pattern::mixed, input_pattern::all_negative,
          input_pattern::all_positive, input_pattern::zero,
          input_pattern::boundary}) {
        input_pattern parsed{};
        CHECK(bench::parse(bench::to_string(pattern), parsed));
        CHECK(parsed == pattern);
    }

    benchmark_id ignored{};
    // Abbreviations are rejected: a row records the name that was typed, and
    // `rel` resolving to `relu` would record one nobody asked for.
    CHECK(!bench::parse("rel", ignored));
    CHECK(!bench::parse("RELU", ignored));
    CHECK(!bench::parse("", ignored));
}

// ── the golden ───────────────────────────────────────────────────────────────

void the_same_seed_gives_the_same_vector()
{
    const auto first
        = bench::generate_int32_inputs(12345, 64, input_pattern::mixed);
    const auto again
        = bench::generate_int32_inputs(12345, 64, input_pattern::mixed);
    const auto different
        = bench::generate_int32_inputs(12346, 64, input_pattern::mixed);
    CHECK_MSG(first == again,
              "the generator is not a function of the seed, so a recorded seed "
              "does not reproduce the run it names");
    CHECK_MSG(first != different,
              "two different seeds produced identical data");
}

/// At one seed, a shorter case is the prefix of a longer one.
///
/// Worth pinning rather than leaving to chance. MB1 exists to straddle the lane
/// boundary, and cases 7 and 8 (N = 16 and N = 17) are only a clean comparison
/// if they share their first sixteen values: then the difference between the
/// two rows is the tail element and nothing else. A generator that reseeded per
/// length would make every pair of adjacent cases differ in all of their data.
void one_seed_gives_nested_cases()
{
    const auto n16
        = bench::generate_int32_inputs(7, 16, input_pattern::mixed);
    const auto n17
        = bench::generate_int32_inputs(7, 17, input_pattern::mixed);
    const auto n1024
        = bench::generate_int32_inputs(7, 1024, input_pattern::mixed);
    CHECK(n16.size() == 16 && n17.size() == 17 && n1024.size() == 1024);
    CHECK_MSG(std::equal(n16.begin(), n16.end(), n17.begin()),
              "N = 16 is not a prefix of N = 17, so the two cases differ in "
              "more than the tail element they were chosen to isolate");
    CHECK(std::equal(n17.begin(), n17.end(), n1024.begin()));
}

void mixed_inputs_are_never_vacuous()
{
    for (std::size_t i = 0; i < bench::frozen_case_count; ++i) {
        const auto count = bench::case_at(benchmark_id::relu, i).elements;
        const auto values
            = bench::generate_int32_inputs(0xC0FFEE, count,
                                           input_pattern::mixed);
        const auto profile = bench::profile_of(values);
        CHECK_MSG(profile.negatives > 0,
                  "MB1 case " + std::to_string(i) + " (N = "
                      + std::to_string(count)
                      + ") has no negative input, so ReLU would pass without "
                        "clamping anything");
        CHECK_MSG(profile.positives > 0,
                  "MB1 case " + std::to_string(i) + " (N = "
                      + std::to_string(count)
                      + ") has no positive input, so a kernel that returned "
                        "all zeros would pass");
    }
}

void every_value_stays_inside_the_documented_bound()
{
    const auto values
        = bench::generate_int32_inputs(99, 4096, input_pattern::mixed);
    for (std::int32_t value : values) {
        CHECK(value >= -bench::mixed_magnitude_bound
              && value <= bench::mixed_magnitude_bound);
    }
}

void the_edge_patterns_are_what_they_claim()
{
    const auto negatives
        = bench::generate_int32_inputs(1, 32, input_pattern::all_negative);
    CHECK(bench::profile_of(negatives).negatives == 32);
    CHECK_MSG(bench::relu_golden(negatives)
                  == std::vector<std::int32_t>(32, 0),
              "ReLU of an all-negative vector is not all zeros");

    const auto positives
        = bench::generate_int32_inputs(1, 32, input_pattern::all_positive);
    CHECK(bench::profile_of(positives).positives == 32);
    CHECK_MSG(bench::relu_golden(positives) == positives,
              "ReLU of an all-positive vector changed it");

    const auto zeros
        = bench::generate_int32_inputs(1, 32, input_pattern::zero);
    CHECK(bench::profile_of(zeros).zeros == 32);

    const auto boundary
        = bench::generate_int32_inputs(1, 5, input_pattern::boundary);
    CHECK(boundary[0] == std::numeric_limits<std::int32_t>::min());
    CHECK(boundary[1] == std::numeric_limits<std::int32_t>::max());
    CHECK(boundary[2] == -1);
    CHECK(boundary[3] == 0);
    CHECK(boundary[4] == 1);
    const auto golden = bench::relu_golden(boundary);
    CHECK_MSG(golden[0] == 0, "ReLU(INT32_MIN) must be 0");
    CHECK_MSG(golden[1] == std::numeric_limits<std::int32_t>::max(),
              "ReLU(INT32_MAX) must be INT32_MAX");

    // The whole point of the boundary pattern is that both halves of the
    // operation fire, so the harness may demand both signs from it.
    CHECK(bench::pattern_requires_both_signs(input_pattern::boundary));
    CHECK(bench::pattern_requires_both_signs(input_pattern::mixed));
    CHECK(!bench::pattern_requires_both_signs(input_pattern::all_positive));
    CHECK(!bench::pattern_requires_both_signs(input_pattern::zero));
}

void the_checksum_wraps_rather_than_overflowing()
{
    // Two INT32_MAX values. A signed accumulation here is undefined behaviour;
    // the documented contract is a wrapping unsigned sum, and the value below
    // is what that produces.
    const std::vector<std::int32_t> values{
        std::numeric_limits<std::int32_t>::max(),
        std::numeric_limits<std::int32_t>::max()};
    CHECK(bench::wrapping_checksum(values) == 0xFFFFFFFEu);
    CHECK(bench::wrapping_checksum({}) == 0u);
}

/// The MB2 operand bound must make the exact dot product representable at
/// every frozen case — checked, not asserted in a comment.
///
/// This test exists because the comment it replaces was wrong: it claimed the
/// MB1 bound of 2^20 was safe for MB2, when 1024 * (2^20)^2 is 2^50.
void the_dot_product_bound_keeps_every_frozen_case_exact()
{
    for (std::size_t i = 0; i < bench::frozen_case_count; ++i) {
        const auto count = bench::case_at(benchmark_id::vector_dot, i).elements;
        const std::int64_t bound = bench::dot_product_magnitude_bound(count);
        CHECK_MSG(bound > 0,
                  "MB2 case " + std::to_string(i) + " has no usable bound");

        // The worst case: every product at the bound, all the same sign.
        const std::int64_t worst = static_cast<std::int64_t>(count) * bound * bound;
        CHECK_MSG(worst <= std::numeric_limits<std::int32_t>::max(),
                  "MB2 case " + std::to_string(i) + " (N = "
                      + std::to_string(count) + ") overflows INT32 at its own "
                        "bound: " + std::to_string(worst));

        // And it must be tight: one larger would overflow, or the bound is
        // needlessly throwing away input range.
        const std::int64_t next = bound + 1;
        CHECK_MSG(static_cast<std::int64_t>(count) * next * next
                      > std::numeric_limits<std::int32_t>::max(),
                  "MB2 case " + std::to_string(i)
                      + " has a bound smaller than it needs to be");
    }

    // The specific value the old comment got wrong.
    CHECK_MSG(bench::dot_product_magnitude_bound(1024)
                  < bench::mixed_magnitude_bound,
              "the MB2 bound at N = 1024 is not below the MB1 bound, so the "
              "distinction this function exists to make has been lost");
    CHECK(bench::dot_product_magnitude_bound(0) == 0);
}

/// Every frozen case must fit the shared firmware buffers.
///
/// The buffers were sized against MB1 and a comment claiming they left room
/// for MB3 and MB4. They did not: frozen MB4 case `(64,64,256)` needs 32768
/// operand bytes against 16384-byte buffers whose destination began
/// immediately after the source, so staging it would have overwritten its own
/// operands. Checked here as well as at runner start-up because a unit test
/// catches it in a compile rather than in a simulation nobody runs until the
/// benchmark is implemented.
void every_frozen_case_fits_the_firmware_buffers()
{
    for (auto benchmark : all_benchmarks) {
        for (std::size_t i = 0; i < bench::frozen_case_count; ++i) {
            const auto shape = bench::case_at(benchmark, i);
            const std::uint64_t in = bench::input_bytes(benchmark, shape);
            const std::uint64_t out = bench::output_bytes(benchmark, shape);
            const std::string where
                = std::string(bench::to_string(benchmark)) + " case "
                + std::to_string(i);
            CHECK_MSG(in <= BENCH_SRAM_BUF_BYTES,
                      where + " needs " + std::to_string(in)
                          + " operand bytes, more than BENCH_SRAM_BUF_BYTES");
            CHECK_MSG(out <= BENCH_SRAM_BUF_BYTES,
                      where + " needs " + std::to_string(out)
                          + " result bytes, more than BENCH_SRAM_BUF_BYTES");
            CHECK_MSG(in <= BENCH_HOST_BUF_BYTES
                          && out <= BENCH_HOST_BUF_BYTES,
                      where + " does not fit the host staging buffers");
        }
    }
    // The source buffer must end before the destination begins, or a large
    // case corrupts its own operands rather than overflowing anything.
    CHECK_MSG(BENCH_SRAM_SRC_ADDR + BENCH_SRAM_BUF_BYTES
                  <= BENCH_SRAM_DST_ADDR,
              "the core SRAM source buffer runs into the destination buffer");
    CHECK_MSG(BENCH_HOST_SRC_ADDR + BENCH_HOST_BUF_BYTES
                  <= BENCH_HOST_DST_ADDR,
              "the host source buffer runs into the destination buffer");
}

/// MB2's golden, and the contract that keeps it exact.
void the_dot_product_golden_is_exact_and_refuses_what_is_not()
{
    for (std::size_t i = 0; i < bench::frozen_case_count; ++i) {
        const auto n = bench::case_at(benchmark_id::vector_dot, i).elements;
        const std::int32_t bound = bench::dot_product_magnitude_bound(n);

        for (auto pattern :
             {input_pattern::mixed, input_pattern::all_negative,
              input_pattern::all_positive, input_pattern::zero,
              input_pattern::boundary}) {
            const auto operands
                = bench::generate_dot_operands(4242, n * 2, pattern, bound);
            CHECK(operands.size() == n * 2);
            for (std::int32_t v : operands) {
                CHECK_MSG(v >= -bound && v <= bound,
                          "MB2 case " + std::to_string(i) + " pattern '"
                              + bench::to_string(pattern)
                              + "' produced an operand outside its bound");
            }
            const std::vector<std::int32_t> a(operands.begin(),
                                              operands.begin() + n);
            const std::vector<std::int32_t> b(operands.begin() + n,
                                              operands.end());
            // Must not throw at any frozen case, at any pattern: that is what
            // the bound is for.
            const std::int32_t result = bench::dot_product_golden(a, b);
            std::int64_t expected = 0;
            for (std::uint32_t j = 0; j < n; ++j) {
                expected += static_cast<std::int64_t>(a[j]) * b[j];
            }
            CHECK_MSG(static_cast<std::int64_t>(result) == expected,
                      "MB2 case " + std::to_string(i)
                          + " golden disagrees with a 64-bit recomputation");
        }
    }

    // Out-of-contract operands are refused, not wrapped. A wrapped golden
    // would present as the model disagreeing with arithmetic.
    bool threw = false;
    try {
        const std::vector<std::int32_t> big(
            4, std::numeric_limits<std::int32_t>::max());
        bench::dot_product_golden(big, big);
    } catch (const std::overflow_error&) {
        threw = true;
    }
    CHECK_MSG(threw, "the golden wrapped instead of refusing an overflow");

    threw = false;
    try {
        bench::dot_product_golden({1, 2}, {1});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_MSG(threw, "the golden accepted operands of different lengths");

    CHECK(bench::dot_product_golden({}, {}) == 0);
    CHECK(bench::dot_product_golden({3, -4}, {5, 6}) == 3 * 5 + (-4) * 6);
}

void byte_footprints_match_the_shapes()
{
    const auto relu = bench::case_at(benchmark_id::relu, 9);
    CHECK(bench::input_bytes(benchmark_id::relu, relu) == 1024u * 4u);
    CHECK(bench::output_bytes(benchmark_id::relu, relu) == 1024u * 4u);

    const auto dot = bench::case_at(benchmark_id::vector_dot, 9);
    CHECK(bench::input_bytes(benchmark_id::vector_dot, dot) == 1024u * 4u * 2u);
    CHECK(bench::output_bytes(benchmark_id::vector_dot, dot) == 4u);

    const auto gemm = bench::case_at(benchmark_id::gemm, 4); // 64,64,64
    CHECK(bench::input_bytes(benchmark_id::gemm, gemm) == 64u * 64u * 2u);
    CHECK(bench::output_bytes(benchmark_id::gemm, gemm) == 64u * 64u * 4u);
}

} // namespace

int main()
{
    mb1_and_mb2_use_the_ten_frozen_element_counts();
    gemv_freezes_m_to_one_and_stays_inside_the_array();
    gemm_cases_stay_inside_the_one_verified_tile();
    every_benchmark_has_exactly_ten_cases_and_refuses_an_eleventh();
    implementations_are_refused_rather_than_substituted();
    every_g2_benchmark_is_runnable();
    names_round_trip();

    the_same_seed_gives_the_same_vector();
    one_seed_gives_nested_cases();
    mixed_inputs_are_never_vacuous();
    every_value_stays_inside_the_documented_bound();
    the_edge_patterns_are_what_they_claim();
    the_checksum_wraps_rather_than_overflowing();
    the_dot_product_bound_keeps_every_frozen_case_exact();
    the_dot_product_golden_is_exact_and_refuses_what_is_not();
    matrix_golden_is_independent_and_exact();
    matrix_inputs_are_deterministic_and_non_vacuous();
    every_frozen_case_fits_the_firmware_buffers();
    byte_footprints_match_the_shapes();

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "test_benchmark_case: all checks passed\n";
    return 0;
}
