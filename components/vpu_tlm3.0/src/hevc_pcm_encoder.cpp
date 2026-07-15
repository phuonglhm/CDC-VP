#include "hevc/hevc_pcm_encoder.hpp"

#include "hevc/bit_writer.hpp"
#include "hevc/cabac.hpp"
#include "hevc/transform.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace hevc {
namespace {

constexpr unsigned kCtuSize = 32;
constexpr unsigned kPcmCuSize = 32;
constexpr std::uint8_t kSplitCuFlagInitIntraContext0 = 139;
constexpr std::uint8_t kPrevIntraLumaPredInitIntra = 184;
constexpr std::uint8_t kIntraChromaPredInitIntra = 63;
constexpr std::uint8_t kCbfChromaDepth0InitIntra = 94;
constexpr std::uint8_t kCbfLumaDepth0InitIntra = 141;
constexpr std::uint8_t kTransformSplit32InitIntra = 153;
constexpr std::uint8_t kCbfChromaDepth1InitIntra = 138;
constexpr std::uint8_t kCbfLumaDepth1InitIntra = 111;
constexpr std::uint8_t kLastLuma32InitIntra = 111;
constexpr std::uint8_t kLastChromaInitIntra = 108;
constexpr std::uint8_t kOneLumaSet0C1InitIntra = 92;
constexpr std::uint8_t kAbsLumaSet0InitIntra = 138;
constexpr std::uint8_t kOneChromaSet4C1InitIntra = 179;
constexpr std::uint8_t kAbsChromaSet4InitIntra = 152;

constexpr std::array<std::uint8_t, 15> kLastLumaInitIntra{{
    110, 110, 124, 125, 140, 153, 125, 127, 140, 109, 111, 143, 127, 111, 79,
}};
constexpr std::array<std::uint8_t, 15> kLastChromaFullInitIntra{{
    108, 123, 63, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154, 154,
}};
constexpr std::array<std::uint8_t, 2> kSigGroupLumaInitIntra{{91, 171}};
constexpr std::array<std::uint8_t, 2> kSigGroupChromaInitIntra{{134, 141}};
constexpr std::array<std::uint8_t, 28> kSigLumaInitIntra{{
    111, 111, 125, 110, 110, 94, 124, 108, 124,
    107, 125, 141, 179, 153, 125, 107, 125, 141, 179, 153, 125,
    107, 125, 141, 179, 153, 125, 141,
}};
constexpr std::array<std::uint8_t, 16> kSigChromaInitIntra{{
    140, 139, 182, 182, 152, 136, 152, 136, 153,
    136, 139, 111, 136, 139, 111, 111,
}};
constexpr std::array<std::uint8_t, 16> kOneLumaInitIntra{{
    140, 92, 137, 138, 140, 152, 138, 139,
    153, 74, 149, 92, 139, 107, 122, 152,
}};
constexpr std::array<std::uint8_t, 4> kAbsLumaInitIntra{{138, 153, 136, 167}};
constexpr std::array<std::uint8_t, 8> kOneChromaInitIntra{{
    140, 179, 166, 182, 140, 227, 122, 197,
}};
constexpr std::array<std::uint8_t, 2> kAbsChromaInitIntra{{152, 152}};

constexpr std::array<unsigned, 32> kLastGroupIndex{{
    0,1,2,3,4,4,5,5,6,6,6,6,7,7,7,7,
    8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,
}};
constexpr std::array<unsigned, 10> kLastGroupMinimum{{
    0,1,2,3,4,6,8,12,16,24,
}};

constexpr std::array<int, 6> kQuantScales{{26214, 23302, 20560, 18396, 16384, 14564}};
constexpr std::array<int, 6> kInverseQuantScales{{40, 45, 51, 57, 64, 72}};
constexpr std::array<int, 52> kChromaQp420{{
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,
    16,17,18,19,20,21,22,23,24,25,26,27,28,29,29,30,
    31,32,33,33,34,34,35,35,36,36,37,37,38,39,40,41,
    42,43,44,45,
}};

unsigned round_up(unsigned value, unsigned multiple) {
    return (value + multiple - 1) / multiple * multiple;
}

unsigned ceil_log2(unsigned value) {
    if (value <= 1) {
        return 0;
    }
    return std::bit_width(value - 1U);
}

int rounded_shift(std::int64_t value, unsigned shift) {
    return static_cast<int>((value + (std::int64_t{1} << (shift - 1))) >> shift);
}

int forward_dc(const Plane& plane, unsigned x0, unsigned y0, unsigned block) {
    const unsigned log2_size = std::countr_zero(block);
    const unsigned shift_first = log2_size - 1;
    const unsigned shift_second = log2_size + 6;
    std::int64_t sum_second = 0;
    for (unsigned y = 0; y < block; ++y) {
        int row_sum = 0;
        for (unsigned x = 0; x < block; ++x) {
            row_sum += static_cast<int>(plane.at(x0 + x, y0 + y)) - 128;
        }
        sum_second += rounded_shift(static_cast<std::int64_t>(row_sum) * 64,
                                    shift_first);
    }
    return rounded_shift(sum_second * 64, shift_second);
}

int quantize_dc(int coefficient, unsigned block, int qp) {
    const int transform_shift = 15 - 8 - static_cast<int>(std::countr_zero(block));
    const int qbits = 14 + qp / 6 + transform_shift;
    const std::int64_t add = std::int64_t{171} << (qbits - 9);
    const auto magnitude = static_cast<std::int64_t>(std::abs(coefficient));
    const int level = static_cast<int>(
        (magnitude * kQuantScales[qp % 6] + add) >> qbits);
    return coefficient < 0 ? -level : level;
}

int inverse_dc_sample(int level, unsigned block, int qp) {
    if (level == 0) return 128;
    const int transform_shift = 15 - 8 - static_cast<int>(std::countr_zero(block));
    const int right_shift = 6 - (transform_shift + qp / 6);
    std::int64_t coefficient;
    if (right_shift > 0) {
        coefficient = (static_cast<std::int64_t>(level) *
                       kInverseQuantScales[qp % 6] +
                       (std::int64_t{1} << (right_shift - 1))) >> right_shift;
    } else {
        coefficient = static_cast<std::int64_t>(level) *
                      kInverseQuantScales[qp % 6] << (-right_shift);
    }
    coefficient = std::clamp<std::int64_t>(coefficient, -32768, 32767);
    const int first = rounded_shift(coefficient * 64, 7);
    const int residual = rounded_shift(static_cast<std::int64_t>(first) * 64, 12);
    return std::clamp(128 + residual, 0, 255);
}

void encode_bypass_bits(CabacEncoder& cabac, std::uint32_t value, unsigned count) {
    for (unsigned i = count; i > 0; --i) {
        cabac.encode_bypass((value >> (i - 1)) & 1U);
    }
}

void encode_coeff_remaining(CabacEncoder& cabac, unsigned symbol) {
    constexpr unsigned reduction = 3;
    unsigned length = 0;
    unsigned code_number = symbol;
    if (code_number < reduction) {
        length = code_number;
        encode_bypass_bits(cabac, (1U << (length + 1)) - 2, length + 1);
        return;
    }
    code_number -= reduction;
    while (code_number >= (1U << length)) {
        code_number -= 1U << length;
        ++length;
    }
    encode_bypass_bits(cabac,
        (1U << (reduction + length + 1)) - 2,
        reduction + length + 1);
    encode_bypass_bits(cabac, code_number, length);
}

void encode_coeff_remaining_rice(CabacEncoder& cabac, unsigned symbol,
                                 unsigned rice_parameter) {
    constexpr unsigned reduction = 3;
    if (symbol < (reduction << rice_parameter)) {
        const unsigned prefix_ones = symbol >> rice_parameter;
        for (unsigned i = 0; i < prefix_ones; ++i) cabac.encode_bypass(1);
        cabac.encode_bypass(0);
        if (rice_parameter != 0) {
            encode_bypass_bits(cabac,
                               symbol & ((1U << rice_parameter) - 1U),
                               rice_parameter);
        }
        return;
    }

    unsigned code_number = symbol - (reduction << rice_parameter);
    unsigned suffix_length = rice_parameter;
    while (code_number >= (1U << suffix_length)) {
        code_number -= 1U << suffix_length;
        ++suffix_length;
    }
    const unsigned prefix_ones = reduction + suffix_length - rice_parameter;
    for (unsigned i = 0; i < prefix_ones; ++i) cabac.encode_bypass(1);
    cabac.encode_bypass(0);
    encode_bypass_bits(cabac, code_number, suffix_length);
}

template <std::size_t N>
std::vector<CabacContext> make_contexts(const std::array<std::uint8_t, N>& init,
                                        int qp) {
    std::vector<CabacContext> contexts;
    contexts.reserve(N);
    for (const auto value : init) {
        contexts.push_back(CabacContext::from_init_value(value, qp));
    }
    return contexts;
}

struct CoefficientContexts {
    explicit CoefficientContexts(bool is_luma, int qp) : luma(is_luma) {
        if (luma) {
            last_x = make_contexts(kLastLumaInitIntra, qp);
            last_y = make_contexts(kLastLumaInitIntra, qp);
            significant_group = make_contexts(kSigGroupLumaInitIntra, qp);
            significant = make_contexts(kSigLumaInitIntra, qp);
            greater_one = make_contexts(kOneLumaInitIntra, qp);
            greater_two = make_contexts(kAbsLumaInitIntra, qp);
        } else {
            last_x = make_contexts(kLastChromaFullInitIntra, qp);
            last_y = make_contexts(kLastChromaFullInitIntra, qp);
            significant_group = make_contexts(kSigGroupChromaInitIntra, qp);
            significant = make_contexts(kSigChromaInitIntra, qp);
            greater_one = make_contexts(kOneChromaInitIntra, qp);
            greater_two = make_contexts(kAbsChromaInitIntra, qp);
        }
    }

