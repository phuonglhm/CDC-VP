#include "fme.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace cdc::components {

namespace {

int abs_int(int value)
{
    return value < 0 ? -value : value;
}

std::uint32_t clamp_cost_u32(std::uint64_t value)
{
    return value > std::numeric_limits<std::uint32_t>::max()
               ? std::numeric_limits<std::uint32_t>::max()
               : static_cast<std::uint32_t>(value);
}

std::uint8_t clamp_u8(int value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

bool is_integer_pel(motion_vector mv)
{
    return (mv.x % 4 == 0) && (mv.y % 4 == 0);
}

bool is_half_pel(motion_vector mv)
{
    const bool x_half = (abs_int(mv.x) % 4) == 2;
    const bool y_half = (abs_int(mv.y) % 4) == 2;
    return x_half || y_half;
}

int floor_div4(int value)
{
    if (value >= 0) {
        return value / 4;
    }

    return -(((-value) + 3) / 4);
}

int floor_mod4(int value)
{
    const int base = floor_div4(value);
    return value - base * 4;
}

} // namespace

prediction_result fme::refine(const prediction_result& ime_prediction) const
{
    // Compatibility path only.
    // Without input/reference frames, real fractional interpolation cannot run.
    if (!ime_prediction.valid || ime_prediction.mode != prediction_mode::inter) {
        return prediction_result::invalid();
    }

    return ime_prediction;
}

prediction_result fme::refine(const frame& input,
                              const frame& reference,
                              const block& region,
                              const prediction_result& ime_prediction,
                              std::uint32_t qp) const
{
    if (!ime_prediction.valid || ime_prediction.mode != prediction_mode::inter) {
        return prediction_result::invalid();
    }

    ime_candidate base_candidate;
    base_candidate.valid = true;
    base_candidate.pu = region;
    base_candidate.partition = ime_prediction.partition;
    base_candidate.mv = ime_prediction.mv;
    base_candidate.sad = ime_prediction.distortion;
    base_candidate.rate = ime_prediction.rate;
    base_candidate.cost = ime_prediction.cost;

    ime_result ime_info;
    ime_info.valid = true;
    ime_info.ctu = region;
    ime_info.qp = qp;
    ime_info.best_partition = ime_prediction.partition;
    ime_info.best_mv = ime_prediction.mv;
    ime_info.best_cost = ime_prediction.cost;
    ime_info.best_inter_result = ime_prediction;

    // Reuse IME candidate container shape.
    ime_candidate tmp;
    tmp.valid = true;
    tmp.pu = region;
    tmp.partition = ime_prediction.partition;
    tmp.mv = ime_prediction.mv;
    tmp.sad = ime_prediction.distortion;
    tmp.rate = ime_prediction.rate;
    tmp.cost = ime_prediction.cost;

    ime_info.candidates.push_back(tmp);

    fme_result result = run(input, reference, ime_info, qp);
    return result.best_inter_result;
}

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
    if (input.empty() || reference.empty() || !ime_info.valid) {
        return fme_result::invalid();
    }

    qp = clamp_qp(qp);

    fme_result result;
    result.valid = true;
    result.ctu = ime_info.ctu;
    result.qp = qp;

    std::vector<fme_candidate> prepared =
        prepare_candidates(ime_info, config);

    for (const fme_candidate& seed : prepared) {
        if (!seed.valid) {
            continue;
        }

        fme_candidate evaluated =
            evaluate_candidate(input,
                               reference,
                               seed.pu,
                               seed.partition,
                               seed.mv,
                               qp,
                               config);

        if (evaluated.valid) {
            result.candidates.push_back(std::move(evaluated));
        }
    }

    if (result.candidates.empty()) {
        return fme_result::invalid();
    }

    fme_candidate best =
        select_best_candidate(result.candidates);

    if (config.enable_skip_decision) {
        best.skip = detect_skip(best, best.pu);
    }

    result.best_partition = best.partition;
    result.best_mv = best.mv;
    result.best_satd = best.satd;
    result.best_rate = best.rate;
    result.best_cost = best.cost;
    result.skip = best.skip;
    result.best_inter_result =
        make_prediction_result(input, reference, best, qp, config);

    return result;
}

