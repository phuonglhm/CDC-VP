#include "ime.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
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

motion_vector make_integer_mv(int x, int y)
{
    // Stored in quarter-pel units.
    return motion_vector {x * 4, y * 4};
}

int qpel_to_integer(int value)
{
    return value / 4;
}

} // namespace

prediction_result ime::run(const frame& input,
                           const block& region) const
{
    // Compatibility path only.
    // Without reference frame, IME cannot do real inter search.
    if (input.empty() || region.area() == 0) {
        return prediction_result::invalid();
    }

    prediction_result result = prediction_result::make_inter(
        region.area(),
        motion_vector {0, 0},
        partition_mode::part_2nx2n,
        INIT_QP
    );

    result.valid = true;
    result.rate = 0;
    result.distortion = region.area();

    return result;
}

ime_result ime::run(const frame& input,
                    const frame& reference,
                    const block& ctu,
                    std::uint32_t qp) const
{
    ime_search_config config;
    config.search_range_x = SEARCH_RANGE;
    config.search_range_y = SEARCH_RANGE;
    config.enable_2nx2n = true;
    config.enable_2nxn = true;
    config.enable_nx2n = true;
    config.enable_split = true;
    config.use_feedback = false;
    config.center_mv = motion_vector {0, 0};
    config.downsample = false;

    return run(input, reference, ctu, config, qp);
}

ime_result ime::run(const frame& input,
                    const frame& reference,
                    const block& ctu,
                    const ime_search_config& config,
                    std::uint32_t qp) const
{
    if (input.empty() || reference.empty() || ctu.area() == 0) {
        return ime_result::invalid();
    }

    qp = clamp_qp(qp);

    ime_result result;
    result.valid = true;
    result.ctu = ctu;
    result.qp = qp;

    const std::vector<block> partitions =
        build_partition_list(ctu, config);

    for (const block& pu : partitions) {
        partition_mode partition = partition_mode::part_2nx2n;

        if (pu.width == ctu.width && pu.height == ctu.height) {
            partition = partition_mode::part_2nx2n;
        } else if (pu.width == ctu.width && pu.height * 2u == ctu.height) {
            partition = partition_mode::part_2nxn;
        } else if (pu.width * 2u == ctu.width && pu.height == ctu.height) {
            partition = partition_mode::part_nx2n;
        } else {
            partition = partition_mode::part_split;
        }

        ime_candidate candidate =
            search_partition(input, reference, pu, partition, config, qp);

        if (candidate.valid) {
            result.candidates.push_back(std::move(candidate));
        }
    }

    if (result.candidates.empty()) {
        return ime_result::invalid();
    }

    const ime_candidate best =
        select_best_candidate(result.candidates);

    result.best_partition = best.partition;
    result.best_mv = best.mv;
    result.best_sad = best.sad;
    result.best_rate = best.rate;
    result.best_cost = best.cost;
    result.best_inter_result =
        make_prediction_result(input, reference, best, qp);

    return result;
}

std::vector<block> ime::build_partition_list(const block& ctu,
                                             const ime_search_config& config) const
{
    std::vector<block> list;

    if (config.enable_2nx2n) {
        list.emplace_back(
            ctu.x,
            ctu.y,
            ctu.width,
            ctu.height,
            block_type::pu,
            ctu.depth
        );
    }

    if (config.enable_2nxn && ctu.height >= 2u) {
        const std::uint32_t half_h = ctu.height / 2u;

        list.emplace_back(
            ctu.x,
            ctu.y,
            ctu.width,
            half_h,
            block_type::pu,
            ctu.depth + 1u
        );

        list.emplace_back(
            ctu.x,
            ctu.y + half_h,
            ctu.width,
            ctu.height - half_h,
            block_type::pu,
            ctu.depth + 1u
        );
    }

    if (config.enable_nx2n && ctu.width >= 2u) {
        const std::uint32_t half_w = ctu.width / 2u;

        list.emplace_back(
            ctu.x,
            ctu.y,
            half_w,
            ctu.height,
            block_type::pu,
            ctu.depth + 1u
        );

        list.emplace_back(
            ctu.x + half_w,
            ctu.y,
            ctu.width - half_w,
            ctu.height,
            block_type::pu,
            ctu.depth + 1u
        );
    }

    if (config.enable_split && ctu.width >= 2u && ctu.height >= 2u) {
        const std::uint32_t half_w = ctu.width / 2u;
        const std::uint32_t half_h = ctu.height / 2u;

        list.emplace_back(
            ctu.x,
            ctu.y,
            half_w,
            half_h,
            block_type::pu,
            ctu.depth + 1u
        );

        list.emplace_back(
            ctu.x + half_w,
            ctu.y,
            ctu.width - half_w,
            half_h,
            block_type::pu,
            ctu.depth + 1u
        );

        list.emplace_back(
            ctu.x,
            ctu.y + half_h,
            half_w,
            ctu.height - half_h,
            block_type::pu,
            ctu.depth + 1u
        );

        list.emplace_back(
            ctu.x + half_w,
            ctu.y + half_h,
            ctu.width - half_w,
            ctu.height - half_h,
            block_type::pu,
            ctu.depth + 1u
        );
    }

    return list;
}