    bool luma = false;
    std::vector<CabacContext> last_x;
    std::vector<CabacContext> last_y;
    std::vector<CabacContext> significant_group;
    std::vector<CabacContext> significant;
    std::vector<CabacContext> greater_one;
    std::vector<CabacContext> greater_two;
};

std::vector<unsigned> diagonal_scan(unsigned width, unsigned height) {
    std::vector<unsigned> scan;
    scan.reserve(static_cast<std::size_t>(width) * height);
    for (unsigned rank = 0; rank < width + height - 1; ++rank) {
        const unsigned x_begin = rank >= height ? rank - (height - 1) : 0;
        const unsigned x_end = std::min(rank, width - 1);
        for (unsigned x = x_begin; x <= x_end; ++x) {
            const unsigned y = rank - x;
            scan.push_back(y * width + x);
        }
    }
    return scan;
}

std::vector<unsigned> grouped_diagonal_scan(unsigned block_size) {
    constexpr unsigned group_size = 4;
    const unsigned groups = block_size / group_size;
    const auto group_scan = diagonal_scan(groups, groups);
    const auto within_group = diagonal_scan(group_size, group_size);
    std::vector<unsigned> scan;
    scan.reserve(static_cast<std::size_t>(block_size) * block_size);
    for (const unsigned group_raster : group_scan) {
        const unsigned gx = group_raster % groups;
        const unsigned gy = group_raster / groups;
        for (const unsigned local_raster : within_group) {
            const unsigned x = gx * group_size + local_raster % group_size;
            const unsigned y = gy * group_size + local_raster / group_size;
            scan.push_back(y * block_size + x);
        }
    }
    return scan;
}

unsigned significant_group_context(const std::vector<unsigned>& group_flags,
                                   unsigned gx, unsigned gy,
                                   unsigned groups) {
    const bool right = gx + 1 < groups &&
                       group_flags[gy * groups + gx + 1] != 0;
    const bool below = gy + 1 < groups &&
                       group_flags[(gy + 1) * groups + gx] != 0;
    return right || below;
}

unsigned significant_pattern(const std::vector<unsigned>& group_flags,
                             unsigned gx, unsigned gy, unsigned groups) {
    if (groups <= 1) return 0;
    const unsigned right = gx + 1 < groups &&
                           group_flags[gy * groups + gx + 1] != 0;
    const unsigned below = gy + 1 < groups &&
                           group_flags[(gy + 1) * groups + gx] != 0;
    return right + (below << 1U);
}

unsigned significant_context(unsigned pattern, unsigned raster_position,
                             unsigned block_size, bool luma) {
    const unsigned x = raster_position % block_size;
    const unsigned y = raster_position / block_size;
    if (x + y == 0) return 0;

    unsigned count = 0;
    const unsigned local_x = x & 3U;
    const unsigned local_y = y & 3U;
    if (pattern == 0) {
        const unsigned total = local_x + local_y;
        count = total >= 3 ? 0 : (total >= 1 ? 1 : 2);
    } else if (pattern == 1) {
        count = local_y >= 2 ? 0 : (local_y >= 1 ? 1 : 2);
    } else if (pattern == 2) {
        count = local_x >= 2 ? 0 : (local_x >= 1 ? 1 : 2);
    } else {
        count = 2;
    }
    const bool not_first_group = (x >> 2U) + (y >> 2U) > 0;
    const unsigned first_context = luma ? 21 : 12;
    return first_context + (luma && not_first_group ? 3U : 0U) + count;
}

void encode_last_position(CabacEncoder& cabac, unsigned x, unsigned y,
                          unsigned block_size, CoefficientContexts& contexts) {
    const unsigned converted_size = std::countr_zero(block_size) - 2;
    const unsigned offset = contexts.luma
        ? converted_size * 3 + ((converted_size + 1) >> 2U) : 0;
    const unsigned shift = contexts.luma
        ? ((converted_size + 3) >> 2U) : converted_size;

    const auto encode_prefix = [&](unsigned position,
                                   std::vector<CabacContext>& coordinate) {
        const unsigned group = kLastGroupIndex[position];
        unsigned index = 0;
        for (; index < group; ++index) {
            cabac.encode_bin(1, coordinate[offset + (index >> shift)]);
        }
        if (group < kLastGroupIndex[block_size - 1]) {
            cabac.encode_bin(0, coordinate[offset + (index >> shift)]);
        }
        return group;
    };
    const auto encode_suffix = [&](unsigned position, unsigned group) {
        if (group > 3) {
            const unsigned suffix_bits = (group - 2) >> 1U;
            encode_bypass_bits(cabac, position - kLastGroupMinimum[group],
                               suffix_bits);
        }
    };
    // HEVC sends both context-coded prefixes before either bypass suffix.
    const unsigned group_x = encode_prefix(x, contexts.last_x);
    const unsigned group_y = encode_prefix(y, contexts.last_y);
    encode_suffix(x, group_x);
    encode_suffix(y, group_y);
}

void encode_coefficients(CabacEncoder& cabac, const QuantizedBlock& block,
                         CoefficientContexts& contexts) {
    if (!block.has_nonzero()) return;
    const unsigned size = block.size;
    const unsigned groups = size / 4;
    const auto scan = grouped_diagonal_scan(size);
    const auto scan_groups = diagonal_scan(groups, groups);
    std::vector<unsigned> group_flags(static_cast<std::size_t>(groups) * groups, 0);

    int last_scan_position = -1;
    unsigned last_raster = 0;
    for (unsigned scan_position = 0; scan_position < scan.size(); ++scan_position) {
        const unsigned raster = scan[scan_position];
        if (block.coefficients[raster] != 0) {
            last_scan_position = static_cast<int>(scan_position);
            last_raster = raster;
            const unsigned x = raster % size;
            const unsigned y = raster / size;
            group_flags[(y >> 2U) * groups + (x >> 2U)] = 1;
        }
    }
    encode_last_position(cabac, last_raster % size, last_raster / size,
                         size, contexts);

    const int last_subset = last_scan_position >> 4;
    int scan_position = last_scan_position;
    unsigned c1 = 1;
    for (int subset = last_subset; subset >= 0; --subset) {
        const int subset_start = subset << 4;
        std::vector<unsigned> magnitudes;
        std::vector<unsigned> signs;
        int last_nonzero_in_group = -1;
        int first_nonzero_in_group = 16;

        if (scan_position == last_scan_position) {
            const int level = block.coefficients[last_raster];
            magnitudes.push_back(static_cast<unsigned>(std::abs(level)));
            signs.push_back(level < 0);
            last_nonzero_in_group = scan_position;
            first_nonzero_in_group = scan_position;
            --scan_position;
        }

        const unsigned group_raster = scan_groups[static_cast<unsigned>(subset)];
        const unsigned gx = group_raster % groups;
        const unsigned gy = group_raster / groups;
        if (subset == last_subset || subset == 0) {
            group_flags[group_raster] = 1;
        } else {
            const unsigned context = significant_group_context(
                group_flags, gx, gy, groups);
            cabac.encode_bin(group_flags[group_raster],
                             contexts.significant_group[context]);
        }

        if (group_flags[group_raster]) {
            const unsigned pattern = significant_pattern(
                group_flags, gx, gy, groups);
            for (; scan_position >= subset_start; --scan_position) {
                const unsigned raster = scan[static_cast<unsigned>(scan_position)];
                const bool significant = block.coefficients[raster] != 0;
                if (scan_position > subset_start || subset == 0 ||
                    !magnitudes.empty()) {
                    const unsigned context = significant_context(
                        pattern, raster, size, contexts.luma);
                    cabac.encode_bin(significant, contexts.significant[context]);
                }
                if (significant) {
                    const int level = block.coefficients[raster];
                    magnitudes.push_back(static_cast<unsigned>(std::abs(level)));
                    signs.push_back(level < 0);
                    if (last_nonzero_in_group == -1) {
                        last_nonzero_in_group = scan_position;
                    }
                    first_nonzero_in_group = scan_position;
                }
            }
        } else {
            scan_position = subset_start - 1;
        }

        if (magnitudes.empty()) continue;
        (void)last_nonzero_in_group;
        (void)first_nonzero_in_group; // sign-data hiding is disabled in the PPS

        const unsigned context_set =
            (contexts.luma && subset > 0 ? 2U : 0U) + (c1 == 0 ? 1U : 0U);
        c1 = 1;
        const unsigned c1_count = std::min<unsigned>(magnitudes.size(), 8);
        int first_c2 = -1;
        bool escape_present = false;
        for (unsigned index = 0; index < c1_count; ++index) {
            const bool greater = magnitudes[index] > 1;
            cabac.encode_bin(greater,
                contexts.greater_one[context_set * 4 + c1]);
            if (greater) {
                c1 = 0;
                if (first_c2 < 0) first_c2 = static_cast<int>(index);
                else escape_present = true;
            } else if (c1 > 0 && c1 < 3) {
                ++c1;
            }
        }
        if (c1 == 0 && first_c2 >= 0) {
            const bool greater_two =
                magnitudes[static_cast<unsigned>(first_c2)] > 2;
            cabac.encode_bin(greater_two, contexts.greater_two[context_set]);
            escape_present = escape_present || greater_two;
        }
        escape_present = escape_present || magnitudes.size() > 8;

        for (const unsigned sign : signs) cabac.encode_bypass(sign);
        if (escape_present) {
            bool first_greater_two = true;
            unsigned rice = 0;
            for (unsigned index = 0; index < magnitudes.size(); ++index) {
                const unsigned base_level = index < 8
                    ? 2U + static_cast<unsigned>(first_greater_two) : 1U;
                if (magnitudes[index] >= base_level) {
                    encode_coeff_remaining_rice(
                        cabac, magnitudes[index] - base_level, rice);
                    if (magnitudes[index] > (3U << rice)) {
                        rice = std::min(rice + 1, 4U);
                    }
                }
                if (magnitudes[index] >= 2) first_greater_two = false;
            }
        }
    }
}

enum class IntraPredictionMode : unsigned {
    Dc = 1,
    Horizontal = 10,
    Vertical = 26,
};

int arithmetic_half(int value) {
    return value >= 0 ? value / 2 : -((-value + 1) / 2);
}

std::vector<std::uint8_t> make_intra_prediction(
    const Plane& reconstructed, unsigned x0, unsigned y0, unsigned block_size,
    bool top_available, bool left_available, bool filter_luma_boundary,
    IntraPredictionMode mode) {
    std::vector<std::uint8_t> top(block_size, 128);
    std::vector<std::uint8_t> left(block_size, 128);
    if (top_available) {
        for (unsigned x = 0; x < block_size; ++x) {
            top[x] = reconstructed.at(x0 + x, y0 - 1);
        }
    }
    if (left_available) {
        for (unsigned y = 0; y < block_size; ++y) {
            left[y] = reconstructed.at(x0 - 1, y0 + y);
        }
    }
    if (!top_available && left_available) {
        std::fill(top.begin(), top.end(), left.front());
    } else if (!left_available && top_available) {
        std::fill(left.begin(), left.end(), top.front());
    }

    const std::uint8_t top_left = top_available && left_available
        ? reconstructed.at(x0 - 1, y0 - 1)
        : (top_available ? top.front() : left.front());

    if (mode == IntraPredictionMode::Vertical) {
        std::vector<std::uint8_t> prediction(
            static_cast<std::size_t>(block_size) * block_size);
        for (unsigned y = 0; y < block_size; ++y) {
            std::copy(top.begin(), top.end(),
                      prediction.begin() +
                          static_cast<std::ptrdiff_t>(y) * block_size);
        }
        if (filter_luma_boundary && block_size < 32) {
            for (unsigned y = 0; y < block_size; ++y) {
                const int value = static_cast<int>(top.front()) +
                    arithmetic_half(static_cast<int>(left[y]) - top_left);
                prediction[static_cast<std::size_t>(y) * block_size] =
                    static_cast<std::uint8_t>(std::clamp(value, 0, 255));
            }
        }
        return prediction;
    }
    if (mode == IntraPredictionMode::Horizontal) {
        std::vector<std::uint8_t> prediction(
            static_cast<std::size_t>(block_size) * block_size);
        for (unsigned y = 0; y < block_size; ++y) {
            std::fill_n(prediction.begin() +
                            static_cast<std::ptrdiff_t>(y) * block_size,
                        block_size, left[y]);
        }
        if (filter_luma_boundary && block_size < 32) {
            for (unsigned x = 0; x < block_size; ++x) {
                const int value = static_cast<int>(left.front()) +
                    arithmetic_half(static_cast<int>(top[x]) - top_left);
                prediction[x] =
                    static_cast<std::uint8_t>(std::clamp(value, 0, 255));
            }
        }
        return prediction;
    }

    unsigned sum = 0;
    for (unsigned i = 0; i < block_size; ++i) {
        sum += top[i] + left[i];
    }
    const unsigned dc =
        (sum + block_size) >> (std::countr_zero(block_size) + 1);
    std::vector<std::uint8_t> prediction(
        static_cast<std::size_t>(block_size) * block_size,
        static_cast<std::uint8_t>(dc));

    // HEVC applies the DC boundary filter to luma blocks smaller than 32x32.
    if (filter_luma_boundary && block_size < 32) {
        prediction[0] = static_cast<std::uint8_t>(
            (left[0] + 2 * dc + top[0] + 2) >> 2);
        for (unsigned x = 1; x < block_size; ++x) {
            prediction[x] = static_cast<std::uint8_t>(
                (top[x] + 3 * dc + 2) >> 2);
        }
        for (unsigned y = 1; y < block_size; ++y) {
            prediction[static_cast<std::size_t>(y) * block_size] =
                static_cast<std::uint8_t>((left[y] + 3 * dc + 2) >> 2);
        }
    }
    return prediction;
}

struct SplitTq16Leaf {
    QuantizedBlock luma;
    QuantizedBlock cb;
    QuantizedBlock cr;
};

struct SplitTq16Result {
    std::array<SplitTq16Leaf, 4> leaves;
    Plane luma_reconstruction{
        kCtuSize, kCtuSize,
        std::vector<std::uint8_t>(kCtuSize * kCtuSize, 128)};
    Plane cb_reconstruction{
        kCtuSize / 2, kCtuSize / 2,
        std::vector<std::uint8_t>(kCtuSize * kCtuSize / 4, 128)};
    Plane cr_reconstruction{
        kCtuSize / 2, kCtuSize / 2,
        std::vector<std::uint8_t>(kCtuSize * kCtuSize / 4, 128)};
};

struct FullTq32Result {
    QuantizedBlock luma;
    QuantizedBlock cb;
    QuantizedBlock cr;
    Plane luma_reconstruction{
        kCtuSize, kCtuSize,
        std::vector<std::uint8_t>(kCtuSize * kCtuSize, 128)};
    Plane cb_reconstruction{
        kCtuSize / 2, kCtuSize / 2,
        std::vector<std::uint8_t>(kCtuSize * kCtuSize / 4, 128)};
    Plane cr_reconstruction{
        kCtuSize / 2, kCtuSize / 2,
        std::vector<std::uint8_t>(kCtuSize * kCtuSize / 4, 128)};
};

SplitTq16Result make_split_tq16_result(
    const Yuv420Frame& source, unsigned x0, unsigned y0, int qp,
    IntraPredictionMode luma_mode = IntraPredictionMode::Dc) {
    SplitTq16Result result;
    const int chroma_qp = kChromaQp420[static_cast<unsigned>(qp)];
    constexpr unsigned luma_tu = kCtuSize / 2;
    constexpr unsigned chroma_tu = luma_tu / 2;
    for (unsigned index = 0; index < result.leaves.size(); ++index) {
        const unsigned dx = (index & 1U) * luma_tu;
        const unsigned dy = (index >> 1U) * luma_tu;
        const bool top_available = dy != 0;
        const bool left_available = dx != 0;
        const auto luma_prediction = make_intra_prediction(
            result.luma_reconstruction, dx, dy, luma_tu,
            top_available, left_available, true, luma_mode);
        const auto cb_prediction = make_intra_prediction(
            result.cb_reconstruction, dx / 2, dy / 2, chroma_tu,
            top_available, left_available, false, IntraPredictionMode::Dc);
        const auto cr_prediction = make_intra_prediction(
            result.cr_reconstruction, dx / 2, dy / 2, chroma_tu,
            top_available, left_available, false, IntraPredictionMode::Dc);

        auto& leaf = result.leaves[index];
        leaf.luma = transform_quantize_predicted_block(
            source.y, x0 + dx, y0 + dy, luma_tu, qp, luma_prediction);
        leaf.cb = transform_quantize_predicted_block(
            source.cb, x0 / 2 + dx / 2, y0 / 2 + dy / 2,
            chroma_tu, chroma_qp, cb_prediction);
        leaf.cr = transform_quantize_predicted_block(
            source.cr, x0 / 2 + dx / 2, y0 / 2 + dy / 2,
            chroma_tu, chroma_qp, cr_prediction);
        inverse_reconstruct_predicted_block(
            leaf.luma, qp, result.luma_reconstruction,
            dx, dy, luma_prediction);
        inverse_reconstruct_predicted_block(
            leaf.cb, chroma_qp, result.cb_reconstruction,
            dx / 2, dy / 2, cb_prediction);
        inverse_reconstruct_predicted_block(
            leaf.cr, chroma_qp, result.cr_reconstruction,
            dx / 2, dy / 2, cr_prediction);
    }
    return result;
}

FullTq32Result make_full_tq32_result(
    const Yuv420Frame& source, unsigned x0, unsigned y0, int qp) {
    FullTq32Result result;
    const int chroma_qp = kChromaQp420[static_cast<unsigned>(qp)];
    result.luma = transform_quantize_block(
        source.y, x0, y0, kCtuSize, qp);
    result.cb = transform_quantize_block(
        source.cb, x0 / 2, y0 / 2, kCtuSize / 2, chroma_qp);
    result.cr = transform_quantize_block(
        source.cr, x0 / 2, y0 / 2, kCtuSize / 2, chroma_qp);
    inverse_reconstruct_block(
        result.luma, qp, result.luma_reconstruction, 0, 0);
    inverse_reconstruct_block(
        result.cb, chroma_qp, result.cb_reconstruction, 0, 0);
    inverse_reconstruct_block(
        result.cr, chroma_qp, result.cr_reconstruction, 0, 0);
    return result;
}

std::uint64_t plane_distortion(
    const Plane& source, unsigned x0, unsigned y0,
    const Plane& reconstruction) {
    std::uint64_t distortion = 0;
    for (unsigned y = 0; y < reconstruction.height; ++y) {
        for (unsigned x = 0; x < reconstruction.width; ++x) {
            const int difference =
                static_cast<int>(source.at(x0 + x, y0 + y)) -
                static_cast<int>(reconstruction.at(x, y));
            distortion += static_cast<std::uint64_t>(difference * difference);
        }
    }
    return distortion;
}

std::uint64_t full_tq32_distortion(
    const Yuv420Frame& source, unsigned x0, unsigned y0,
    const FullTq32Result& result) {
    return plane_distortion(source.y, x0, y0, result.luma_reconstruction) +
           plane_distortion(source.cb, x0 / 2, y0 / 2,
                            result.cb_reconstruction) +
           plane_distortion(source.cr, x0 / 2, y0 / 2,
                            result.cr_reconstruction);
}

std::uint64_t split_tq16_distortion(
    const Yuv420Frame& source, unsigned x0, unsigned y0,
    const SplitTq16Result& result) {
    return plane_distortion(source.y, x0, y0, result.luma_reconstruction) +
           plane_distortion(source.cb, x0 / 2, y0 / 2,
                            result.cb_reconstruction) +
           plane_distortion(source.cr, x0 / 2, y0 / 2,
                            result.cr_reconstruction);
}

bool choose_split_tq16(
    const Yuv420Frame& source, unsigned x0, unsigned y0,
    const FullTq32Result& full, const SplitTq16Result& split) {
    return split_tq16_distortion(source, x0, y0, split) <
           full_tq32_distortion(source, x0, y0, full);
}

struct DirectionalDecision {
    FullTq32Result full;
    SplitTq16Result dc;
    SplitTq16Result horizontal;
    SplitTq16Result vertical;
    bool use_split = false;
    IntraPredictionMode mode = IntraPredictionMode::Dc;
};

DirectionalDecision make_directional_decision(
    const Yuv420Frame& source, unsigned x0, unsigned y0, int qp) {
    DirectionalDecision decision{
        make_full_tq32_result(source, x0, y0, qp),
        make_split_tq16_result(
            source, x0, y0, qp, IntraPredictionMode::Dc),
        make_split_tq16_result(
            source, x0, y0, qp, IntraPredictionMode::Horizontal),
        make_split_tq16_result(
            source, x0, y0, qp, IntraPredictionMode::Vertical),
    };
    std::uint64_t best = full_tq32_distortion(
        source, x0, y0, decision.full);
    const auto consider = [&](const SplitTq16Result& candidate,
                              IntraPredictionMode mode) {
        const auto distortion = split_tq16_distortion(
            source, x0, y0, candidate);
        if (distortion < best) {
            best = distortion;
            decision.use_split = true;
            decision.mode = mode;
        }
    };
    consider(decision.dc, IntraPredictionMode::Dc);
    consider(decision.horizontal, IntraPredictionMode::Horizontal);
    consider(decision.vertical, IntraPredictionMode::Vertical);
    return decision;
}

const SplitTq16Result& selected_split(const DirectionalDecision& decision) {
    if (decision.mode == IntraPredictionMode::Horizontal) {
        return decision.horizontal;
    }
    if (decision.mode == IntraPredictionMode::Vertical) {
        return decision.vertical;
    }
    return decision.dc;
}

void encode_luma_intra_mode(CabacEncoder& cabac, CabacContext& context,
                            IntraPredictionMode mode) {
    if (mode == IntraPredictionMode::Horizontal) {
        cabac.encode_bin(0, context); // not in {planar, DC, vertical} MPM list
        encode_bypass_bits(cabac, 8, 5); // rem_intra_luma_pred_mode
        return;
    }
    cabac.encode_bin(1, context);
    cabac.encode_bypass(1);
    cabac.encode_bypass(mode == IntraPredictionMode::Vertical);
}

void encode_chroma_dc_mode(CabacEncoder& cabac, CabacContext& context,
                           IntraPredictionMode luma_mode) {
    if (luma_mode == IntraPredictionMode::Dc) {
        cabac.encode_bin(0, context); // derived luma mode is already DC
        return;
    }
    cabac.encode_bin(1, context);
    encode_bypass_bits(cabac, 3, 2); // explicit chroma DC candidate
}

void encode_single_dc_coefficient(CabacEncoder& cabac, int level,
                                  CabacContext& last_x, CabacContext& last_y,
                                  CabacContext& greater_one,
                                  CabacContext& greater_two) {
    if (level == 0) return;
    cabac.encode_bin(0, last_x); // last_sig_coeff_x_prefix = 0
    cabac.encode_bin(0, last_y); // last_sig_coeff_y_prefix = 0

    const unsigned magnitude = static_cast<unsigned>(std::abs(level));
    cabac.encode_bin(magnitude > 1, greater_one);
    if (magnitude > 1) {
        cabac.encode_bin(magnitude > 2, greater_two);
    }
    cabac.encode_bypass(level < 0); // coeff_sign_flag
    if (magnitude > 2) {
        encode_coeff_remaining(cabac, magnitude - 3);
    }
}

void write_profile_tier_level(BitWriter& bits) {
    bits.put_bits(0, 2); // general_profile_space
    bits.put_bit(false); // general_tier_flag: Main tier
    bits.put_bits(1, 5); // general_profile_idc: Main profile
    for (unsigned i = 0; i < 32; ++i) {
        bits.put_bit(i == 1); // compatible with Main profile
    }
    bits.put_bit(true);  // progressive source
    bits.put_bit(false); // interlaced source
    bits.put_bit(false); // non-packed constraint
    bits.put_bit(true);  // frame-only constraint
    bits.put_bits(0, 43); // reserved_zero_43bits for Main profile
    bits.put_bit(false);  // general_inbld_flag
    bits.put_bits(186, 8); // Level 6.2; timing is intentionally not signalled
}

void append_annex_b_nal(std::vector<std::uint8_t>& output, unsigned nal_unit_type,
                        const std::vector<std::uint8_t>& rbsp) {
    output.insert(output.end(), {0x00, 0x00, 0x00, 0x01});
    const std::uint16_t header = static_cast<std::uint16_t>(
        ((nal_unit_type & 0x3fU) << 9) | 1U); // layer_id=0, temporal_id_plus1=1
    output.push_back(static_cast<std::uint8_t>(header >> 8));
    output.push_back(static_cast<std::uint8_t>(header & 0xff));
    const auto ebsp = rbsp_to_ebsp(rbsp);
    output.insert(output.end(), ebsp.begin(), ebsp.end());
}

void write_pcm_plane(BitWriter& bits, const Plane& plane, unsigned x0, unsigned y0,
                     unsigned block_size) {
    for (unsigned y = 0; y < block_size; ++y) {
        for (unsigned x = 0; x < block_size; ++x) {
            bits.put_byte(plane.at(x0 + x, y0 + y));
        }
    }
}

} // namespace