std::vector<fme_candidate> fme::prepare_candidates(
    const ime_result& ime_info,
    const fme_refine_config& config) const
{
    std::vector<fme_candidate> candidates;

    std::vector<ime_candidate> ime_seeds = ime_info.candidates;

    if (ime_seeds.empty() && ime_info.best_inter_result.valid) {
        ime_candidate seed;
        seed.valid = true;
        seed.pu = ime_info.ctu;
        seed.partition = ime_info.best_inter_result.partition;
        seed.mv = ime_info.best_inter_result.mv;
        seed.sad = ime_info.best_inter_result.distortion;
        seed.rate = ime_info.best_inter_result.rate;
        seed.cost = ime_info.best_inter_result.cost;
        ime_seeds.push_back(seed);
    }

    const std::vector<motion_vector> half_offsets =
        build_half_pel_offsets(config);

    const std::vector<motion_vector> quarter_offsets =
        build_quarter_pel_offsets(config);

    for (const ime_candidate& seed : ime_seeds) {
        if (!seed.valid) {
            continue;
        }

        // Always include IME integer-pel MV.
        fme_candidate center;
        center.valid = true;
        center.pu = seed.pu;
        center.partition = seed.partition;
        center.mv = seed.mv;
        center.half_pel = false;
        center.quarter_pel = false;
        candidates.push_back(center);

        if (config.enable_half_pel) {
            for (const motion_vector& offset : half_offsets) {
                fme_candidate candidate;
                candidate.valid = true;
                candidate.pu = seed.pu;
                candidate.partition = seed.partition;
                candidate.mv = motion_vector {
                    seed.mv.x + offset.x,
                    seed.mv.y + offset.y
                };
                candidate.half_pel = is_half_pel(candidate.mv);
                candidate.quarter_pel = false;
                candidates.push_back(candidate);
            }
        }

        if (config.enable_quarter_pel) {
            for (const motion_vector& offset : quarter_offsets) {
                fme_candidate candidate;
                candidate.valid = true;
                candidate.pu = seed.pu;
                candidate.partition = seed.partition;
                candidate.mv = motion_vector {
                    seed.mv.x + offset.x,
                    seed.mv.y + offset.y
                };
                candidate.half_pel = is_half_pel(candidate.mv);
                candidate.quarter_pel = !is_integer_pel(candidate.mv);
                candidates.push_back(candidate);
            }
        }
    }

    return candidates;
}

std::vector<motion_vector> fme::build_half_pel_offsets(
    const fme_refine_config& config) const
{
    std::vector<motion_vector> offsets;

    const int radius =
        std::max(0, static_cast<int>(config.half_pel_radius_qpel));

    for (int y = -radius; y <= radius; y += 2) {
        for (int x = -radius; x <= radius; x += 2) {
            if (x == 0 && y == 0) {
                continue;
            }

            offsets.push_back(motion_vector {x, y});
        }
    }

    return offsets;
}

std::vector<motion_vector> fme::build_quarter_pel_offsets(
    const fme_refine_config& config) const
{
    std::vector<motion_vector> offsets;

    const int radius =
        std::max(0, static_cast<int>(config.quarter_pel_radius_qpel));

    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            if (x == 0 && y == 0) {
                continue;
            }

            offsets.push_back(motion_vector {x, y});
        }
    }

    return offsets;
}

fme_candidate fme::evaluate_candidate(const frame& input,
                                      const frame& reference,
                                      const block& pu,
                                      partition_mode partition,
                                      motion_vector mv,
                                      std::uint32_t qp,
                                      const fme_refine_config& config) const
{
    fme_candidate candidate;
    candidate.valid = false;
    candidate.pu = pu;
    candidate.partition = partition;
    candidate.mv = mv;
    candidate.half_pel = is_half_pel(mv);
    candidate.quarter_pel = !is_integer_pel(mv);

    if (pu.area() == 0) {
        return candidate;
    }

    candidate.satd =
        compute_satd_and_prediction(input,
                                    reference,
                                    pu,
                                    mv,
                                    config,
                                    nullptr,
                                    nullptr);

    candidate.rate = estimate_mv_rate(mv, partition);
    candidate.cost = compute_cost(candidate.satd, candidate.rate, qp);
    candidate.valid = true;

    return candidate;
}

