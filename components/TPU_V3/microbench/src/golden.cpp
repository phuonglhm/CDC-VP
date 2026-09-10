// SPDX-License-Identifier: Apache-2.0

#include "tpu_v3/bench/golden.h"

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace cdc::components::tpu_v3::bench {
namespace {

/// splitmix64. Written out because a recorded seed has to reproduce the run it
/// names on any host; see the header for why `<random>` distributions cannot
/// promise that.
class splitmix64 {
public:
    explicit splitmix64(std::uint64_t seed) noexcept : state_(seed) {}

    std::uint64_t next() noexcept
    {
        state_ += 0x9E3779B97F4A7C15ull;
        std::uint64_t z = state_;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

private:
    std::uint64_t state_;
};

/// A value in `[-bound, bound]`.
///
/// Modulo of a 64-bit draw over a 21-bit range: the bias is below one part in
/// 2^43 and is documented rather than corrected, because rejection sampling
/// would make the number of draws depend on the values drawn and the sequence
/// would stop being a function of the seed alone.
std::int32_t bounded(splitmix64& source, std::int32_t bound) noexcept
{
    const std::uint64_t span = static_cast<std::uint64_t>(bound) * 2u + 1u;
    return static_cast<std::int32_t>(source.next() % span) - bound;
}

} // namespace

const char* to_string(input_pattern pattern) noexcept
{
    switch (pattern) {
    case input_pattern::mixed: return "mixed";
    case input_pattern::all_negative: return "all_negative";
    case input_pattern::all_positive: return "all_positive";
    case input_pattern::zero: return "zero";
    case input_pattern::boundary: return "boundary";
    }
    return "unknown";
}

bool parse(const std::string& text, input_pattern& out)
{
    for (auto candidate :
         {input_pattern::mixed, input_pattern::all_negative,
          input_pattern::all_positive, input_pattern::zero,
          input_pattern::boundary}) {
        if (text == to_string(candidate)) {
            out = candidate;
            return true;
        }
    }
    return false;
}

std::int32_t dot_product_magnitude_bound(std::uint32_t count) noexcept
{
    if (count == 0) {
        return 0;
    }
    const std::uint64_t per_term
        = static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())
        / count;
    // Integer square root by descent from the double estimate, so the result
    // does not depend on floating-point rounding at the boundary.
    auto bound = static_cast<std::int64_t>(std::sqrt(
        static_cast<double>(per_term)));
    while (bound > 0
           && static_cast<std::uint64_t>(bound) * static_cast<std::uint64_t>(bound)
               > per_term) {
        --bound;
    }
    while (static_cast<std::uint64_t>(bound + 1) * static_cast<std::uint64_t>(bound + 1)
           <= per_term) {
        ++bound;
    }
    return static_cast<std::int32_t>(bound);
}

std::vector<std::int32_t> generate_int32_inputs(std::uint64_t seed,
                                                std::uint32_t count,
                                                input_pattern pattern)
{
    std::vector<std::int32_t> values(count, 0);
    splitmix64 source(seed);

    switch (pattern) {
    case input_pattern::mixed:
        for (std::uint32_t i = 0; i < count; ++i) {
            values[i] = bounded(source, mixed_magnitude_bound);
        }
        // Three positions are pinned so that even the three-element case
        // contains something to clamp, something to leave alone and the
        // boundary between them. Without this a short random vector could come
        // out all positive and ReLU would pass without clamping anything --
        // the failure mode the Phase 7 harness already refuses by hand.
        if (count > 0) {
            values[0] = -mixed_magnitude_bound;
        }
        if (count > 1) {
            values[1] = 0;
        }
        if (count > 2) {
            values[2] = mixed_magnitude_bound;
        }
        break;

    case input_pattern::all_negative:
        for (std::uint32_t i = 0; i < count; ++i) {
            values[i] = -1
                - static_cast<std::int32_t>(source.next()
                                            % static_cast<std::uint64_t>(
                                                mixed_magnitude_bound));
        }
        break;

    case input_pattern::all_positive:
        for (std::uint32_t i = 0; i < count; ++i) {
            values[i] = 1
                + static_cast<std::int32_t>(source.next()
                                            % static_cast<std::uint64_t>(
                                                mixed_magnitude_bound));
        }
        break;

    case input_pattern::zero:
        break;

    case input_pattern::boundary: {
        constexpr std::array<std::int32_t, 5> cycle{
            std::numeric_limits<std::int32_t>::min(),
            std::numeric_limits<std::int32_t>::max(), -1, 0, 1};
        for (std::uint32_t i = 0; i < count; ++i) {
            values[i] = cycle[i % cycle.size()];
        }
        break;
    }
    }

    return values;
}

std::vector<std::int32_t> generate_dot_operands(std::uint64_t seed,
                                                std::uint32_t count,
                                                input_pattern pattern,
                                                std::int32_t bound)
{
    std::vector<std::int32_t> values(count, 0);
    if (bound <= 0) {
        return values;
    }
    splitmix64 source(seed);

    switch (pattern) {
    case input_pattern::mixed:
        for (std::uint32_t i = 0; i < count; ++i) {
            values[i] = bounded(source, bound);
        }
        // The same three pinned positions as MB1, for the same reason: a short
        // random vector could otherwise come out with no negative operand and
        // leave sign handling untested.
        if (count > 0) {
            values[0] = -bound;
        }
        if (count > 1) {
            values[1] = 0;
        }
        if (count > 2) {
            values[2] = bound;
        }
        break;

    case input_pattern::all_negative:
        for (std::uint32_t i = 0; i < count; ++i) {
            values[i] = -1
                - static_cast<std::int32_t>(source.next()
                                            % static_cast<std::uint64_t>(bound));
        }
        break;

    case input_pattern::all_positive:
        for (std::uint32_t i = 0; i < count; ++i) {
            values[i] = 1
                + static_cast<std::int32_t>(source.next()
                                            % static_cast<std::uint64_t>(bound));
        }
        break;

    case input_pattern::zero:
        break;

    case input_pattern::boundary: {
        // The operand bound, not the representable range: see the header.
        const std::array<std::int32_t, 5> cycle{-bound, bound, -1, 0, 1};
        for (std::uint32_t i = 0; i < count; ++i) {
            values[i] = cycle[i % cycle.size()];
        }
        break;
    }
    }
    return values;
}

std::vector<std::int8_t> generate_int8_inputs(std::uint64_t seed,
                                              std::uint32_t count,
                                              input_pattern pattern)
{
    std::vector<std::int8_t> values(count, 0);
    splitmix64 source(seed);

    switch (pattern) {
    case input_pattern::mixed:
        for (std::uint32_t i = 0; i < count; ++i) {
            values[i] = static_cast<std::int8_t>(
                static_cast<std::int32_t>(source.next() % 255u) - 127);
        }
        if (count > 0) values[0] = -127;
        if (count > 1) values[1] = 0;
        if (count > 2) values[2] = 127;
        break;
    case input_pattern::all_negative:
        for (std::uint32_t i = 0; i < count; ++i) {
            values[i] = static_cast<std::int8_t>(
                -1 - static_cast<std::int32_t>(source.next() % 127u));
        }
        break;
    case input_pattern::all_positive:
        for (std::uint32_t i = 0; i < count; ++i) {
            values[i] = static_cast<std::int8_t>(
                1 + static_cast<std::int32_t>(source.next() % 127u));
        }
        break;
    case input_pattern::zero:
        break;
    case input_pattern::boundary: {
        constexpr std::array<std::int8_t, 5> cycle{
            std::numeric_limits<std::int8_t>::min(),
            std::numeric_limits<std::int8_t>::max(), -1, 0, 1};
        for (std::uint32_t i = 0; i < count; ++i) {
            values[i] = cycle[i % cycle.size()];
        }
        break;
    }
    }
    return values;
}

std::int32_t dot_product_golden(const std::vector<std::int32_t>& a,
                                const std::vector<std::int32_t>& b)
{
    if (a.size() != b.size()) {
        throw std::invalid_argument(
            "dot_product_golden: operand lengths differ ("
            + std::to_string(a.size()) + " and " + std::to_string(b.size())
            + ")");
    }
    std::int64_t sum = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        sum += static_cast<std::int64_t>(a[i]) * static_cast<std::int64_t>(b[i]);
    }
    if (sum < std::numeric_limits<std::int32_t>::min()
        || sum > std::numeric_limits<std::int32_t>::max()) {
        // Thrown rather than wrapped. A wrapped golden would disagree with a
        // correct machine and present as an arithmetic defect in the model,
        // which is the most expensive way to discover that the operands were
        // out of contract.
        throw std::overflow_error(
            "dot_product_golden: the exact sum " + std::to_string(sum)
            + " is not representable in INT32; the operands exceed the bound "
              "section 5.1 requires");
    }
    return static_cast<std::int32_t>(sum);
}