HevcPcmEncoder::HevcPcmEncoder(EncoderConfig config) : config_(config) {
    (void)yuv420_frame_bytes(config.width, config.height);
    if (config.qp < 0 || config.qp > 51) {
        throw std::invalid_argument("QP must be in range 0..51");
    }
    if (static_cast<std::uint32_t>(config.mode) >
        static_cast<std::uint32_t>(CodingMode::IntraDirectionalTq)) {
        throw std::invalid_argument("unsupported coding mode");
    }
    padded_width_ = round_up(config.width, kCtuSize);
    padded_height_ = round_up(config.height, kCtuSize);
}

unsigned HevcPcmEncoder::ctu_count() const {
    return (padded_width_ / kCtuSize) * (padded_height_ / kCtuSize);
}

std::vector<std::uint8_t> HevcPcmEncoder::make_vps_rbsp() const {
    BitWriter bits;
    bits.put_bits(0, 4);      // vps_video_parameter_set_id
    bits.put_bit(true);       // vps_base_layer_internal_flag
    bits.put_bit(true);       // vps_base_layer_available_flag
    bits.put_bits(0, 6);      // vps_max_layers_minus1
    bits.put_bits(0, 3);      // vps_max_sub_layers_minus1
    bits.put_bit(true);       // vps_temporal_id_nesting_flag
    bits.put_bits(0xffff, 16);
    write_profile_tier_level(bits);
    bits.put_bit(true);       // sub-layer ordering info present
    bits.put_ue(0);           // max_dec_pic_buffering_minus1
    bits.put_ue(0);           // max_num_reorder_pics
    bits.put_ue(0);           // max_latency_increase_plus1
    bits.put_bits(0, 6);      // vps_max_layer_id
    bits.put_ue(0);           // vps_num_layer_sets_minus1
    bits.put_bit(false);      // vps_timing_info_present_flag
    bits.put_bit(false);      // vps_extension_flag
    bits.rbsp_trailing_bits();
    return bits.bytes();
}