prediction_result fme::make_prediction_result(const frame& input,
                                              const frame& reference,
                                              const fme_candidate& best,
                                              std::uint32_t qp,
                                              const fme_refine_config& config) const
{
    if (!best.valid) {
        return prediction_result::invalid();
    }

    std::vector<std::uint8_t> predicted;
    std::vector<std::int16_t> residual;

    const std::uint32_t satd =
        compute_satd_and_prediction(input,
                                    reference,
                                    best.pu,
                                    best.mv,
                                    config,
                                    &predicted,
                                    &residual);

    prediction_result result = prediction_result::make_inter(
        best.cost,
        best.mv,
        best.partition,
        qp
    );

    result.valid = true;
    result.rate = best.rate;
    result.distortion = satd;
    result.predicted_luma = std::move(predicted);
    result.residual_luma = std::move(residual);
    result.skip = best.skip;

    return result;
}

std::uint32_t fme::compute_satd_and_prediction(
    const frame& input,
    const frame& reference,
    const block& pu,
    motion_vector mv,
    const fme_refine_config& config,
    std::vector<std::uint8_t>* predicted_out,
    std::vector<std::int16_t>* residual_out) const
{
    std::vector<std::int16_t> local_residual;
    local_residual.reserve(pu.area());

    if (predicted_out != nullptr) {
        predicted_out->clear();
        predicted_out->reserve(pu.area());
    }

    if (residual_out != nullptr) {
        residual_out->clear();
        residual_out->reserve(pu.area());
    }

    for (std::uint32_t y = 0; y < pu.height; ++y) {
        for (std::uint32_t x = 0; x < pu.width; ++x) {
            const std::uint32_t cur_x = pu.x + x;
            const std::uint32_t cur_y = pu.y + y;

            const std::uint8_t original =
                input.get_luma(cur_x, cur_y);

            const std::uint8_t predicted =
                interpolate_luma_qpel(reference,
                                      static_cast<int>(cur_x),
                                      static_cast<int>(cur_y),
                                      mv,
                                      config);

            const int diff =
                static_cast<int>(original) - static_cast<int>(predicted);

            const auto residual =
                static_cast<std::int16_t>(diff);

            local_residual.push_back(residual);

            if (predicted_out != nullptr) {
                predicted_out->push_back(predicted);
            }

            if (residual_out != nullptr) {
                residual_out->push_back(residual);
            }
        }
    }

    return compute_satd_4x4_blocks(local_residual, pu.width, pu.height);
}

std::uint32_t fme::compute_satd_4x4_blocks(
    const std::vector<std::int16_t>& residual,
    std::uint32_t width,
    std::uint32_t height) const
{
    if (width == 0 || height == 0 || residual.empty()) {
        return 0;
    }

    std::uint64_t satd = 0;

    for (std::uint32_t y = 0; y < height; y += 4) {
        for (std::uint32_t x = 0; x < width; x += 4) {
            if (x + 3 < width && y + 3 < height) {
                satd += hadamard_4x4_satd(residual, width, x, y);
            } else {
                for (std::uint32_t yy = y; yy < std::min(y + 4, height); ++yy) {
                    for (std::uint32_t xx = x; xx < std::min(x + 4, width); ++xx) {
                        const std::size_t idx =
                            static_cast<std::size_t>(yy) * width + xx;

                        if (idx < residual.size()) {
                            satd += static_cast<std::uint32_t>(
                                abs_int(residual[idx])
                            );
                        }
                    }
                }
            }
        }
    }

    return clamp_cost_u32(satd);
}