std::vector<std::int32_t> matrix_multiply_int8_golden(
    const std::vector<std::int8_t>& a, const std::vector<std::int8_t>& b,
    std::uint32_t m, std::uint32_t n, std::uint32_t k)
{
    if (m == 0 || n == 0 || k == 0) {
        throw std::invalid_argument(
            "matrix_multiply_int8_golden: dimensions must be non-zero");
    }
    const std::uint64_t a_elements = static_cast<std::uint64_t>(m) * k;
    const std::uint64_t b_elements = static_cast<std::uint64_t>(k) * n;
    if (a.size() != a_elements || b.size() != b_elements) {
        throw std::invalid_argument(
            "matrix_multiply_int8_golden: operand lengths do not match M/N/K");
    }

    std::vector<std::int32_t> output(static_cast<std::size_t>(m) * n, 0);
    for (std::uint32_t row = 0; row < m; ++row) {
        for (std::uint32_t column = 0; column < n; ++column) {
            std::int64_t sum = 0;
            for (std::uint32_t inner = 0; inner < k; ++inner) {
                sum += static_cast<std::int64_t>(a[row * k + inner])
                    * static_cast<std::int64_t>(b[inner * n + column]);
            }
            if (sum < std::numeric_limits<std::int32_t>::min()
                || sum > std::numeric_limits<std::int32_t>::max()) {
                throw std::overflow_error(
                    "matrix_multiply_int8_golden: an exact output is not "
                    "representable in INT32");
            }
            output[row * n + column] = static_cast<std::int32_t>(sum);
        }
    }
    return output;
}