std::vector<std::uint8_t> HevcPcmEncoder::make_sps_rbsp() const {
    BitWriter bits;
    bits.put_bits(0, 4); // sps_video_parameter_set_id
    bits.put_bits(0, 3); // sps_max_sub_layers_minus1
    bits.put_bit(true);
    write_profile_tier_level(bits);
    bits.put_ue(0); // sps_seq_parameter_set_id
    bits.put_ue(1); // chroma_format_idc: 4:2:0
    bits.put_ue(padded_width_);
    bits.put_ue(padded_height_);

    const bool cropped = padded_width_ != config_.width || padded_height_ != config_.height;
    bits.put_bit(cropped);
    if (cropped) {
        bits.put_ue(0); // left
        bits.put_ue((padded_width_ - config_.width) / 2); // right, crop unit is 2 luma samples
        bits.put_ue(0); // top
        bits.put_ue((padded_height_ - config_.height) / 2); // bottom
    }
    bits.put_ue(0); // bit_depth_luma_minus8
    bits.put_ue(0); // bit_depth_chroma_minus8
    bits.put_ue(4); // log2_max_pic_order_cnt_lsb_minus4
    bits.put_bit(true);
    bits.put_ue(0); // max_dec_pic_buffering_minus1
    bits.put_ue(0); // max_num_reorder_pics
    bits.put_ue(0); // max_latency_increase_plus1

    bits.put_ue(1); // log2_min_luma_coding_block_size_minus3: 16
    bits.put_ue(1); // log2_diff_max_min_luma_coding_block_size: CTU 32
    bits.put_ue(0); // log2_min_luma_transform_block_size_minus2: 4
    bits.put_ue(3); // log2_diff_max_min_luma_transform_block_size: 32
    bits.put_ue(0); // max_transform_hierarchy_depth_inter
    bits.put_ue(config_.mode == CodingMode::IntraFullTq16 ||
                        config_.mode == CodingMode::IntraAdaptiveTq ||
                        config_.mode == CodingMode::IntraDirectionalTq
                    ? 1 : 0);
                    // max_transform_hierarchy_depth_intra
    bits.put_bit(false); // scaling_list_enabled_flag
    bits.put_bit(false); // amp_enabled_flag
    bits.put_bit(false); // sample_adaptive_offset_enabled_flag
    const bool pcm_enabled = config_.mode == CodingMode::Pcm ||
                             config_.mode == CodingMode::HybridDc;
    bits.put_bit(pcm_enabled);
    if (pcm_enabled) {
        bits.put_bits(7, 4); // pcm_sample_bit_depth_luma_minus1
        bits.put_bits(7, 4); // pcm_sample_bit_depth_chroma_minus1
        bits.put_ue(2);      // log2_min_pcm_luma_coding_block_size_minus3: 32
        bits.put_ue(0);      // log2_diff_max_min_pcm_luma_coding_block_size: max 32
        bits.put_bit(true);  // pcm_loop_filter_disabled_flag
    }
    bits.put_ue(0);      // num_short_term_ref_pic_sets
    bits.put_bit(false); // long_term_ref_pics_present_flag
    bits.put_bit(false); // sps_temporal_mvp_enabled_flag
    bits.put_bit(false); // strong_intra_smoothing_enabled_flag
    bits.put_bit(false); // vui_parameters_present_flag
    bits.put_bit(false); // sps_extension_present_flag
    bits.rbsp_trailing_bits();
    return bits.bytes();
}