std::uint32_t fme::hadamard_4x4_satd(
    const std::vector<std::int16_t>& residual,
    std::uint32_t width,
    std::uint32_t start_x,
    std::uint32_t start_y) const
{
    int m[4][4];
    int d[4][4];

    for (int y = 0; y < 4; ++y) {
        const std::size_t base =
            static_cast<std::size_t>(start_y + y) * width + start_x;

        const int s0 = residual[base + 0];
        const int s1 = residual[base + 1];
        const int s2 = residual[base + 2];
        const int s3 = residual[base + 3];

        const int a0 = s0 + s3;
        const int a1 = s1 + s2;
        const int a2 = s1 - s2;
        const int a3 = s0 - s3;

        m[y][0] = a0 + a1;
        m[y][1] = a3 + a2;
        m[y][2] = a0 - a1;
        m[y][3] = a3 - a2;
    }

    std::uint64_t sum = 0;

    for (int x = 0; x < 4; ++x) {
        const int s0 = m[0][x];
        const int s1 = m[1][x];
        const int s2 = m[2][x];
        const int s3 = m[3][x];

        const int a0 = s0 + s3;
        const int a1 = s1 + s2;
        const int a2 = s1 - s2;
        const int a3 = s0 - s3;

        d[0][x] = a0 + a1;
        d[1][x] = a3 + a2;
        d[2][x] = a0 - a1;
        d[3][x] = a3 - a2;
    }

    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            sum += static_cast<std::uint32_t>(abs_int(d[y][x]));
        }
    }

    return clamp_cost_u32((sum + 1u) >> 1u);
}

std::uint8_t fme::interpolate_luma_qpel(const frame& reference,
                                        int x,
                                        int y,
                                        motion_vector mv,
                                        const fme_refine_config& config) const
{
    const int sample_x_qpel = x * 4 + mv.x;
    const int sample_y_qpel = y * 4 + mv.y;

    if (config.use_hevc_luma_filter) {
        return interpolate_luma_hevc_filter(reference,
                                            sample_x_qpel,
                                            sample_y_qpel);
    }

    return interpolate_luma_bilinear(reference,
                                     sample_x_qpel,
                                     sample_y_qpel);
}

std::uint8_t fme::interpolate_luma_hevc_filter(
    const frame& reference,
    int sample_x_qpel,
    int sample_y_qpel) const
{
    // HEVC luma 8-tap interpolation coefficients for qpel phases.
    static constexpr int coeff[4][8] = {
        {  0,  0,   0, 64,  0,   0,  0,  0 },
        { -1,  4, -10, 58, 17,  -5,  1,  0 },
        { -1,  4, -11, 40, 40, -11,  4, -1 },
        {  0,  1,  -5, 17, 58, -10,  4, -1 }
    };

    const int base_x = floor_div4(sample_x_qpel);
    const int base_y = floor_div4(sample_y_qpel);

    const int phase_x = floor_mod4(sample_x_qpel);
    const int phase_y = floor_mod4(sample_y_qpel);

    if (phase_x == 0 && phase_y == 0) {
        return sample_reference_luma(reference, base_x, base_y);
    }

    if (phase_y == 0) {
        int sum = 0;

        for (int k = 0; k < 8; ++k) {
            const int px = base_x + k - 3;
            sum += coeff[phase_x][k] *
                   static_cast<int>(sample_reference_luma(reference, px, base_y));
        }

        return clamp_u8((sum + 32) >> 6);
    }

    if (phase_x == 0) {
        int sum = 0;

        for (int k = 0; k < 8; ++k) {
            const int py = base_y + k - 3;
            sum += coeff[phase_y][k] *
                   static_cast<int>(sample_reference_luma(reference, base_x, py));
        }

        return clamp_u8((sum + 32) >> 6);
    }

    // Separable horizontal then vertical filtering.
    int intermediate[8] = {0};

    for (int j = 0; j < 8; ++j) {
        const int py = base_y + j - 3;
        int hsum = 0;

        for (int k = 0; k < 8; ++k) {
            const int px = base_x + k - 3;
            hsum += coeff[phase_x][k] *
                    static_cast<int>(sample_reference_luma(reference, px, py));
        }

        intermediate[j] = hsum;
    }

    int vsum = 0;

    for (int k = 0; k < 8; ++k) {
        vsum += coeff[phase_y][k] * intermediate[k];
    }

    return clamp_u8((vsum + 2048) >> 12);
}

std::uint8_t fme::interpolate_luma_bilinear(const frame& reference,
                                            int sample_x_qpel,
                                            int sample_y_qpel) const
{
    const int base_x = floor_div4(sample_x_qpel);
    const int base_y = floor_div4(sample_y_qpel);

    const int phase_x = floor_mod4(sample_x_qpel);
    const int phase_y = floor_mod4(sample_y_qpel);

    const int p00 = sample_reference_luma(reference, base_x,     base_y);
    const int p10 = sample_reference_luma(reference, base_x + 1, base_y);
    const int p01 = sample_reference_luma(reference, base_x,     base_y + 1);
    const int p11 = sample_reference_luma(reference, base_x + 1, base_y + 1);

    const int w00 = (4 - phase_x) * (4 - phase_y);
    const int w10 = phase_x * (4 - phase_y);
    const int w01 = (4 - phase_x) * phase_y;
    const int w11 = phase_x * phase_y;

    const int value =
        (p00 * w00 + p10 * w10 + p01 * w01 + p11 * w11 + 8) >> 4;

    return clamp_u8(value);
}