ime_candidate ime::search_partition(const frame& input,
                                    const frame& reference,
                                    const block& pu,
                                    partition_mode partition,
                                    const ime_search_config& config,
                                    std::uint32_t qp) const
{
    ime_candidate best;
    best.valid = false;
    best.pu = pu;
    best.partition = partition;

    std::uint32_t best_cost = std::numeric_limits<std::uint32_t>::max();

    const std::vector<motion_vector> search_points =
        build_search_points(config);

    for (const motion_vector& mv : search_points) {
        const std::uint32_t sad =
            compute_sad(input, reference, pu, mv);

        const std::uint32_t rate =
            estimate_mv_rate(mv, partition, qp);

        const std::uint32_t cost =
            compute_cost(sad, rate, qp);

        if (!best.valid || cost < best_cost) {
            best.valid = true;
            best.mv = mv;
            best.sad = sad;
            best.rate = rate;
            best.cost = cost;
            best_cost = cost;
        }
    }

    return best;
}

std::uint32_t ime::compute_sad(const frame& input,
                               const frame& reference,
                               const block& pu,
                               motion_vector mv,
                               std::vector<std::uint8_t>* predicted_out,
                               std::vector<std::int16_t>* residual_out) const
{
    std::uint64_t sad = 0;

    if (predicted_out != nullptr) {
        predicted_out->clear();
        predicted_out->reserve(pu.area());
    }

    if (residual_out != nullptr) {
        residual_out->clear();
        residual_out->reserve(pu.area());
    }

    const int mv_x = qpel_to_integer(mv.x);
    const int mv_y = qpel_to_integer(mv.y);

    for (std::uint32_t by = 0; by < pu.height; ++by) {
        for (std::uint32_t bx = 0; bx < pu.width; ++bx) {
            const std::uint32_t cur_x = pu.x + bx;
            const std::uint32_t cur_y = pu.y + by;

            const std::uint8_t original =
                input.get_luma(cur_x, cur_y);

            const int ref_x = static_cast<int>(cur_x) + mv_x;
            const int ref_y = static_cast<int>(cur_y) + mv_y;

            const std::uint8_t predicted =
                sample_reference_luma(reference, ref_x, ref_y);

            const int diff =
                static_cast<int>(original) - static_cast<int>(predicted);

            sad += static_cast<std::uint32_t>(abs_int(diff));

            if (predicted_out != nullptr) {
                predicted_out->push_back(predicted);
            }

            if (residual_out != nullptr) {
                residual_out->push_back(static_cast<std::int16_t>(diff));
            }
        }
    }

    return clamp_cost_u32(sad);
}

std::uint32_t ime::estimate_mv_rate(motion_vector mv,
                                    partition_mode partition,
                                    std::uint32_t qp) const
{
    (void)qp;

    const std::uint32_t mv_bits =
        static_cast<std::uint32_t>(abs_int(mv.x) + abs_int(mv.y)) / 4u;

    std::uint32_t partition_bits = 1u;

    switch (partition) {
    case partition_mode::part_2nx2n:
        partition_bits = 1u;
        break;
    case partition_mode::part_2nxn:
    case partition_mode::part_nx2n:
        partition_bits = 3u;
        break;
    case partition_mode::part_split:
        partition_bits = 5u;
        break;
    default:
        partition_bits = 3u;
        break;
    }

    return mv_bits + partition_bits;
}

std::uint32_t ime::compute_cost(std::uint32_t sad,
                                std::uint32_t rate,
                                std::uint32_t qp) const
{
    const std::uint32_t lambda = 1u + qp / 6u;

    return clamp_cost_u32(
        static_cast<std::uint64_t>(sad) +
        static_cast<std::uint64_t>(lambda) * rate
    );
}

std::uint8_t ime::sample_reference_luma(const frame& reference,
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

ime_candidate ime::select_best_candidate(const std::vector<ime_candidate>& candidates) const
{
    ime_candidate best;

    if (candidates.empty()) {
        return best;
    }

    best = candidates.front();

    for (const ime_candidate& candidate : candidates) {
        if (!candidate.valid) {
            continue;
        }

        if (!best.valid || candidate.cost < best.cost) {
            best = candidate;
        }
    }

    return best;
}

prediction_result ime::make_prediction_result(const frame& input,
                                              const frame& reference,
                                              const ime_candidate& best,
                                              std::uint32_t qp) const
{
    if (!best.valid) {
        return prediction_result::invalid();
    }

    std::vector<std::uint8_t> predicted;
    std::vector<std::int16_t> residual;

    const std::uint32_t sad =
        compute_sad(input,
                    reference,
                    best.pu,
                    best.mv,
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
    result.distortion = sad;
    result.predicted_luma = std::move(predicted);
    result.residual_luma = std::move(residual);

    return result;
}

std::vector<motion_vector> ime::build_search_points(const ime_search_config& config) const
{
    std::vector<motion_vector> points;

    const int center_x =
        config.use_feedback ? qpel_to_integer(config.center_mv.x) : 0;

    const int center_y =
        config.use_feedback ? qpel_to_integer(config.center_mv.y) : 0;

    const int range_x =
        static_cast<int>(std::min<std::uint32_t>(config.search_range_x, SEARCH_RANGE));

    const int range_y =
        static_cast<int>(std::min<std::uint32_t>(config.search_range_y, SEARCH_RANGE));

    for (int dy = -range_y; dy <= range_y; ++dy) {
        for (int dx = -range_x; dx <= range_x; ++dx) {
            points.push_back(
                make_integer_mv(center_x + dx, center_y + dy)
            );
        }
    }

    return points;
}

std::uint32_t ime::clamp_qp(std::uint32_t qp) const
{
    return std::clamp(qp, MIN_QP, MAX_QP);
}

} // namespace cdc::components