std::vector<std::uint8_t> HevcPcmEncoder::make_pps_rbsp() const {
    BitWriter bits;
    bits.put_ue(0);      // pps_pic_parameter_set_id
    bits.put_ue(0);      // pps_seq_parameter_set_id
    bits.put_bit(false); // dependent_slice_segments_enabled_flag
    bits.put_bit(false); // output_flag_present_flag
    bits.put_bits(0, 3); // num_extra_slice_header_bits
    bits.put_bit(false); // sign_data_hiding_enabled_flag
    bits.put_bit(false); // cabac_init_present_flag
    bits.put_ue(0);      // num_ref_idx_l0_default_active_minus1
    bits.put_ue(0);      // num_ref_idx_l1_default_active_minus1
    bits.put_se(config_.qp - 26); // init_qp_minus26
    bits.put_bit(false); // constrained_intra_pred_flag
    bits.put_bit(false); // transform_skip_enabled_flag
    bits.put_bit(false); // cu_qp_delta_enabled_flag
    bits.put_se(0);      // pps_cb_qp_offset
    bits.put_se(0);      // pps_cr_qp_offset
    bits.put_bit(false); // pps_slice_chroma_qp_offsets_present_flag
    bits.put_bit(false); // weighted_pred_flag
    bits.put_bit(false); // weighted_bipred_flag
    bits.put_bit(false); // transquant_bypass_enabled_flag
    bits.put_bit(false); // tiles_enabled_flag
    bits.put_bit(false); // entropy_coding_sync_enabled_flag
    bits.put_bit(false); // pps_loop_filter_across_slices_enabled_flag
    const bool disable_deblocking =
        config_.mode == CodingMode::IntraFullTq16 ||
        config_.mode == CodingMode::IntraAdaptiveTq ||
        config_.mode == CodingMode::IntraDirectionalTq;
    bits.put_bit(disable_deblocking); // deblocking_filter_control_present_flag
    if (disable_deblocking) {
        bits.put_bit(false); // deblocking_filter_override_enabled_flag
        bits.put_bit(true);  // pps_deblocking_filter_disabled_flag
    }
    bits.put_bit(false); // pps_scaling_list_data_present_flag
    bits.put_bit(false); // lists_modification_present_flag
    bits.put_ue(0);      // log2_parallel_merge_level_minus2
    bits.put_bit(false); // slice_segment_header_extension_present_flag
    bits.put_bit(false); // pps_extension_present_flag
    bits.rbsp_trailing_bits();
    return bits.bytes();
}

std::vector<std::uint8_t> HevcPcmEncoder::parameter_sets_annex_b() const {
    std::vector<std::uint8_t> output;
    append_annex_b_nal(output, 32, make_vps_rbsp());
    append_annex_b_nal(output, 33, make_sps_rbsp());
    append_annex_b_nal(output, 34, make_pps_rbsp());
    return output;
}

std::vector<std::uint8_t>
HevcPcmEncoder::make_pcm_slice_rbsp(const Yuv420Frame& padded, unsigned ctu_address,
                                    bool first_slice) const {
    BitWriter bits;
    bits.put_bit(first_slice);
    bits.put_bit(false); // no_output_of_prior_pics_flag
    bits.put_ue(0);      // slice_pic_parameter_set_id
    if (!first_slice) {
        bits.put_bits(ctu_address, ceil_log2(ctu_count()));
    }
    bits.put_ue(2); // slice_type: I
    bits.put_se(0); // slice_qp_delta

    // byte_alignment(): a one bit followed by zero bits
    bits.put_bit(true);
    bits.byte_align_zero();

    const unsigned ctus_per_row = padded_width_ / kCtuSize;
    const unsigned ctu_x = ctu_address % ctus_per_row;
    const unsigned ctu_y = ctu_address / ctus_per_row;
    const unsigned x0 = ctu_x * kCtuSize;
    const unsigned y0 = ctu_y * kCtuSize;

    // One independent slice carries one 32x32 CTU/CU. The CU is kept unsplit,
    // then pcm_flag terminates arithmetic decoding before the raw samples.
    auto split_context = CabacContext::from_init_value(
        kSplitCuFlagInitIntraContext0, config_.qp);
    const auto cabac_start = bits.bits_written();
    CabacEncoder before_pcm(bits);
    before_pcm.encode_bin(0, split_context);
    before_pcm.encode_terminate(1);
    before_pcm.finish();
    bits.byte_align_zero();
    if (bits.bits_written() - cabac_start < 9) {
        bits.put_byte(0);
    }
    write_pcm_plane(bits, padded.y, x0, y0, kPcmCuSize);
    write_pcm_plane(bits, padded.cb, x0 / 2, y0 / 2, kPcmCuSize / 2);
    write_pcm_plane(bits, padded.cr, x0 / 2, y0 / 2, kPcmCuSize / 2);

    // The arithmetic decoder is reset after pcm_sample_chroma. This slice owns
    // exactly one CTU, so its only remaining bin is end_of_slice_segment_flag=1.
    CabacEncoder after_pcm(bits);
    after_pcm.encode_terminate(1);
    after_pcm.finish();
    bits.rbsp_trailing_bits();
    return bits.bytes();
}

std::vector<std::uint8_t>
HevcPcmEncoder::make_intra_dc_slice_rbsp(unsigned ctu_address,
                                         bool first_slice) const {
    BitWriter bits;
    bits.put_bit(first_slice);
    bits.put_bit(false); // no_output_of_prior_pics_flag
    bits.put_ue(0);      // slice_pic_parameter_set_id
    if (!first_slice) {
        bits.put_bits(ctu_address, ceil_log2(ctu_count()));
    }
    bits.put_ue(2); // slice_type: I
    bits.put_se(0); // slice_qp_delta
    bits.put_bit(true); // byte_alignment()
    bits.byte_align_zero();

    // One unsplit 32x32 CU/TU per independent slice. Both neighbouring PUs are
    // unavailable at a slice boundary, so the MPM list is Planar, DC, Vertical.
    // We select MPM index 1 (DC), derive chroma from luma and signal no residual.
    auto split = CabacContext::from_init_value(
        kSplitCuFlagInitIntraContext0, config_.qp);
    auto luma_mode = CabacContext::from_init_value(
        kPrevIntraLumaPredInitIntra, config_.qp);
    auto chroma_mode = CabacContext::from_init_value(
        kIntraChromaPredInitIntra, config_.qp);
    auto cbf_chroma = CabacContext::from_init_value(
        kCbfChromaDepth0InitIntra, config_.qp);
    auto cbf_luma = CabacContext::from_init_value(
        kCbfLumaDepth0InitIntra, config_.qp);

    CabacEncoder cabac(bits);
    cabac.encode_bin(0, split);       // split_cu_flag: one 32x32 CU
    if (config_.mode == CodingMode::HybridDc) {
        cabac.encode_terminate(0);     // pcm_flag: coded CU, not PCM
    }
    cabac.encode_bin(1, luma_mode);   // prev_intra_luma_pred_flag
    cabac.encode_bypass(1);           // mpm_idx != 0
    cabac.encode_bypass(0);           // mpm_idx - 1 = 0 -> DC
    cabac.encode_bin(0, chroma_mode); // intra_chroma_pred_mode: derived
    cabac.encode_bin(0, cbf_chroma);  // cbf_cb = 0
    cabac.encode_bin(0, cbf_chroma);  // cbf_cr = 0, same adaptive context
    cabac.encode_bin(0, cbf_luma);    // cbf_luma = 0
    cabac.encode_terminate(1);         // end_of_slice_segment_flag
    cabac.finish();
    bits.rbsp_trailing_bits();
    return bits.bytes();
}

HevcPcmEncoder::DcLevels
HevcPcmEncoder::dc_levels(const Yuv420Frame& padded,
                          unsigned ctu_address) const {
    const unsigned ctus_per_row = padded_width_ / kCtuSize;
    const unsigned x0 = (ctu_address % ctus_per_row) * kCtuSize;
    const unsigned y0 = (ctu_address / ctus_per_row) * kCtuSize;
    const int chroma_qp = kChromaQp420[static_cast<unsigned>(config_.qp)];
    return {
        quantize_dc(forward_dc(padded.y, x0, y0, kCtuSize),
                    kCtuSize, config_.qp),
        quantize_dc(forward_dc(padded.cb, x0 / 2, y0 / 2, kCtuSize / 2),
                    kCtuSize / 2, chroma_qp),
        quantize_dc(forward_dc(padded.cr, x0 / 2, y0 / 2, kCtuSize / 2),
                    kCtuSize / 2, chroma_qp),
    };
}

