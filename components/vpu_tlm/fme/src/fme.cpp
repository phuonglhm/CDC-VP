#include "fme.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace cdc::components {

namespace {

struct fme_candidate {
    bool valid = false;

    motion_vector mv {};
    std::uint32_t satd = 0;
    std::uint32_t rate = 0;
    std::uint32_t cost = 0;

    std::vector<std::uint8_t> predicted_luma;
};

int abs_int(int value)
{
    return value < 0 ? -value : value;
}

std::uint32_t clamp_u32(std::uint64_t value)
{
    return value > std::numeric_limits<std::uint32_t>::max()
               ? std::numeric_limits<std::uint32_t>::max()
               : static_cast<std::uint32_t>(value);
}

std::uint8_t clamp_u8(int value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

std::uint8_t read_luma_clamped(const frame& input, int x, int y)
{
    if (input.empty()) {
        return 128;
    }

    const int max_x = static_cast<int>(input.width) - 1;
    const int max_y = static_cast<int>(input.height) - 1;

    const int cx = std::clamp(x, 0, max_x);
    const int cy = std::clamp(y, 0, max_y);

    return input.get_luma(static_cast<std::uint32_t>(cx),
                          static_cast<std::uint32_t>(cy));
}

// TLM equivalent of fme_interpolator / half / quarter interpolation.
// Motion vectors are kept in quarter-pixel unit.
std::uint8_t read_luma_qpel(const frame& reference,
                            int pixel_x,
                            int pixel_y,
                            const motion_vector& mv)
{
    const int qx = pixel_x * 4 + mv.x;
    const int qy = pixel_y * 4 + mv.y;

    int base_x = qx / 4;
    int base_y = qy / 4;
    int frac_x = qx % 4;
    int frac_y = qy % 4;

    if (frac_x < 0) {
        frac_x += 4;
        --base_x;
    }

    if (frac_y < 0) {
        frac_y += 4;
        --base_y;
    }

    const int p00 = read_luma_clamped(reference, base_x,     base_y);
    const int p10 = read_luma_clamped(reference, base_x + 1, base_y);
    const int p01 = read_luma_clamped(reference, base_x,     base_y + 1);
    const int p11 = read_luma_clamped(reference, base_x + 1, base_y + 1);

    const int wx0 = 4 - frac_x;
    const int wx1 = frac_x;
    const int wy0 = 4 - frac_y;
    const int wy1 = frac_y;

    const int value =
        p00 * wx0 * wy0 +
        p10 * wx1 * wy0 +
        p01 * wx0 * wy1 +
        p11 * wx1 * wy1;

    return clamp_u8((value + 8) >> 4);
}

std::vector<std::uint8_t> make_prediction_block(const frame& reference,
                                                const block& current,
                                                const motion_vector& mv)
{
    std::vector<std::uint8_t> predicted;
    predicted.resize(static_cast<std::size_t>(current.area()), 128);

    for (std::uint32_t y = 0; y < current.height; ++y) {
        for (std::uint32_t x = 0; x < current.width; ++x) {
            const int px = static_cast<int>(current.x + x);
            const int py = static_cast<int>(current.y + y);

            predicted[static_cast<std::size_t>(y * current.width + x)] =
                read_luma_qpel(reference, px, py, mv);
        }
    }

    return predicted;
}

void hadamard_1d_8(std::array<int, 8>& data)
{
    for (std::size_t step = 1; step < 8; step <<= 1u) {
        for (std::size_t base = 0; base < 8; base += step << 1u) {
            for (std::size_t i = 0; i < step; ++i) {
                const int a = data[base + i];
                const int b = data[base + i + step];

                data[base + i] = a + b;
                data[base + i + step] = a - b;
            }
        }
    }
}

// TLM equivalent of fme_satd_8x8 + hadamard_trans_1d/2d.
// RTL divides accumulated 8x8 SATD by 4.
std::uint32_t satd_8x8(const std::array<int, 64>& residual)
{
    std::array<int, 64> tmp = residual;

    for (std::size_t y = 0; y < 8; ++y) {
        std::array<int, 8> row {};

        for (std::size_t x = 0; x < 8; ++x) {
            row[x] = tmp[y * 8 + x];
        }

        hadamard_1d_8(row);

        for (std::size_t x = 0; x < 8; ++x) {
            tmp[y * 8 + x] = row[x];
        }
    }

    std::uint64_t sum = 0;

    for (std::size_t x = 0; x < 8; ++x) {
        std::array<int, 8> col {};

        for (std::size_t y = 0; y < 8; ++y) {
            col[y] = tmp[y * 8 + x];
        }

        hadamard_1d_8(col);

        for (int value : col) {
            sum += static_cast<std::uint32_t>(abs_int(value));
        }
    }

    return clamp_u32((sum + 2u) >> 2u);
}

// TLM equivalent of fme_satd_gen.
// The RTL computes SATD for 9 candidates candi0..candi8.
std::uint32_t calculate_block_satd(const frame& input,
                                   const block& current,
                                   const std::vector<std::uint8_t>& predicted)
{
    std::uint64_t total = 0;

    for (std::uint32_t by = 0; by < current.height; by += 8u) {
        for (std::uint32_t bx = 0; bx < current.width; bx += 8u) {
            std::array<int, 64> residual {};

            for (std::uint32_t y = 0; y < 8u; ++y) {
                for (std::uint32_t x = 0; x < 8u; ++x) {
                    const std::uint32_t lx = bx + x;
                    const std::uint32_t ly = by + y;

                    int diff = 0;

                    if (lx < current.width && ly < current.height) {
                        const int original =
                            read_luma_clamped(input,
                                              static_cast<int>(current.x + lx),
                                              static_cast<int>(current.y + ly));

                        const std::size_t idx =
                            static_cast<std::size_t>(ly * current.width + lx);

                        const int pred =
                            idx < predicted.size()
                                ? static_cast<int>(predicted[idx])
                                : 128;

                        diff = original - pred;
                    }

                    residual[static_cast<std::size_t>(y * 8u + x)] = diff;
                }
            }

            total += satd_8x8(residual);
        }
    }

    return clamp_u32(total);
}

std::uint32_t lambda_from_qp(std::uint32_t qp)
{
    if (qp <= 15u) return 1u;
    if (qp <= 19u) return 2u;
    if (qp <= 22u) return 3u;
    if (qp <= 25u) return 4u;
    if (qp == 26u) return 5u;
    if (qp <= 28u) return 6u;
    if (qp == 29u) return 7u;
    if (qp == 30u) return 8u;
    if (qp == 31u) return 9u;
    if (qp == 32u) return 10u;
    if (qp == 33u) return 11u;
    if (qp == 34u) return 13u;
    if (qp == 35u) return 14u;
    if (qp == 36u) return 16u;
    if (qp == 37u) return 18u;
    if (qp == 38u) return 20u;
    if (qp == 39u) return 23u;
    if (qp == 40u) return 25u;
    if (qp == 41u) return 29u;
    if (qp == 42u) return 32u;
    if (qp == 43u) return 36u;
    if (qp == 44u) return 40u;
    if (qp == 45u) return 45u;
    if (qp == 46u) return 51u;
    if (qp == 47u) return 57u;
    if (qp == 48u) return 64u;
    if (qp == 49u) return 72u;
    if (qp == 50u) return 81u;
    return 91u;
}

std::uint32_t exp_golomb_signed_bits(int value)
{
    const std::uint32_t abs_value =
        static_cast<std::uint32_t>(abs_int(value));

    const std::uint32_t code_num =
        value <= 0 ? abs_value << 1u : (abs_value << 1u) - 1u;

    std::uint32_t tmp = code_num + 1u;
    std::uint32_t leading_bits = 0;

    while (tmp > 1u) {
        tmp >>= 1u;
        ++leading_bits;
    }

    return leading_bits * 2u + 1u;
}

// TLM equivalent of fme_cost + bits_num/getbits.
// Cost = SATD + lambda * MV bit cost.
std::uint32_t estimate_mv_rate(const motion_vector& mv)
{
    return exp_golomb_signed_bits(mv.x) +
           exp_golomb_signed_bits(mv.y);
}

fme_candidate evaluate_candidate(const frame& input,
                                 const frame& reference,
                                 const block& current,
                                 const motion_vector& mv,
                                 std::uint32_t qp)
{
    fme_candidate candidate;
    candidate.valid = false;
    candidate.mv = mv;

    if (input.empty() || reference.empty() || current.area() == 0) {
        return candidate;
    }

    candidate.predicted_luma =
        make_prediction_block(reference, current, mv);

    candidate.satd =
        calculate_block_satd(input, current, candidate.predicted_luma);

    candidate.rate =
        estimate_mv_rate(mv);

    candidate.cost =
        clamp_u32(static_cast<std::uint64_t>(candidate.satd) +
                  static_cast<std::uint64_t>(lambda_from_qp(qp)) *
                  static_cast<std::uint64_t>(candidate.rate));

    candidate.valid = true;
    return candidate;
}

std::array<motion_vector, 9> make_3x3_candidates(const motion_vector& center,
                                                 int step_qpel)
{
    std::array<motion_vector, 9> candidates {};

    std::size_t index = 0;

    for (int dy = -step_qpel; dy <= step_qpel; dy += step_qpel) {
        for (int dx = -step_qpel; dx <= step_qpel; dx += step_qpel) {
            motion_vector mv;
            mv.x = center.x + dx;
            mv.y = center.y + dy;

            candidates[index++] = mv;
        }
    }

    return candidates;
}

fme_candidate search_3x3(const frame& input,
                         const frame& reference,
                         const block& current,
                         const motion_vector& center,
                         int step_qpel,
                         std::uint32_t qp)
{
    fme_candidate best;
    best.valid = false;
    best.cost = std::numeric_limits<std::uint32_t>::max();

    const std::array<motion_vector, 9> candidates =
        make_3x3_candidates(center, step_qpel);

    for (const motion_vector& mv : candidates) {
        fme_candidate candidate =
            evaluate_candidate(input, reference, current, mv, qp);

        if (candidate.valid &&
            (!best.valid || candidate.cost < best.cost)) {
            best = std::move(candidate);
        }
    }

    return best;
}

std::uint32_t skip_threshold_for_block(const block& current)
{
    const std::uint32_t size = std::min(current.width, current.height);

    if (size <= 8u) {
        return 4000u;
    }

    if (size <= 16u) {
        return 14000u;
    }

    if (size <= 32u) {
        return 49000u;
    }

    return 171500u;
}

// TLM equivalent of fme_skip:
// simple skip/merge check using final cost against block-size threshold.
bool is_skip_candidate(const fme_candidate& best,
                       const block& current)
{
    if (!best.valid) {
        return false;
    }

    const bool square = current.width == current.height;
    const bool zero_or_small_mv =
        abs_int(best.mv.x) <= 1 && abs_int(best.mv.y) <= 1;

    return square &&
           zero_or_small_mv &&
           best.cost < skip_threshold_for_block(current);
}

partition_mode choose_partition_from_ime(const ime_result& ime_info)
{
    if (ime_info.valid) {
        return ime_info.best_partition;
    }

    return partition_mode::part_2nx2n;
}

motion_vector choose_integer_mv_from_ime(const ime_result& ime_info)
{
    if (ime_info.valid) {
        return ime_info.best_mv;
    }

    motion_vector mv;
    mv.x = 0;
    mv.y = 0;
    return mv;
}

prediction_result make_inter_prediction_result(const block& current,
                                               const fme_candidate& best,
                                               partition_mode partition,
                                               std::uint32_t qp)
{
    prediction_result result;

    result.valid = best.valid;
    result.mode = prediction_mode::inter;
    result.partition = partition;
    result.mv = best.mv;
    result.qp = qp;
    result.distortion = best.satd;
    result.rate = best.rate;
    result.cost = best.cost;
    result.predicted_luma = best.predicted_luma;

    return result;
}

// Main RTL-level TLM FME flow:
// 1. read integer MV from IME
// 2. half-pixel 3x3 refinement
// 3. quarter-pixel 3x3 refinement around best half-pel MV
// 4. generate prediction pixels
// 5. optional skip decision
prediction_result refine_inter_prediction(const frame& input,
                                          const frame& reference,
                                          const block& current,
                                          const ime_result& ime_info,
                                          std::uint32_t qp)
{
    const motion_vector integer_mv =
        choose_integer_mv_from_ime(ime_info);

    const partition_mode partition =
        choose_partition_from_ime(ime_info);

    fme_candidate half_best =
        search_3x3(input,
                   reference,
                   current,
                   integer_mv,
                   2,
                   qp);

    if (!half_best.valid) {
        return prediction_result::invalid();
    }

    fme_candidate quarter_best =
        search_3x3(input,
                   reference,
                   current,
                   half_best.mv,
                   1,
                   qp);

    if (!quarter_best.valid) {
        return prediction_result::invalid();
    }

    prediction_result refined =
        make_inter_prediction_result(current,
                                     quarter_best,
                                     partition,
                                     qp);

    // Keep refined MV as output even if skip is detected.
    // Skip flag/index are stored in fme_result when the header supports them.
    (void)is_skip_candidate(quarter_best, current);

    return refined;
}

} // namespace


fme_result fme::run(const frame& input,
                    const frame& reference,
                    const ime_result& ime_info,
                    std::uint32_t qp) const
{
    fme_refine_config config;
    return run(input, reference, ime_info, config, qp);
}

fme_result fme::run(const frame& input,
                    const frame& reference,
                    const ime_result& ime_info,
                    const fme_refine_config& config,
                    std::uint32_t qp) const
{
    (void)config;

    fme_result result;

    const block ctu = ime_info.ctu;

    if (input.empty() || reference.empty() || !ime_info.valid || ctu.area() == 0) {
        result.valid = false;
        return result;
    }

    const std::uint32_t clamped_qp =
        std::clamp(qp, MIN_QP, MAX_QP);

    prediction_result refined =
        refine_inter_prediction(input,
                                reference,
                                ctu,
                                ime_info,
                                clamped_qp);

    if (!refined.valid) {
        result.valid = false;
        return result;
    }

    result.valid = true;
    result.ctu = ctu;
    result.qp = clamped_qp;
    result.best_inter_result = refined;
    result.best_cost = refined.cost;

    return result;
}

} // namespace cdc::components