std::uint8_t fme::sample_reference_luma(const frame& reference,
                                        int x,
                                        int y) const
{
    if (reference.empty()) {
        return 0;
    }

    const int max_x = static_cast<int>(reference.width) - 1;
    const int max_y = static_cast<int>(reference.height) - 1;

    const int cx = std::clamp(x, 0, max_x);
    const int cy = std::clamp(y, 0, max_y);

    return reference.get_luma(static_cast<std::uint32_t>(cx),
                              static_cast<std::uint32_t>(cy));
}

std::uint32_t fme::estimate_mv_rate(motion_vector mv,
                                    partition_mode partition) const
{
    std::uint32_t bits = 0;

    bits += signed_exp_golomb_bits(mv.x);
    bits += signed_exp_golomb_bits(mv.y);

    switch (partition) {
    case partition_mode::part_2nx2n:
        bits += 1;
        break;
    case partition_mode::part_2nxn:
    case partition_mode::part_nx2n:
        bits += 3;
        break;
    case partition_mode::part_split:
        bits += 5;
        break;
    default:
        bits += 3;
        break;
    }

    return bits;
}

std::uint32_t fme::signed_exp_golomb_bits(int value) const
{
    const std::uint32_t code_num =
        value <= 0
            ? static_cast<std::uint32_t>(-2 * value)
            : static_cast<std::uint32_t>(2 * value - 1);

    return unsigned_exp_golomb_bits(code_num);
}

std::uint32_t fme::unsigned_exp_golomb_bits(std::uint32_t value) const
{
    const std::uint32_t code_num = value + 1u;

    std::uint32_t leading_bits = 0;
    std::uint32_t temp = code_num;

    while (temp > 1u) {
        temp >>= 1u;
        ++leading_bits;
    }

    return leading_bits * 2u + 1u;
}

std::uint32_t fme::lambda_from_qp(std::uint32_t qp) const
{
    qp = clamp_qp(qp);

    // Integer lambda scaled by 256.
    // Common HEVC-like RD relation: lambda grows exponentially with QP.
    const double lambda =
        0.57 * std::pow(2.0, (static_cast<double>(qp) - 12.0) / 3.0);

    const double scaled = lambda * 256.0;

    if (scaled < 1.0) {
        return 1u;
    }

    if (scaled > static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
        return std::numeric_limits<std::uint32_t>::max();
    }

    return static_cast<std::uint32_t>(scaled);
}

std::uint32_t fme::compute_cost(std::uint32_t satd,
                                std::uint32_t rate,
                                std::uint32_t qp) const
{
    const std::uint32_t lambda = lambda_from_qp(qp);

    const std::uint64_t rate_cost =
        (static_cast<std::uint64_t>(lambda) * rate + 128u) >> 8u;

    return clamp_cost_u32(static_cast<std::uint64_t>(satd) + rate_cost);
}

bool fme::detect_skip(const fme_candidate& candidate,
                      const block& pu) const
{
    if (!candidate.valid || pu.area() == 0) {
        return false;
    }

    const bool small_residual =
        candidate.satd <= pu.area();

    const bool small_mv =
        abs_int(candidate.mv.x) <= 1 &&
        abs_int(candidate.mv.y) <= 1;

    return small_residual && small_mv;
}

fme_candidate fme::select_best_candidate(
    const std::vector<fme_candidate>& candidates) const
{
    fme_candidate best;

    for (const fme_candidate& candidate : candidates) {
        if (!candidate.valid) {
            continue;
        }

        if (!best.valid || candidate.cost < best.cost) {
            best = candidate;
        }
    }

    return best;
}

std::uint32_t fme::clamp_qp(std::uint32_t qp) const
{
    return std::clamp(qp, MIN_QP, MAX_QP);
}

} // namespace cdc::components