std::vector<std::uint8_t>
HevcPcmEncoder::make_intra_dc_tq_slice_rbsp(const Yuv420Frame& padded,
                                            unsigned ctu_address,
                                            bool first_slice) const {
    BitWriter bits;
    bits.put_bit(first_slice);
    bits.put_bit(false); // no_output_of_prior_pics_flag
    bits.put_ue(0);      // slice_pic_parameter_set_id
    if (!first_slice) {
        bits.put_bits(ctu_address, ceil_log2(ctu_count()));
    }
    bits.put_ue(2); // slice_type: I
    bits.put_se(0); // slice_qp_delta
    bits.put_bit(true);
    bits.byte_align_zero();

    const auto levels = dc_levels(padded, ctu_address);
    auto split = CabacContext::from_init_value(
        kSplitCuFlagInitIntraContext0, config_.qp);
    auto luma_mode = CabacContext::from_init_value(
        kPrevIntraLumaPredInitIntra, config_.qp);
    auto chroma_mode = CabacContext::from_init_value(
        kIntraChromaPredInitIntra, config_.qp);
    auto cbf_chroma = CabacContext::from_init_value(
        kCbfChromaDepth0InitIntra, config_.qp);
    auto cbf_luma = CabacContext::from_init_value(
        kCbfLumaDepth0InitIntra, config_.qp);

    auto last_x_luma = CabacContext::from_init_value(
        kLastLuma32InitIntra, config_.qp);
    auto last_y_luma = CabacContext::from_init_value(
        kLastLuma32InitIntra, config_.qp);
    auto one_luma = CabacContext::from_init_value(
        kOneLumaSet0C1InitIntra, config_.qp);
    auto abs_luma = CabacContext::from_init_value(
        kAbsLumaSet0InitIntra, config_.qp);
    auto last_x_chroma = CabacContext::from_init_value(
        kLastChromaInitIntra, config_.qp);
    auto last_y_chroma = CabacContext::from_init_value(
        kLastChromaInitIntra, config_.qp);
    auto one_chroma = CabacContext::from_init_value(
        kOneChromaSet4C1InitIntra, config_.qp);
    auto abs_chroma = CabacContext::from_init_value(
        kAbsChromaSet4InitIntra, config_.qp);

    CabacEncoder cabac(bits);
    cabac.encode_bin(0, split);
    cabac.encode_bin(1, luma_mode);   // DC is MPM index 1
    cabac.encode_bypass(1);
    cabac.encode_bypass(0);
    cabac.encode_bin(0, chroma_mode); // derived DC
    cabac.encode_bin(levels.cb != 0, cbf_chroma);
    cabac.encode_bin(levels.cr != 0, cbf_chroma);
    cabac.encode_bin(levels.y != 0, cbf_luma);

    encode_single_dc_coefficient(cabac, levels.y, last_x_luma, last_y_luma,
                                 one_luma, abs_luma);
    encode_single_dc_coefficient(cabac, levels.cb, last_x_chroma, last_y_chroma,
                                 one_chroma, abs_chroma);
    encode_single_dc_coefficient(cabac, levels.cr, last_x_chroma, last_y_chroma,
                                 one_chroma, abs_chroma);
    cabac.encode_terminate(1);
    cabac.finish();
    bits.rbsp_trailing_bits();
    return bits.bytes();
}

std::vector<std::uint8_t>
HevcPcmEncoder::make_intra_full_tq_slice_rbsp(const Yuv420Frame& padded,
                                              unsigned ctu_address,
                                              bool first_slice) const {
    BitWriter bits;
    bits.put_bit(first_slice);
    bits.put_bit(false); // no_output_of_prior_pics_flag
    bits.put_ue(0);      // slice_pic_parameter_set_id
    if (!first_slice) {
        bits.put_bits(ctu_address, ceil_log2(ctu_count()));
    }
    bits.put_ue(2); // slice_type: I
    bits.put_se(0); // slice_qp_delta
    bits.put_bit(true);
    bits.byte_align_zero();

    const unsigned ctus_per_row = padded_width_ / kCtuSize;
    const unsigned x0 = (ctu_address % ctus_per_row) * kCtuSize;
    const unsigned y0 = (ctu_address / ctus_per_row) * kCtuSize;
    const int chroma_qp = kChromaQp420[static_cast<unsigned>(config_.qp)];
    const auto luma = transform_quantize_block(
        padded.y, x0, y0, kCtuSize, config_.qp);
    const auto cb = transform_quantize_block(
        padded.cb, x0 / 2, y0 / 2, kCtuSize / 2, chroma_qp);
    const auto cr = transform_quantize_block(
        padded.cr, x0 / 2, y0 / 2, kCtuSize / 2, chroma_qp);

    auto split = CabacContext::from_init_value(
        kSplitCuFlagInitIntraContext0, config_.qp);
    auto luma_mode = CabacContext::from_init_value(
        kPrevIntraLumaPredInitIntra, config_.qp);
    auto chroma_mode = CabacContext::from_init_value(
        kIntraChromaPredInitIntra, config_.qp);
    auto cbf_chroma = CabacContext::from_init_value(
        kCbfChromaDepth0InitIntra, config_.qp);
    auto cbf_luma = CabacContext::from_init_value(
        kCbfLumaDepth0InitIntra, config_.qp);
    CoefficientContexts luma_contexts(true, config_.qp);
    CoefficientContexts chroma_contexts(false, config_.qp);

    CabacEncoder cabac(bits);
    cabac.encode_bin(0, split);
    cabac.encode_bin(1, luma_mode);   // DC is MPM index 1
    cabac.encode_bypass(1);
    cabac.encode_bypass(0);
    cabac.encode_bin(0, chroma_mode); // derived DC
    cabac.encode_bin(cb.has_nonzero(), cbf_chroma);
    cabac.encode_bin(cr.has_nonzero(), cbf_chroma);
    cabac.encode_bin(luma.has_nonzero(), cbf_luma);
    encode_coefficients(cabac, luma, luma_contexts);
    encode_coefficients(cabac, cb, chroma_contexts);
    encode_coefficients(cabac, cr, chroma_contexts);
    cabac.encode_terminate(1);
    cabac.finish();
    bits.rbsp_trailing_bits();
    return bits.bytes();
}

std::vector<std::uint8_t>
HevcPcmEncoder::make_intra_full_tq16_slice_rbsp(const Yuv420Frame& padded,
                                                unsigned ctu_address,
                                                bool first_slice) const {
    BitWriter bits;
    bits.put_bit(first_slice);
    bits.put_bit(false); // no_output_of_prior_pics_flag
    bits.put_ue(0);      // slice_pic_parameter_set_id
    if (!first_slice) {
        bits.put_bits(ctu_address, ceil_log2(ctu_count()));
    }
    bits.put_ue(2); // slice_type: I
    bits.put_se(0); // slice_qp_delta
    bits.put_bit(true);
    bits.byte_align_zero();

    const unsigned ctus_per_row = padded_width_ / kCtuSize;
    const unsigned x0 = (ctu_address % ctus_per_row) * kCtuSize;
    const unsigned y0 = (ctu_address / ctus_per_row) * kCtuSize;
    const auto split = make_split_tq16_result(padded, x0, y0, config_.qp);

    const bool root_cb = std::any_of(split.leaves.begin(), split.leaves.end(),
        [](const SplitTq16Leaf& leaf) { return leaf.cb.has_nonzero(); });
    const bool root_cr = std::any_of(split.leaves.begin(), split.leaves.end(),
        [](const SplitTq16Leaf& leaf) { return leaf.cr.has_nonzero(); });
    auto split_cu = CabacContext::from_init_value(
        kSplitCuFlagInitIntraContext0, config_.qp);
    auto luma_mode = CabacContext::from_init_value(
        kPrevIntraLumaPredInitIntra, config_.qp);
    auto chroma_mode = CabacContext::from_init_value(
        kIntraChromaPredInitIntra, config_.qp);
    auto transform_split = CabacContext::from_init_value(
        kTransformSplit32InitIntra, config_.qp);
    auto cbf_chroma_root = CabacContext::from_init_value(
        kCbfChromaDepth0InitIntra, config_.qp);
    auto cbf_chroma_leaf = CabacContext::from_init_value(
        kCbfChromaDepth1InitIntra, config_.qp);
    auto cbf_luma_leaf = CabacContext::from_init_value(
        kCbfLumaDepth1InitIntra, config_.qp);
    CoefficientContexts luma_contexts(true, config_.qp);
    CoefficientContexts chroma_contexts(false, config_.qp);

    CabacEncoder cabac(bits);
    cabac.encode_bin(0, split_cu);
    cabac.encode_bin(1, luma_mode);   // DC is MPM index 1
    cabac.encode_bypass(1);
    cabac.encode_bypass(0);
    cabac.encode_bin(0, chroma_mode); // derived DC
    cabac.encode_bin(1, transform_split); // split 32x32 into four 16x16 TUs
    cabac.encode_bin(root_cb, cbf_chroma_root);
    cabac.encode_bin(root_cr, cbf_chroma_root);
    for (const auto& leaf : split.leaves) {
        if (root_cb) {
            cabac.encode_bin(leaf.cb.has_nonzero(), cbf_chroma_leaf);
        }
        if (root_cr) {
            cabac.encode_bin(leaf.cr.has_nonzero(), cbf_chroma_leaf);
        }
        cabac.encode_bin(leaf.luma.has_nonzero(), cbf_luma_leaf);
        encode_coefficients(cabac, leaf.luma, luma_contexts);
        encode_coefficients(cabac, leaf.cb, chroma_contexts);
        encode_coefficients(cabac, leaf.cr, chroma_contexts);
    }
    cabac.encode_terminate(1);
    cabac.finish();
    bits.rbsp_trailing_bits();
    return bits.bytes();
}

