// SPDX-License-Identifier: Apache-2.0
//
// Deterministic inputs and the independent host golden (§5.1).
//
// "Independent" is the load-bearing word. Nothing here calls into the core, the
// engines or the firmware: the golden is recomputed on the host from the same
// seed the guest was given, in ordinary C++. A golden obtained by asking the
// model what it produced agrees with the model by construction and tests
// nothing, which is the mistake the Phase 7 harness records having avoided for
// the same reason.
//
// The generator is written out here rather than taken from `<random>`. Two
// reasons, and the second is the one that matters: `std::mt19937` is portable
// but the *distributions* are not — `std::uniform_int_distribution` is
// implementation-defined, so the same seed on another standard library gives
// different data and a recorded seed stops reproducing the run it names.

#ifndef CDC_COMPONENTS_TPU_V3_BENCH_GOLDEN_H
#define CDC_COMPONENTS_TPU_V3_BENCH_GOLDEN_H

#include <cstdint>
#include <string>
#include <vector>

namespace cdc::components::tpu_v3::bench {

/// The edge patterns §5.1 asks for, selectable per run so each is a separate
/// recorded experiment rather than a few elements hidden inside one vector.
enum class input_pattern {
    /// Pseudo-random mixed signs, with a negative, a zero and a positive
    /// forced into the first three positions where the case is long enough.
    mixed,
    /// Every value strictly negative: the golden ReLU output is all zeros, so
    /// a clamp that never fired cannot pass.
    all_negative,
    /// Every value strictly positive: the golden output equals the input, so a
    /// clamp that fired when it should not have cannot pass.
    all_positive,
    /// Every value zero. `max(0, 0)` is the boundary of the comparison itself.
    zero,
    /// `INT32_MIN`, `INT32_MAX`, -1, 0, 1 cycling: the representable
    /// boundaries.
    boundary,
};

const char* to_string(input_pattern pattern) noexcept;
bool parse(const std::string& text, input_pattern& out);

/// The bound on `mixed` magnitudes for MB1.
///
/// ReLU cannot overflow at any input, so this bound exists only to keep the
/// data legible and away from the boundaries the `boundary` pattern covers
/// deliberately.
///
/// **It is not sufficient for MB2**, and an earlier version of this comment
/// claimed it was. The claim was arithmetically wrong: MB2 sums `N` products
/// of two operands, and at `N = 1024` with this bound the exact result reaches
/// `1024 * (2^20)^2 = 2^50`, which is not representable in INT32. MB2 needs
/// its own operand bound — `dot_product_magnitude_bound()` below — and using
/// this one would produce a golden that silently disagrees with any correct
/// machine.
inline constexpr std::int32_t mixed_magnitude_bound = 1 << 20;

/// The largest operand magnitude for which `sum(a[i] * b[i])` over `count`
/// elements is exact in INT32.
///
/// `floor(sqrt(INT32_MAX / count))`, so `count * bound^2 <= INT32_MAX` holds by
/// construction rather than by a comment. A function rather than a constant
/// because the safe bound falls as the case grows: the ten frozen MB2 sizes
/// span three elements to 1024, and one number cannot be tight for both.
///
/// §5.1 requires inputs whose specified result cannot overflow; this is that
/// requirement expressed as code, and `test_benchmark_case.cpp` checks it at
/// every frozen case rather than trusting the derivation.
std::int32_t dot_product_magnitude_bound(std::uint32_t count) noexcept;

/// `count` INT32 values, decided entirely by `seed` and `pattern`.
///
/// The same arguments give the same vector on any host, which is what makes
/// the seed recorded in a result row worth recording.
std::vector<std::int32_t> generate_int32_inputs(std::uint64_t seed,
                                                std::uint32_t count,
                                                input_pattern pattern);

/// MB2 operands: `count` values bounded so the specified dot product is exact.
///
/// A separate generator from `generate_int32_inputs` rather than a parameter on
/// it, because the two benchmarks mean different things by "boundary". ReLU is
/// defined across the whole representable range, so its boundary pattern is
/// `INT32_MIN`/`INT32_MAX`. A dot product is not: §5.1 requires inputs whose
/// specified result cannot overflow, and at `INT32_MAX` the exact sum is not
/// representable at any length. MB2's boundary is therefore the operand bound
/// itself — the largest magnitude at which the benchmark is still defined.
///
/// `bound` is normally `dot_product_magnitude_bound(count)`.
std::vector<std::int32_t> generate_dot_operands(std::uint64_t seed,
                                                std::uint32_t count,
                                                input_pattern pattern,
                                                std::int32_t bound);

/// Signed INT8 operands for MB3/MB4. Mixed and boundary patterns force negative,
/// zero and positive values into every non-trivial tensor; the other patterns
/// retain the same meanings as the INT32 generators.
std::vector<std::int8_t> generate_int8_inputs(std::uint64_t seed,
                                              std::uint32_t count,
                                              input_pattern pattern);

/// `sum(a[i] * b[i])` over equal-length vectors, exact in INT32 (§5.3).
///
/// Computed in 64-bit and then checked, so a caller that supplied operands
/// outside the bound gets a thrown `std::overflow_error` naming the sum rather
/// than a wrapped result that looks like an arithmetic disagreement with the
/// machine.
std::int32_t dot_product_golden(const std::vector<std::int32_t>& a,
                                const std::vector<std::int32_t>& b);

/// Row-major `C[M x N] = A[M x K] * B[K x N]`, INT8 multiply and exact INT32
/// accumulation. Dimensions and operand lengths are checked, and a result that
/// cannot be represented is refused rather than wrapped.
std::vector<std::int32_t> matrix_multiply_int8_golden(
    const std::vector<std::int8_t>& a, const std::vector<std::int8_t>& b,
    std::uint32_t m, std::uint32_t n, std::uint32_t k);

/// `y[i] = max(x[i], 0)`, signed INT32 (§5.2).
std::vector<std::int32_t> relu_golden(const std::vector<std::int32_t>& input);

/// Wrapping unsigned accumulation over a vector.
///
/// Wrapping, and said so: the `boundary` pattern contains `INT32_MAX`, and a
/// signed sum of those would be undefined behaviour. This is a cross-check
/// value the guest can compute the same way, not an arithmetic result — the
/// real comparison is element by element.
std::uint32_t wrapping_checksum(const std::vector<std::int32_t>& values);

/// What a generated vector actually contains, so the harness can refuse a case
/// that would pass without exercising anything.
struct input_profile {
    std::uint32_t negatives = 0;
    std::uint32_t positives = 0;
    std::uint32_t zeros = 0;
};

input_profile profile_of(const std::vector<std::int32_t>& values);
input_profile profile_of(const std::vector<std::int8_t>& values);

/// True when this pattern is expected to make ReLU do observable work — i.e.
/// when at least one element must be clamped and at least one must survive.
///
/// Only `mixed` and `boundary` promise both. `all_positive` deliberately
/// clamps nothing and `all_negative` deliberately keeps nothing; refusing them
/// for being one-sided would throw away the two patterns that isolate the two
/// halves of the operation.
bool pattern_requires_both_signs(input_pattern pattern) noexcept;

} // namespace cdc::components::tpu_v3::bench

#endif // CDC_COMPONENTS_TPU_V3_BENCH_GOLDEN_H
