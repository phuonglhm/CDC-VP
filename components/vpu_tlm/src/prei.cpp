#include "prei.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace cdc::components {

namespace {

int abs_int(int value)
{
    return value < 0 ? -value : value;
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

std::uint32_t safe_add_cost(std::uint32_t a, std::uint32_t b)
{
    const std::uint64_t sum = static_cast<std::uint64_t>(a) + b;
    return sum > std::numeric_limits<std::uint32_t>::max()
               ? std::numeric_limits<std::uint32_t>::max()
               : static_cast<std::uint32_t>(sum);
}

} // namespace

prei_result prei::run(const frame& input,
                      const block& ctu) const
{
    prei_rate_control_config rc_config;
    rc_config.initial_qp = INIT_QP;
    rc_config.min_qp = MIN_QP;
    rc_config.max_qp = MAX_QP;
    rc_config.delta_qp = 0;
    rc_config.lcu_rc_enable = false;

    return run(input, ctu, rc_config);
}

prei_result prei::run(const frame& input,
                      const block& ctu,
                      const prei_rate_control_config& rc_config) const
{
    if (input.empty() || ctu.area() == 0) {
        return prei_result::invalid();
    }

    prei_result result;
    result.valid = true;
    result.ctu = ctu;

    run_mode_decision(input, ctu, result);

    result.modebest64_sum = calculate_modebest64_sum(result);
    result.qp = run_rate_control(ctu, result, rc_config);

    if (!result.mode_entries.empty()) {
        const auto best_it = std::min_element(
            result.mode_entries.begin(),
            result.mode_entries.end(),
            [](const prei_mode_entry& a, const prei_mode_entry& b) {
                return a.best_cost < b.best_cost;
            }
        );

        if (best_it != result.mode_entries.end() && best_it->valid) {
            result.best_intra_hint = prediction_result::make_intra(
                best_it->best_cost,
                best_it->best_mode,
                partition_mode::part_2nx2n,
                result.qp
            );

            result.best_intra_hint.valid = true;
            result.best_intra_hint.rate = estimate_mode_rate(
                best_it->best_mode_index,
                best_it->cu
            );
            result.best_intra_hint.distortion = best_it->activity;
        }
    }

    return result;
}

void prei::run_mode_decision(const frame& input,
                             const block& ctu,
                             prei_result& result) const
{
    result.mode_entries.clear();

    const std::vector<block> cu_list = build_cu_list(ctu);

    std::uint32_t qp_for_md = result.qp;
    if (qp_for_md < MIN_QP || qp_for_md > MAX_QP) {
        qp_for_md = INIT_QP;
    }

    for (const block& cu : cu_list) {
        prei_mode_entry entry = evaluate_cu_modes(input, cu, qp_for_md);

        if (entry.valid) {
            result.mode_entries.push_back(std::move(entry));
        }
    }
}

std::vector<block> prei::build_cu_list(const block& ctu) const
{
    std::vector<block> list;

    const std::uint32_t sizes[] = {
        CTU_SIZE,
        32u,
        16u,
        8u
    };

    for (std::uint32_t size : sizes) {
        if (size > ctu.width || size > ctu.height) {
            continue;
        }

        std::uint32_t depth = 0;
        if (size == 32u) {
            depth = 1;
        } else if (size == 16u) {
            depth = 2;
        } else if (size == 8u) {
            depth = 3;
        }

        for (std::uint32_t y = ctu.y; y < ctu.bottom(); y += size) {
            for (std::uint32_t x = ctu.x; x < ctu.right(); x += size) {
                const std::uint32_t width =
                    std::min(size, ctu.right() - x);
                const std::uint32_t height =
                    std::min(size, ctu.bottom() - y);

                if (width == 0 || height == 0) {
                    continue;
                }

                block cu {
                    x,
                    y,
                    width,
                    height,
                    block_type::cu,
                    depth
                };

                list.push_back(cu);
            }
        }
    }

    return list;
}

prei_mode_entry prei::evaluate_cu_modes(const frame& input,
                                        const block& cu,
                                        std::uint32_t qp) const
{
    prei_mode_entry entry;
    entry.valid = true;
    entry.cu = cu;
    entry.candidates.reserve(NUM_INTRA_MODES);

    std::uint32_t best_cost = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t best_mode_index = INTRA_DC_MODE;
    intra_prediction_mode best_mode = intra_prediction_mode::dc;
    std::uint32_t best_activity = 0;

    for (std::uint32_t mode_index = 0;
         mode_index < NUM_INTRA_MODES;
         ++mode_index) {
        prei_mode_candidate candidate =
            evaluate_intra_mode(input, cu, mode_index, qp);

        if (candidate.valid && candidate.cost < best_cost) {
            best_cost = candidate.cost;
            best_mode_index = candidate.mode_index;
            best_mode = candidate.mapped_mode;
            best_activity = candidate.distortion;
        }

        entry.candidates.push_back(candidate);
    }

    entry.best_mode_index = best_mode_index;
    entry.best_mode = best_mode;
    entry.best_cost = best_cost;
    entry.activity = best_activity;

    return entry;
}

prei_mode_candidate prei::evaluate_intra_mode(const frame& input,
                                              const block& cu,
                                              std::uint32_t mode_index,
                                              std::uint32_t qp) const
{
    prei_mode_candidate candidate;
    candidate.valid = false;

    if (mode_index >= NUM_INTRA_MODES || cu.area() == 0 || input.empty()) {
        return candidate;
    }

    std::uint64_t distortion = 0;

    for (std::uint32_t by = 0; by < cu.height; ++by) {
        for (std::uint32_t bx = 0; bx < cu.width; ++bx) {
            const std::uint8_t original =
                input.get_luma(cu.x + bx, cu.y + by);

            const std::uint8_t predicted =
                predict_intra_sample(input, cu, bx, by, mode_index);

            distortion += static_cast<std::uint32_t>(
                abs_int(static_cast<int>(original) -
                        static_cast<int>(predicted))
            );
        }
    }

    const std::uint32_t distortion32 =
        distortion > std::numeric_limits<std::uint32_t>::max()
            ? std::numeric_limits<std::uint32_t>::max()
            : static_cast<std::uint32_t>(distortion);

    const std::uint32_t rate = estimate_mode_rate(mode_index, cu);

    // TLM RD-style cost:
    // cost = distortion + lambda(QP) * rate
    const std::uint32_t lambda = 1u + qp / 6u;
    const std::uint32_t rate_cost = rate * lambda;

    candidate.valid = true;
    candidate.mode_index = mode_index;
    candidate.mapped_mode = map_intra_mode(mode_index);
    candidate.distortion = distortion32;
    candidate.rate = rate;
    candidate.cost = safe_add_cost(distortion32, rate_cost);

    return candidate;
}

std::uint8_t prei::predict_intra_sample(const frame& input,
                                        const block& cu,
                                        std::uint32_t local_x,
                                        std::uint32_t local_y,
                                        std::uint32_t mode_index) const
{
    if (mode_index == INTRA_PLANAR_MODE) {
        return predict_planar_sample(input, cu, local_x, local_y);
    }

    if (mode_index == INTRA_DC_MODE) {
        return predict_dc_sample(input, cu);
    }

    return predict_angular_sample(input, cu, local_x, local_y, mode_index);
}

std::uint8_t prei::predict_planar_sample(const frame& input,
                                         const block& cu,
                                         std::uint32_t local_x,
                                         std::uint32_t local_y) const
{
    const int x = static_cast<int>(cu.x + local_x);
    const int y = static_cast<int>(cu.y + local_y);

    const int left =
        read_luma_clamped(input, static_cast<int>(cu.x) - 1, y);

    const int top =
        read_luma_clamped(input, x, static_cast<int>(cu.y) - 1);

    const int top_right =
        read_luma_clamped(input,
                          static_cast<int>(cu.x + cu.width),
                          static_cast<int>(cu.y) - 1);

    const int bottom_left =
        read_luma_clamped(input,
                          static_cast<int>(cu.x) - 1,
                          static_cast<int>(cu.y + cu.height));

    const int width = static_cast<int>(cu.width);
    const int height = static_cast<int>(cu.height);

    const int lx = static_cast<int>(local_x);
    const int ly = static_cast<int>(local_y);

    const int hor =
        (width - 1 - lx) * left + (lx + 1) * top_right;

    const int ver =
        (height - 1 - ly) * top + (ly + 1) * bottom_left;

    const int denom = std::max(1, width + height);
    const int value = (hor + ver + denom / 2) / denom;

    return clamp_u8(value);
}

std::uint8_t prei::predict_dc_sample(const frame& input,
                                     const block& cu) const
{
    std::uint32_t sum = 0;
    std::uint32_t count = 0;

    if (cu.y > 0) {
        for (std::uint32_t bx = 0; bx < cu.width; ++bx) {
            sum += read_luma_clamped(input,
                                     static_cast<int>(cu.x + bx),
                                     static_cast<int>(cu.y) - 1);
            ++count;
        }
    }

    if (cu.x > 0) {
        for (std::uint32_t by = 0; by < cu.height; ++by) {
            sum += read_luma_clamped(input,
                                     static_cast<int>(cu.x) - 1,
                                     static_cast<int>(cu.y + by));
            ++count;
        }
    }

    if (count == 0) {
        for (std::uint32_t by = 0; by < cu.height; ++by) {
            for (std::uint32_t bx = 0; bx < cu.width; ++bx) {
                sum += input.get_luma(cu.x + bx, cu.y + by);
                ++count;
            }
        }
    }

    if (count == 0) {
        return 128;
    }

    return static_cast<std::uint8_t>(sum / count);
}

std::uint8_t prei::predict_angular_sample(const frame& input,
                                          const block& cu,
                                          std::uint32_t local_x,
                                          std::uint32_t local_y,
                                          std::uint32_t mode_index) const
{
    const int mode = static_cast<int>(mode_index);
    const int center_mode = 18;
    const int delta = mode - center_mode;

    const int x = static_cast<int>(cu.x + local_x);
    const int y = static_cast<int>(cu.y + local_y);

    // TLM directional approximation for HEVC angular modes.
    // Modes below center lean toward horizontal reference.
    // Modes above center lean toward vertical reference.
    if (mode < center_mode) {
        const int projected_y =
            y + (delta * static_cast<int>(local_x + 1)) / 8;

        return read_luma_clamped(input,
                                 static_cast<int>(cu.x) - 1,
                                 projected_y);
    }

    if (mode > center_mode) {
        const int projected_x =
            x + (delta * static_cast<int>(local_y + 1)) / 8;

        return read_luma_clamped(input,
                                 projected_x,
                                 static_cast<int>(cu.y) - 1);
    }

    const int left =
        read_luma_clamped(input, static_cast<int>(cu.x) - 1, y);

    const int top =
        read_luma_clamped(input, x, static_cast<int>(cu.y) - 1);

    return static_cast<std::uint8_t>((left + top) / 2);
}

std::uint32_t prei::estimate_mode_rate(std::uint32_t mode_index,
                                       const block& cu) const
{
    std::uint32_t rate = 1u;

    if (mode_index == INTRA_PLANAR_MODE ||
        mode_index == INTRA_DC_MODE) {
        rate += 1u;
    } else {
        rate += 3u;
    }

    if (cu.size <= 8u) {
        rate += 2u;
    } else if (cu.size <= 16u) {
        rate += 1u;
    }

    return rate;
}

std::uint32_t prei::calculate_modebest64_sum(const prei_result& result) const
{
    std::uint64_t sum = 0;

    for (const prei_mode_entry& entry : result.mode_entries) {
        if (!entry.valid) {
            continue;
        }

        sum += entry.best_cost;
    }

    return sum > std::numeric_limits<std::uint32_t>::max()
               ? std::numeric_limits<std::uint32_t>::max()
               : static_cast<std::uint32_t>(sum);
}

std::uint32_t prei::run_rate_control(const block& ctu,
                                     const prei_result& md_result,
                                     const prei_rate_control_config& rc_config) const
{
    std::uint32_t qp = rc_config.initial_qp;

    if (rc_config.delta_qp > 0) {
        qp = safe_add_cost(qp, rc_config.delta_qp);
    }

    if (rc_config.lcu_rc_enable && rc_config.target_bitnum > 0) {
        if (rc_config.actual_bitnum > rc_config.target_bitnum) {
            qp = safe_add_cost(qp, 1u);
        } else if (rc_config.actual_bitnum * 2u < rc_config.target_bitnum &&
                   qp > 0u) {
            --qp;
        }
    }

    if (md_result.modebest64_sum > ctu.area() * 32u) {
        qp = safe_add_cost(qp, 1u);
    }

    if (rc_config.roi_enable && is_inside_roi(ctu, rc_config) && qp > 0u) {
        --qp;
    }

    return clamp_qp(qp, rc_config.min_qp, rc_config.max_qp);
}

bool prei::is_inside_roi(const block& ctu,
                         const prei_rate_control_config& rc_config) const
{
    const std::uint32_t roi_right = rc_config.roi_x + rc_config.roi_width;
    const std::uint32_t roi_bottom = rc_config.roi_y + rc_config.roi_height;

    const bool overlap_x =
        ctu.x < roi_right && ctu.right() > rc_config.roi_x;

    const bool overlap_y =
        ctu.y < roi_bottom && ctu.bottom() > rc_config.roi_y;

    return overlap_x && overlap_y;
}

std::uint32_t prei::clamp_qp(std::uint32_t qp,
                             std::uint32_t min_qp,
                             std::uint32_t max_qp) const
{
    if (min_qp > max_qp) {
        min_qp = MIN_QP;
        max_qp = MAX_QP;
    }

    return std::clamp(qp, min_qp, max_qp);
}

intra_prediction_mode prei::map_intra_mode(std::uint32_t mode_index) const
{
    if (mode_index == INTRA_PLANAR_MODE) {
        return intra_prediction_mode::planar;
    }

    if (mode_index == INTRA_DC_MODE) {
        return intra_prediction_mode::dc;
    }

    if (mode_index <= 6u) {
        return intra_prediction_mode::angular_2;
    }

    if (mode_index <= 14u) {
        return intra_prediction_mode::angular_10;
    }

    if (mode_index <= 22u) {
        return intra_prediction_mode::angular_18;
    }

    if (mode_index <= 30u) {
        return intra_prediction_mode::angular_26;
    }

    return intra_prediction_mode::angular_34;
}

} // namespace cdc::components