std::vector<std::uint8_t>
HevcPcmEncoder::make_intra_adaptive_tq_slice_rbsp(
    const Yuv420Frame& padded, unsigned ctu_address, bool first_slice) const {
    BitWriter bits;
    bits.put_bit(first_slice);
    bits.put_bit(false); // no_output_of_prior_pics_flag
    bits.put_ue(0);      // slice_pic_parameter_set_id
    if (!first_slice) {
        bits.put_bits(ctu_address, ceil_log2(ctu_count()));
    }
    bits.put_ue(2); // slice_type: I
    bits.put_se(0); // slice_qp_delta
    bits.put_bit(true);
    bits.byte_align_zero();

    const unsigned ctus_per_row = padded_width_ / kCtuSize;
    const unsigned x0 = (ctu_address % ctus_per_row) * kCtuSize;
    const unsigned y0 = (ctu_address / ctus_per_row) * kCtuSize;
    const auto full = make_full_tq32_result(padded, x0, y0, config_.qp);
    const auto split = make_split_tq16_result(padded, x0, y0, config_.qp);
    const bool use_split = choose_split_tq16(padded, x0, y0, full, split);

    auto split_cu = CabacContext::from_init_value(
        kSplitCuFlagInitIntraContext0, config_.qp);
    auto luma_mode = CabacContext::from_init_value(
        kPrevIntraLumaPredInitIntra, config_.qp);
    auto chroma_mode = CabacContext::from_init_value(
        kIntraChromaPredInitIntra, config_.qp);
    auto transform_split = CabacContext::from_init_value(
        kTransformSplit32InitIntra, config_.qp);
    auto cbf_chroma_root = CabacContext::from_init_value(
        kCbfChromaDepth0InitIntra, config_.qp);
    auto cbf_chroma_leaf = CabacContext::from_init_value(
        kCbfChromaDepth1InitIntra, config_.qp);
    auto cbf_luma_root = CabacContext::from_init_value(
        kCbfLumaDepth0InitIntra, config_.qp);
    auto cbf_luma_leaf = CabacContext::from_init_value(
        kCbfLumaDepth1InitIntra, config_.qp);
    CoefficientContexts luma_contexts(true, config_.qp);
    CoefficientContexts chroma_contexts(false, config_.qp);

    CabacEncoder cabac(bits);
    cabac.encode_bin(0, split_cu);
    cabac.encode_bin(1, luma_mode);   // DC is MPM index 1
    cabac.encode_bypass(1);
    cabac.encode_bypass(0);
    cabac.encode_bin(0, chroma_mode); // derived DC
    cabac.encode_bin(use_split, transform_split);
    if (use_split) {
        const bool root_cb = std::any_of(
            split.leaves.begin(), split.leaves.end(),
            [](const SplitTq16Leaf& leaf) { return leaf.cb.has_nonzero(); });
        const bool root_cr = std::any_of(
            split.leaves.begin(), split.leaves.end(),
            [](const SplitTq16Leaf& leaf) { return leaf.cr.has_nonzero(); });
        cabac.encode_bin(root_cb, cbf_chroma_root);
        cabac.encode_bin(root_cr, cbf_chroma_root);
        for (const auto& leaf : split.leaves) {
            if (root_cb) {
                cabac.encode_bin(leaf.cb.has_nonzero(), cbf_chroma_leaf);
            }
            if (root_cr) {
                cabac.encode_bin(leaf.cr.has_nonzero(), cbf_chroma_leaf);
            }
            cabac.encode_bin(leaf.luma.has_nonzero(), cbf_luma_leaf);
            encode_coefficients(cabac, leaf.luma, luma_contexts);
            encode_coefficients(cabac, leaf.cb, chroma_contexts);
            encode_coefficients(cabac, leaf.cr, chroma_contexts);
        }
    } else {
        cabac.encode_bin(full.cb.has_nonzero(), cbf_chroma_root);
        cabac.encode_bin(full.cr.has_nonzero(), cbf_chroma_root);
        cabac.encode_bin(full.luma.has_nonzero(), cbf_luma_root);
        encode_coefficients(cabac, full.luma, luma_contexts);
        encode_coefficients(cabac, full.cb, chroma_contexts);
        encode_coefficients(cabac, full.cr, chroma_contexts);
    }
    cabac.encode_terminate(1);
    cabac.finish();
    bits.rbsp_trailing_bits();
    return bits.bytes();
}

std::vector<std::uint8_t>
HevcPcmEncoder::make_intra_directional_tq_slice_rbsp(
    const Yuv420Frame& padded, unsigned ctu_address, bool first_slice) const {
    BitWriter bits;
    bits.put_bit(first_slice);
    bits.put_bit(false); // no_output_of_prior_pics_flag
    bits.put_ue(0);      // slice_pic_parameter_set_id
    if (!first_slice) {
        bits.put_bits(ctu_address, ceil_log2(ctu_count()));
    }
    bits.put_ue(2); // slice_type: I
    bits.put_se(0); // slice_qp_delta
    bits.put_bit(true);
    bits.byte_align_zero();

    const unsigned ctus_per_row = padded_width_ / kCtuSize;
    const unsigned x0 = (ctu_address % ctus_per_row) * kCtuSize;
    const unsigned y0 = (ctu_address / ctus_per_row) * kCtuSize;
    const auto decision = make_directional_decision(
        padded, x0, y0, config_.qp);

    auto split_cu = CabacContext::from_init_value(
        kSplitCuFlagInitIntraContext0, config_.qp);
    auto luma_mode = CabacContext::from_init_value(
        kPrevIntraLumaPredInitIntra, config_.qp);
    auto chroma_mode = CabacContext::from_init_value(
        kIntraChromaPredInitIntra, config_.qp);
    auto transform_split = CabacContext::from_init_value(
        kTransformSplit32InitIntra, config_.qp);
    auto cbf_chroma_root = CabacContext::from_init_value(
        kCbfChromaDepth0InitIntra, config_.qp);
    auto cbf_chroma_leaf = CabacContext::from_init_value(
        kCbfChromaDepth1InitIntra, config_.qp);
    auto cbf_luma_root = CabacContext::from_init_value(
        kCbfLumaDepth0InitIntra, config_.qp);
    auto cbf_luma_leaf = CabacContext::from_init_value(
        kCbfLumaDepth1InitIntra, config_.qp);
    CoefficientContexts luma_contexts(true, config_.qp);
    CoefficientContexts chroma_contexts(false, config_.qp);

    const auto selected_mode = decision.use_split
        ? decision.mode : IntraPredictionMode::Dc;
    CabacEncoder cabac(bits);
    cabac.encode_bin(0, split_cu);
    encode_luma_intra_mode(cabac, luma_mode, selected_mode);
    encode_chroma_dc_mode(cabac, chroma_mode, selected_mode);
    cabac.encode_bin(decision.use_split, transform_split);
    if (decision.use_split) {
        const auto& split = selected_split(decision);
        const bool root_cb = std::any_of(
            split.leaves.begin(), split.leaves.end(),
            [](const SplitTq16Leaf& leaf) { return leaf.cb.has_nonzero(); });
        const bool root_cr = std::any_of(
            split.leaves.begin(), split.leaves.end(),
            [](const SplitTq16Leaf& leaf) { return leaf.cr.has_nonzero(); });
        cabac.encode_bin(root_cb, cbf_chroma_root);
        cabac.encode_bin(root_cr, cbf_chroma_root);
        for (const auto& leaf : split.leaves) {
            if (root_cb) {
                cabac.encode_bin(leaf.cb.has_nonzero(), cbf_chroma_leaf);
            }
            if (root_cr) {
                cabac.encode_bin(leaf.cr.has_nonzero(), cbf_chroma_leaf);
            }
            cabac.encode_bin(leaf.luma.has_nonzero(), cbf_luma_leaf);
            encode_coefficients(cabac, leaf.luma, luma_contexts);
            encode_coefficients(cabac, leaf.cb, chroma_contexts);
            encode_coefficients(cabac, leaf.cr, chroma_contexts);
        }
    } else {
        const auto& full = decision.full;
        cabac.encode_bin(full.cb.has_nonzero(), cbf_chroma_root);
        cabac.encode_bin(full.cr.has_nonzero(), cbf_chroma_root);
        cabac.encode_bin(full.luma.has_nonzero(), cbf_luma_root);
        encode_coefficients(cabac, full.luma, luma_contexts);
        encode_coefficients(cabac, full.cb, chroma_contexts);
        encode_coefficients(cabac, full.cr, chroma_contexts);
    }
    cabac.encode_terminate(1);
    cabac.finish();
    bits.rbsp_trailing_bits();
    return bits.bytes();
}

bool HevcPcmEncoder::use_intra_dc(const Yuv420Frame& padded,
                                  unsigned ctu_address) const {
    if (config_.mode == CodingMode::IntraDc) return true;
    if (config_.mode == CodingMode::Pcm) return false;

    const unsigned ctus_per_row = padded_width_ / kCtuSize;
    const unsigned x0 = (ctu_address % ctus_per_row) * kCtuSize;
    const unsigned y0 = (ctu_address / ctus_per_row) * kCtuSize;
    std::uint64_t sad = 0;
    std::uint64_t samples = 0;
    const auto accumulate = [&](const Plane& plane, unsigned px, unsigned py,
                                unsigned block) {
        for (unsigned y = 0; y < block; ++y) {
            for (unsigned x = 0; x < block; ++x) {
                const int value = plane.at(px + x, py + y);
                sad += static_cast<std::uint64_t>(value > 128
                    ? value - 128 : 128 - value);
                ++samples;
            }
        }
    };
    accumulate(padded.y, x0, y0, kCtuSize);
    accumulate(padded.cb, x0 / 2, y0 / 2, kCtuSize / 2);
    accumulate(padded.cr, x0 / 2, y0 / 2, kCtuSize / 2);

    // QP acts as a distortion budget for the M1 hybrid mode. A CTU whose mean
    // absolute error to the no-residual DC reconstruction is at most QP is
    // coded as DC; otherwise it falls back to lossless PCM.
    return sad <= samples * static_cast<std::uint64_t>(config_.qp);
}