std::vector<std::int32_t> relu_golden(const std::vector<std::int32_t>& input)
{
    std::vector<std::int32_t> output(input.size(), 0);
    for (std::size_t i = 0; i < input.size(); ++i) {
        output[i] = input[i] > 0 ? input[i] : 0;
    }
    return output;
}

std::uint32_t wrapping_checksum(const std::vector<std::int32_t>& values)
{
    std::uint32_t sum = 0;
    for (std::int32_t value : values) {
        sum += static_cast<std::uint32_t>(value);
    }
    return sum;
}

input_profile profile_of(const std::vector<std::int32_t>& values)
{
    input_profile profile;
    for (std::int32_t value : values) {
        if (value < 0) {
            ++profile.negatives;
        } else if (value > 0) {
            ++profile.positives;
        } else {
            ++profile.zeros;
        }
    }
    return profile;
}

input_profile profile_of(const std::vector<std::int8_t>& values)
{
    input_profile profile;
    for (std::int8_t value : values) {
        if (value < 0) {
            ++profile.negatives;
        } else if (value > 0) {
            ++profile.positives;
        } else {
            ++profile.zeros;
        }
    }
    return profile;
}

bool pattern_requires_both_signs(input_pattern pattern) noexcept
{
    return pattern == input_pattern::mixed
        || pattern == input_pattern::boundary;
}

} // namespace cdc::components::tpu_v3::bench
