#include "ime.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace cdc::components {

namespace {

enum class rtl_partition_kind {
    part_1nx1n,
    part_1nx2n,
    part_2nx1n,
    part_2nx2n
};

struct rtl_candidate {
    bool valid = false;

    block region {};
    motion_vector mv {};

    rtl_partition_kind rtl_partition = rtl_partition_kind::part_2nx2n;
    partition_mode public_partition = partition_mode::part_2nx2n;

    std::uint32_t sad = 0;
    std::uint32_t mvd_cost = 0;
    std::uint32_t cost = 0;

    std::vector<std::uint8_t> predicted_luma;
};

struct rtl_partition_eval {
    bool valid = false;

    rtl_partition_kind rtl_partition = rtl_partition_kind::part_2nx2n;
    partition_mode public_partition = partition_mode::part_2nx2n;

    std::uint32_t sad = 0;
    std::uint32_t mvd_cost = 0;
    std::uint32_t cost = 0;

    std::vector<rtl_candidate> pieces;
};

int abs_int(int value)
{
    return value < 0 ? -value : value;
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

std::uint32_t clamp_u32(std::uint64_t value)
{
    return value > std::numeric_limits<std::uint32_t>::max()
               ? std::numeric_limits<std::uint32_t>::max()
               : static_cast<std::uint32_t>(value);
}

std::uint32_t safe_add_u32(std::uint32_t a, std::uint32_t b)
{
    return clamp_u32(static_cast<std::uint64_t>(a) +
                     static_cast<std::uint64_t>(b));
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

std::uint32_t mvd_bits_component(int value)
{
    const int abs_value = abs_int(value);

    if (abs_value == 0) return 1u;
    if (abs_value == 1) return 7u;
    if (abs_value <= 3) return 9u;
    if (abs_value <= 7) return 11u;
    if (abs_value <= 15) return 13u;
    if (abs_value <= 31) return 15u;
    if (abs_value <= 63) return 17u;
    if (abs_value <= 127) return 19u;

    return 21u;
}

std::uint32_t calculate_mvd_cost(const motion_vector& mv,
                                 std::uint32_t qp)
{
    // RTL IME uses integer-pixel MV. In this TLM model motion_vector uses
    // quarter-pel unit, so convert back to integer pixel for MVD bit cost.
    const int mv_x_int = mv.x / 4;
    const int mv_y_int = mv.y / 4;

    const std::uint32_t bits =
        mvd_bits_component(mv_x_int) + mvd_bits_component(mv_y_int);

    const std::uint32_t lambda = lambda_from_qp(qp);

    return bits * lambda;
}

std::vector<std::uint8_t> make_prediction_block(const frame& reference,
                                                const block& current,
                                                const motion_vector& mv)
{
    std::vector<std::uint8_t> predicted;
    predicted.resize(static_cast<std::size_t>(current.area()), 128);

    const int mv_x_int = mv.x / 4;
    const int mv_y_int = mv.y / 4;

    for (std::uint32_t y = 0; y < current.height; ++y) {
        for (std::uint32_t x = 0; x < current.width; ++x) {
            const int ref_x = static_cast<int>(current.x + x) + mv_x_int;
            const int ref_y = static_cast<int>(current.y + y) + mv_y_int;

            predicted[static_cast<std::size_t>(y * current.width + x)] =
                read_luma_clamped(reference, ref_x, ref_y);
        }
    }

    return predicted;
}

std::uint32_t calculate_sad(const frame& input,
                            const frame& reference,
                            const block& current,
                            const motion_vector& mv,
                            bool downsample)
{
    const int mv_x_int = mv.x / 4;
    const int mv_y_int = mv.y / 4;

    std::uint64_t sad = 0;
    const std::uint32_t step = downsample ? 2u : 1u;
    const std::uint32_t scale = downsample ? 4u : 1u;

    for (std::uint32_t y = 0; y < current.height; y += step) {
        for (std::uint32_t x = 0; x < current.width; x += step) {
            const int cur_x = static_cast<int>(current.x + x);
            const int cur_y = static_cast<int>(current.y + y);

            const int ref_x = cur_x + mv_x_int;
            const int ref_y = cur_y + mv_y_int;

            const int cur_pixel = read_luma_clamped(input, cur_x, cur_y);
            const int ref_pixel = read_luma_clamped(reference, ref_x, ref_y);

            sad += static_cast<std::uint32_t>(abs_int(cur_pixel - ref_pixel)) * scale;
        }
    }

    return clamp_u32(sad);
}

int align_to_integer_pel_qpel(int qpel)
{
    if (qpel >= 0) {
        return ((qpel + 2) / 4) * 4;
    }

    return -((((-qpel) + 2) / 4) * 4);
}

std::vector<motion_vector> generate_search_points(const ime_search_config& config)
{
    std::vector<motion_vector> points;
    const std::uint32_t range_x = std::min<std::uint32_t>(config.search_range_x,
                                                           IME_SEARCH_RANGE);
    const std::uint32_t range_y = std::min<std::uint32_t>(config.search_range_y,
                                                           IME_SEARCH_RANGE);
    const int center_x = config.use_feedback
                             ? align_to_integer_pel_qpel(config.center_mv.x)
                             : 0;
    const int center_y = config.use_feedback
                             ? align_to_integer_pel_qpel(config.center_mv.y)
                             : 0;

    points.reserve(static_cast<std::size_t>((range_x * 2u + 1u) *
                                            (range_y * 2u + 1u)));

    for (int dx = -static_cast<int>(range_x);
         dx <= static_cast<int>(range_x);
         ++dx) {
        for (int dy = -static_cast<int>(range_y);
             dy <= static_cast<int>(range_y);
             ++dy) {
            motion_vector mv;
            mv.x = center_x + dx * 4;
            mv.y = center_y + dy * 4;
            points.push_back(mv);
        }
    }

    return points;
}

partition_mode to_public_partition(rtl_partition_kind kind)
{
    switch (kind) {
    case rtl_partition_kind::part_1nx1n:
        return partition_mode::part_split;
    case rtl_partition_kind::part_1nx2n:
        return partition_mode::part_nx2n;
    case rtl_partition_kind::part_2nx1n:
        return partition_mode::part_2nxn;
    case rtl_partition_kind::part_2nx2n:
    default:
        return partition_mode::part_2nx2n;
    }
}

rtl_candidate search_one_block(const frame& input,
                               const frame& reference,
                               const block& current,
                               rtl_partition_kind rtl_partition,
                               const ime_search_config& config,
                               std::uint32_t qp)
{
    rtl_candidate best;
    best.valid = false;
    best.region = current;
    best.rtl_partition = rtl_partition;
    best.public_partition = to_public_partition(rtl_partition);
    best.cost = std::numeric_limits<std::uint32_t>::max();

    if (input.empty() || reference.empty() || current.area() == 0) {
        return best;
    }

    const std::vector<motion_vector> points = generate_search_points(config);

    for (const motion_vector& mv : points) {
        const std::uint32_t sad =
            calculate_sad(input, reference, current, mv, config.downsample);

        const std::uint32_t mvd_cost =
            calculate_mvd_cost(mv, qp);

        const std::uint32_t cost =
            safe_add_u32(sad, mvd_cost);

        if (!best.valid || cost < best.cost) {
            best.valid = true;
            best.mv = mv;
            best.sad = sad;
            best.mvd_cost = mvd_cost;
            best.cost = cost;
        }
    }

    if (best.valid) {
        best.predicted_luma =
            make_prediction_block(reference, current, best.mv);
    }

    return best;
}

block make_child_block(const block& parent,
                       std::uint32_t rel_x,
                       std::uint32_t rel_y,
                       std::uint32_t width,
                       std::uint32_t height)
{
    block child {
        parent.x + rel_x,
        parent.y + rel_y,
        std::min(width, parent.width - rel_x),
        std::min(height, parent.height - rel_y),
        block_type::pu,
        parent.depth + 1u
    };

    return child;
}

rtl_partition_eval evaluate_partition_2nx2n(const frame& input,
                                            const frame& reference,
                                            const block& current,
                                            const ime_search_config& config,
                                            std::uint32_t qp)
{
    rtl_partition_eval eval;
    eval.rtl_partition = rtl_partition_kind::part_2nx2n;
    eval.public_partition = partition_mode::part_2nx2n;

    rtl_candidate c =
        search_one_block(input,
                         reference,
                         current,
                         rtl_partition_kind::part_2nx2n,
                         config,
                         qp);

    if (!c.valid) {
        return eval;
    }

    eval.valid = true;
    eval.sad = c.sad;
    eval.mvd_cost = c.mvd_cost;
    eval.cost = c.cost;
    eval.pieces.push_back(std::move(c));

    return eval;
}

rtl_partition_eval evaluate_partition_2nx1n(const frame& input,
                                            const frame& reference,
                                            const block& current,
                                            const ime_search_config& config,
                                            std::uint32_t qp)
{
    rtl_partition_eval eval;
    eval.rtl_partition = rtl_partition_kind::part_2nx1n;
    eval.public_partition = partition_mode::part_2nxn;

    const std::uint32_t h0 = current.height / 2u;
    const std::uint32_t h1 = current.height - h0;

    if (h0 == 0 || h1 == 0) {
        return eval;
    }

    std::vector<block> parts;
    parts.push_back(make_child_block(current, 0u, 0u, current.width, h0));
    parts.push_back(make_child_block(current, 0u, h0, current.width, h1));

    for (const block& part : parts) {
        rtl_candidate c =
            search_one_block(input,
                             reference,
                             part,
                             rtl_partition_kind::part_2nx1n,
                             config,
                             qp);

        if (!c.valid) {
            return eval;
        }

        eval.sad = safe_add_u32(eval.sad, c.sad);
        eval.mvd_cost = safe_add_u32(eval.mvd_cost, c.mvd_cost);
        eval.cost = safe_add_u32(eval.cost, c.cost);
        eval.pieces.push_back(std::move(c));
    }

    eval.valid = true;
    return eval;
}

rtl_partition_eval evaluate_partition_1nx2n(const frame& input,
                                            const frame& reference,
                                            const block& current,
                                            const ime_search_config& config,
                                            std::uint32_t qp)
{
    rtl_partition_eval eval;
    eval.rtl_partition = rtl_partition_kind::part_1nx2n;
    eval.public_partition = partition_mode::part_nx2n;

    const std::uint32_t w0 = current.width / 2u;
    const std::uint32_t w1 = current.width - w0;

    if (w0 == 0 || w1 == 0) {
        return eval;
    }

    std::vector<block> parts;
    parts.push_back(make_child_block(current, 0u, 0u, w0, current.height));
    parts.push_back(make_child_block(current, w0, 0u, w1, current.height));

    for (const block& part : parts) {
        rtl_candidate c =
            search_one_block(input,
                             reference,
                             part,
                             rtl_partition_kind::part_1nx2n,
                             config,
                             qp);

        if (!c.valid) {
            return eval;
        }

        eval.sad = safe_add_u32(eval.sad, c.sad);
        eval.mvd_cost = safe_add_u32(eval.mvd_cost, c.mvd_cost);
        eval.cost = safe_add_u32(eval.cost, c.cost);
        eval.pieces.push_back(std::move(c));
    }

    eval.valid = true;
    return eval;
}

rtl_partition_eval evaluate_partition_1nx1n(const frame& input,
                                            const frame& reference,
                                            const block& current,
                                            const ime_search_config& config,
                                            std::uint32_t qp)
{
    rtl_partition_eval eval;
    eval.rtl_partition = rtl_partition_kind::part_1nx1n;
    eval.public_partition = partition_mode::part_split;

    const std::uint32_t w0 = current.width / 2u;
    const std::uint32_t h0 = current.height / 2u;
    const std::uint32_t w1 = current.width - w0;
    const std::uint32_t h1 = current.height - h0;

    if (w0 == 0 || h0 == 0 || w1 == 0 || h1 == 0) {
        return eval;
    }

    std::vector<block> parts;
    parts.push_back(make_child_block(current, 0u, 0u, w0, h0));
    parts.push_back(make_child_block(current, w0, 0u, w1, h0));
    parts.push_back(make_child_block(current, 0u, h0, w0, h1));
    parts.push_back(make_child_block(current, w0, h0, w1, h1));

    for (const block& part : parts) {
        rtl_candidate c =
            search_one_block(input,
                             reference,
                             part,
                             rtl_partition_kind::part_1nx1n,
                             config,
                             qp);

        if (!c.valid) {
            return eval;
        }

        eval.sad = safe_add_u32(eval.sad, c.sad);
        eval.mvd_cost = safe_add_u32(eval.mvd_cost, c.mvd_cost);
        eval.cost = safe_add_u32(eval.cost, c.cost);
        eval.pieces.push_back(std::move(c));
    }

    eval.valid = true;
    return eval;
}

rtl_partition_eval choose_partition_like_rtl(const frame& input,
                                             const frame& reference,
                                             const block& ctu,
                                             const ime_search_config& config,
                                             std::uint32_t qp)
{
    rtl_partition_eval best;
    best.valid = false;
    best.cost = std::numeric_limits<std::uint32_t>::max();

    std::vector<rtl_partition_eval> candidates;
    candidates.reserve(4);

    if (config.enable_split) {
        candidates.push_back(
            evaluate_partition_1nx1n(input, reference, ctu, config, qp));
    }

    if (config.enable_2nxn) {
        candidates.push_back(
            evaluate_partition_2nx1n(input, reference, ctu, config, qp));
    }

    if (config.enable_nx2n) {
        candidates.push_back(
            evaluate_partition_1nx2n(input, reference, ctu, config, qp));
    }

    if (config.enable_2nx2n || candidates.empty()) {
        candidates.push_back(
            evaluate_partition_2nx2n(input, reference, ctu, config, qp));
    }

    for (const rtl_partition_eval& candidate : candidates) {
        if (candidate.valid &&
            (!best.valid || candidate.cost < best.cost)) {
            best = candidate;
        }
    }

    return best;
}

std::vector<std::uint8_t> merge_prediction(const block& ctu,
                                           const rtl_partition_eval& eval)
{
    std::vector<std::uint8_t> out;
    out.resize(static_cast<std::size_t>(ctu.area()), 128);

    for (const rtl_candidate& piece : eval.pieces) {
        if (!piece.valid) {
            continue;
        }

        for (std::uint32_t y = 0; y < piece.region.height; ++y) {
            for (std::uint32_t x = 0; x < piece.region.width; ++x) {
                const std::size_t src_idx =
                    static_cast<std::size_t>(y * piece.region.width + x);

                const std::uint32_t dst_x =
                    piece.region.x + x - ctu.x;

                const std::uint32_t dst_y =
                    piece.region.y + y - ctu.y;

                if (dst_x >= ctu.width || dst_y >= ctu.height) {
                    continue;
                }

                const std::size_t dst_idx =
                    static_cast<std::size_t>(dst_y * ctu.width + dst_x);

                if (src_idx < piece.predicted_luma.size() &&
                    dst_idx < out.size()) {
                    out[dst_idx] = piece.predicted_luma[src_idx];
                }
            }
        }
    }

    return out;
}

motion_vector representative_mv(const rtl_partition_eval& eval)
{
    if (!eval.pieces.empty()) {
        return eval.pieces.front().mv;
    }

    motion_vector mv;
    mv.x = 0;
    mv.y = 0;
    return mv;
}

std::vector<std::int16_t> build_residual(const frame& input,
                                         const block& ctu,
                                         const std::vector<std::uint8_t>& predicted)
{
    std::vector<std::int16_t> residual;
    residual.resize(predicted.size(), 0);

    for (std::uint32_t y = 0; y < ctu.height; ++y) {
        for (std::uint32_t x = 0; x < ctu.width; ++x) {
            const std::size_t idx =
                static_cast<std::size_t>(y * ctu.width + x);
            const int cur =
                read_luma_clamped(input,
                                  static_cast<int>(ctu.x + x),
                                  static_cast<int>(ctu.y + y));
            const int pred =
                idx < predicted.size()
                    ? static_cast<int>(predicted[idx])
                    : 128;
            residual[idx] = static_cast<std::int16_t>(cur - pred);
        }
    }

    return residual;
}

prediction_result make_prediction_result_from_eval(const frame& input,
                                                   const block& ctu,
                                                   const rtl_partition_eval& eval,
                                                   std::uint32_t qp)
{
    prediction_result result;
    const std::vector<std::uint8_t> merged_prediction =
        merge_prediction(ctu, eval);
    result.valid = eval.valid;
    result.mode = prediction_mode::inter;
    result.partition = eval.public_partition;
    result.mv = representative_mv(eval);
    result.qp = qp;
    result.distortion = eval.sad;
    result.rate = eval.mvd_cost;
    result.cost = eval.cost;
    result.predicted_luma = merged_prediction;
    result.residual_luma = build_residual(input, ctu, merged_prediction);

    return result;
}

} // namespace

prediction_result ime::run(const frame& input,
                           const block& region) const
{
    if (input.empty() || region.area() == 0) {
        return prediction_result::invalid();
    }

    ime_result ime_info = run(input, input, region, INIT_QP);
    return static_cast<prediction_result>(ime_info);
}

ime_result ime::run(const frame& input,
                    const frame& reference,
                    const block& ctu,
                    std::uint32_t qp) const
{
    ime_search_config config;
    return run(input, reference, ctu, config, qp);
}

ime_result ime::run(const frame& input,
                    const frame& reference,
                    const block& ctu,
                    const ime_search_config& config,
                    std::uint32_t qp) const
{
    ime_result result;

    if (input.empty() ||
        reference.empty() ||
        ctu.area() == 0 ||
        ctu.right() > input.width ||
        ctu.bottom() > input.height ||
        ctu.right() > reference.width ||
        ctu.bottom() > reference.height) {
        return ime_result::invalid();
    }

    const std::uint32_t clamped_qp =
        std::clamp(qp, MIN_QP, MAX_QP);

    rtl_partition_eval best =
        choose_partition_like_rtl(input, reference, ctu, config, clamped_qp);

    if (!best.valid) {
        result.valid = false;
        return result;
    }

    prediction_result inter_prediction =
        make_prediction_result_from_eval(input, ctu, best, clamped_qp);

    result.valid = true;
    result.ctu = ctu;
    result.qp = clamped_qp;
    result.best_inter_result = inter_prediction;
    result.best_partition = best.public_partition;
    result.best_mv = representative_mv(best);
    result.best_cost = best.cost;
    result.best_sad = best.sad;
    result.best_rate = best.mvd_cost;

    result.candidates.clear();

    for (const rtl_candidate& piece : best.pieces) {
        ime_candidate candidate;
        candidate.valid = piece.valid;
        candidate.pu = piece.region;
        candidate.partition = piece.public_partition;
        candidate.mv = piece.mv;
        candidate.sad = piece.sad;
        candidate.rate = piece.mvd_cost;
        candidate.cost = piece.cost;

        result.candidates.push_back(std::move(candidate));
    }

    return result;
}


} // namespace cdc::components