Yuv420Frame HevcPcmEncoder::reconstruct_idr_frame(const Yuv420Frame& frame) const {
    if (frame.width != config_.width || frame.height != config_.height) {
        throw std::invalid_argument("input frame dimensions do not match encoder config");
    }
    auto padded = pad_yuv420_edge(frame, padded_width_, padded_height_);
    const unsigned ctus_per_row = padded_width_ / kCtuSize;
    const auto fill_block = [](Plane& plane, unsigned x0, unsigned y0,
                               unsigned block) {
        for (unsigned y = 0; y < block; ++y) {
            std::fill_n(plane.samples.begin() +
                            static_cast<std::ptrdiff_t>(y0 + y) * plane.width + x0,
                        block, static_cast<std::uint8_t>(128));
        }
    };
    for (unsigned address = 0; address < ctu_count(); ++address) {
        if (config_.mode == CodingMode::Pcm) continue;
        if (config_.mode == CodingMode::HybridDc &&
            !use_intra_dc(padded, address)) continue;
        const unsigned x0 = (address % ctus_per_row) * kCtuSize;
        const unsigned y0 = (address / ctus_per_row) * kCtuSize;
        if (config_.mode == CodingMode::IntraDcTq) {
            const auto levels = dc_levels(padded, address);
            const int chroma_qp = kChromaQp420[static_cast<unsigned>(config_.qp)];
            const auto fill_value = [](Plane& plane, unsigned px, unsigned py,
                                       unsigned block, std::uint8_t value) {
                for (unsigned y = 0; y < block; ++y) {
                    std::fill_n(plane.samples.begin() +
                                    static_cast<std::ptrdiff_t>(py + y) *
                                    plane.width + px,
                                block, value);
                }
            };
            fill_value(padded.y, x0, y0, kCtuSize,
                       static_cast<std::uint8_t>(inverse_dc_sample(
                           levels.y, kCtuSize, config_.qp)));
            fill_value(padded.cb, x0 / 2, y0 / 2, kCtuSize / 2,
                       static_cast<std::uint8_t>(inverse_dc_sample(
                           levels.cb, kCtuSize / 2, chroma_qp)));
            fill_value(padded.cr, x0 / 2, y0 / 2, kCtuSize / 2,
                       static_cast<std::uint8_t>(inverse_dc_sample(
                           levels.cr, kCtuSize / 2, chroma_qp)));
        } else if (config_.mode == CodingMode::IntraFullTq) {
            const int chroma_qp = kChromaQp420[static_cast<unsigned>(config_.qp)];
            const auto luma = transform_quantize_block(
                padded.y, x0, y0, kCtuSize, config_.qp);
            const auto cb = transform_quantize_block(
                padded.cb, x0 / 2, y0 / 2, kCtuSize / 2, chroma_qp);
            const auto cr = transform_quantize_block(
                padded.cr, x0 / 2, y0 / 2, kCtuSize / 2, chroma_qp);
            inverse_reconstruct_block(luma, config_.qp, padded.y, x0, y0);
            inverse_reconstruct_block(cb, chroma_qp, padded.cb, x0 / 2, y0 / 2);
            inverse_reconstruct_block(cr, chroma_qp, padded.cr, x0 / 2, y0 / 2);
        } else if (config_.mode == CodingMode::IntraDirectionalTq) {
            const auto decision = make_directional_decision(
                padded, x0, y0, config_.qp);
            const auto copy_block = [](const Plane& source, Plane& destination,
                                       unsigned px, unsigned py) {
                for (unsigned y = 0; y < source.height; ++y) {
                    std::copy_n(source.samples.begin() +
                                    static_cast<std::ptrdiff_t>(y) * source.width,
                                source.width,
                                destination.samples.begin() +
                                    static_cast<std::ptrdiff_t>(py + y) *
                                    destination.width + px);
                }
            };
            if (decision.use_split) {
                const auto& split = selected_split(decision);
                copy_block(split.luma_reconstruction, padded.y, x0, y0);
                copy_block(split.cb_reconstruction, padded.cb, x0 / 2, y0 / 2);
                copy_block(split.cr_reconstruction, padded.cr, x0 / 2, y0 / 2);
            } else {
                copy_block(decision.full.luma_reconstruction, padded.y, x0, y0);
                copy_block(decision.full.cb_reconstruction,
                           padded.cb, x0 / 2, y0 / 2);
                copy_block(decision.full.cr_reconstruction,
                           padded.cr, x0 / 2, y0 / 2);
            }
        } else if (config_.mode == CodingMode::IntraFullTq16 ||
                   config_.mode == CodingMode::IntraAdaptiveTq) {
            const auto split = make_split_tq16_result(
                padded, x0, y0, config_.qp);
            const auto copy_block = [](const Plane& source, Plane& destination,
                                       unsigned px, unsigned py) {
                for (unsigned y = 0; y < source.height; ++y) {
                    std::copy_n(source.samples.begin() +
                                    static_cast<std::ptrdiff_t>(y) * source.width,
                                source.width,
                                destination.samples.begin() +
                                    static_cast<std::ptrdiff_t>(py + y) *
                                    destination.width + px);
                }
            };
            if (config_.mode == CodingMode::IntraAdaptiveTq) {
                const auto full = make_full_tq32_result(
                    padded, x0, y0, config_.qp);
                if (!choose_split_tq16(padded, x0, y0, full, split)) {
                    copy_block(full.luma_reconstruction, padded.y, x0, y0);
                    copy_block(full.cb_reconstruction, padded.cb, x0 / 2, y0 / 2);
                    copy_block(full.cr_reconstruction, padded.cr, x0 / 2, y0 / 2);
                    continue;
                }
            }
            copy_block(split.luma_reconstruction, padded.y, x0, y0);
            copy_block(split.cb_reconstruction, padded.cb, x0 / 2, y0 / 2);
            copy_block(split.cr_reconstruction, padded.cr, x0 / 2, y0 / 2);
        } else {
            fill_block(padded.y, x0, y0, kCtuSize);
            fill_block(padded.cb, x0 / 2, y0 / 2, kCtuSize / 2);
            fill_block(padded.cr, x0 / 2, y0 / 2, kCtuSize / 2);
        }
    }

    Yuv420Frame result{
        config_.width,
        config_.height,
        {config_.width, config_.height,
         std::vector<std::uint8_t>(static_cast<std::size_t>(config_.width) *
                                   config_.height)},
        {config_.width / 2, config_.height / 2,
         std::vector<std::uint8_t>(static_cast<std::size_t>(config_.width / 2) *
                                   (config_.height / 2))},
        {config_.width / 2, config_.height / 2,
         std::vector<std::uint8_t>(static_cast<std::size_t>(config_.width / 2) *
                                   (config_.height / 2))},
    };
    const auto crop = [](const Plane& source, Plane& destination) {
        for (unsigned y = 0; y < destination.height; ++y) {
            std::copy_n(source.samples.begin() +
                            static_cast<std::ptrdiff_t>(y) * source.width,
                        destination.width,
                        destination.samples.begin() +
                            static_cast<std::ptrdiff_t>(y) * destination.width);
        }
    };
    crop(padded.y, result.y);
    crop(padded.cb, result.cb);
    crop(padded.cr, result.cr);
    return result;
}

std::vector<std::uint8_t>
HevcPcmEncoder::encode_idr_frame_annex_b(const Yuv420Frame& frame,
                                         std::uint64_t /*frame_index*/) const {
    if (frame.width != config_.width || frame.height != config_.height) {
        throw std::invalid_argument("input frame dimensions do not match encoder config");
    }
    std::vector<std::uint8_t> output;
    if (config_.mode == CodingMode::Pcm) {
        const auto padded = pad_yuv420_edge(frame, padded_width_, padded_height_);
        output.reserve(static_cast<std::size_t>(padded_width_) * padded_height_ * 3 / 2 +
                       ctu_count() * 64);
        for (unsigned address = 0; address < ctu_count(); ++address) {
            append_annex_b_nal(output, 20,
                make_pcm_slice_rbsp(padded, address, address == 0));
        }
    } else if (config_.mode == CodingMode::IntraDc) {
        output.reserve(ctu_count() * 24);
        for (unsigned address = 0; address < ctu_count(); ++address) {
            append_annex_b_nal(output, 20,
                make_intra_dc_slice_rbsp(address, address == 0));
        }
    } else if (config_.mode == CodingMode::HybridDc) {
        const auto padded = pad_yuv420_edge(frame, padded_width_, padded_height_);
        output.reserve(static_cast<std::size_t>(padded_width_) * padded_height_ * 3 / 2);
        for (unsigned address = 0; address < ctu_count(); ++address) {
            if (use_intra_dc(padded, address)) {
                append_annex_b_nal(output, 20,
                    make_intra_dc_slice_rbsp(address, address == 0));
            } else {
                append_annex_b_nal(output, 20,
                    make_pcm_slice_rbsp(padded, address, address == 0));
            }
        }
    } else if (config_.mode == CodingMode::IntraDcTq) {
        const auto padded = pad_yuv420_edge(frame, padded_width_, padded_height_);
        output.reserve(ctu_count() * 32);
        for (unsigned address = 0; address < ctu_count(); ++address) {
            append_annex_b_nal(output, 20,
                make_intra_dc_tq_slice_rbsp(padded, address, address == 0));
        }
    } else if (config_.mode == CodingMode::IntraFullTq) {
        const auto padded = pad_yuv420_edge(frame, padded_width_, padded_height_);
        output.reserve(static_cast<std::size_t>(padded_width_) * padded_height_ / 2);
        for (unsigned address = 0; address < ctu_count(); ++address) {
            append_annex_b_nal(output, 20,
                make_intra_full_tq_slice_rbsp(padded, address, address == 0));
        }
    } else if (config_.mode == CodingMode::IntraFullTq16) {
        const auto padded = pad_yuv420_edge(frame, padded_width_, padded_height_);
        output.reserve(static_cast<std::size_t>(padded_width_) * padded_height_ / 2);
        for (unsigned address = 0; address < ctu_count(); ++address) {
            append_annex_b_nal(output, 20,
                make_intra_full_tq16_slice_rbsp(padded, address, address == 0));
        }
    } else if (config_.mode == CodingMode::IntraAdaptiveTq) {
        const auto padded = pad_yuv420_edge(frame, padded_width_, padded_height_);
        output.reserve(static_cast<std::size_t>(padded_width_) * padded_height_ / 2);
        for (unsigned address = 0; address < ctu_count(); ++address) {
            append_annex_b_nal(output, 20,
                make_intra_adaptive_tq_slice_rbsp(
                    padded, address, address == 0));
        }
    } else {
        const auto padded = pad_yuv420_edge(frame, padded_width_, padded_height_);
        output.reserve(static_cast<std::size_t>(padded_width_) * padded_height_ / 2);
        for (unsigned address = 0; address < ctu_count(); ++address) {
            append_annex_b_nal(output, 20,
                make_intra_directional_tq_slice_rbsp(
                    padded, address, address == 0));
        }
    }
    return output;
}

} // namespace hevc
