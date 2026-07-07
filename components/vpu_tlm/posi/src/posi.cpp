#include "posi.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

#include "prei_result.h"

namespace cdc::components {

namespace {

constexpr std::uint32_t POSI_MIN_SIZE = 4u;
constexpr std::uint32_t POSI_MAX_SIZE = 32u;
constexpr std::uint32_t NUM_REF_SAMPLES = 32u;

struct reference_samples {
    std::array<std::uint8_t, NUM_REF_SAMPLES> top{};
    std::array<std::uint8_t, NUM_REF_SAMPLES> right{};
    std::array<std::uint8_t, NUM_REF_SAMPLES> left{};
    std::array<std::uint8_t, NUM_REF_SAMPLES> down{};
    std::uint8_t top_left = 128;
};

struct block_eval {
    bool valid = false;

    block region;

    std::uint32_t mode_index = INTRA_DC_MODE;
    intra_prediction_mode mapped_mode = intra_prediction_mode::dc;

    std::uint32_t distortion = 0;
    std::uint32_t rate = 0;
    std::uint32_t cost = 0;

    std::vector<std::uint8_t> predicted;
    std::vector<std::int16_t> residual;
};

struct partition_eval {
    bool valid = false;

    std::uint32_t size = POSI_MIN_SIZE;
    std::uint32_t total_distortion = 0;
    std::uint32_t total_rate = 0;
    std::uint32_t total_cost = 0;

    std::uint32_t representative_mode_index = INTRA_DC_MODE;
    intra_prediction_mode representative_mode = intra_prediction_mode::dc;

    std::vector<std::uint8_t> predicted_region;
    std::vector<std::int16_t> residual_region;
};

int abs_int(int value)
{
    return value < 0 ? -value : value;
}

std::uint8_t clamp_u8(int value)
{
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
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

intra_prediction_mode map_mode_index(std::uint32_t mode_index)
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

bool contains_mode(const std::vector<std::uint32_t>& modes,
                   std::uint32_t mode)
{
    return std::find(modes.begin(), modes.end(), mode) != modes.end();
}

void add_unique_mode(std::vector<std::uint32_t>& modes,
                     std::uint32_t mode)
{
    if (mode >= NUM_INTRA_MODES) {
        return;
    }

    if (!contains_mode(modes, mode)) {
        modes.push_back(mode);
    }
}

bool block_overlaps(const block& a, const block& b)
{
    const bool overlap_x = a.x < b.right() && a.right() > b.x;
    const bool overlap_y = a.y < b.bottom() && a.bottom() > b.y;

    return overlap_x && overlap_y;
}

std::vector<std::uint32_t> collect_candidate_modes(const prei_result& prei_info,
                                                   const block& region)
{
    std::vector<std::uint32_t> modes;

    for (const prei_mode_entry& entry : prei_info.mode_entries) {
        if (!entry.valid || !block_overlaps(entry.cu, region)) {
            continue;
        }

        if (!entry.candidates.empty()) {
            for (const prei_mode_candidate& candidate : entry.candidates) {
                if (candidate.valid) {
                    add_unique_mode(modes, candidate.mode_index);
                }
            }
        } else {
            add_unique_mode(modes, entry.best_mode_index);
        }
    }

    if (modes.empty()) {
        add_unique_mode(modes, INTRA_PLANAR_MODE);
        add_unique_mode(modes, INTRA_DC_MODE);
        add_unique_mode(modes, 10u);
        add_unique_mode(modes, 18u);
        add_unique_mode(modes, 26u);
        add_unique_mode(modes, 33u);
    }

    std::sort(modes.begin(), modes.end());
    return modes;
}

// TLM equivalent of posi_reference:
// collect top/right/left/down/top-left reference samples.
// RTL has row/col/frame RAM and padding/filtering. Here reconstructed frame
// is used as the already-available reference memory.
reference_samples make_reference_samples(const frame& reconstructed,
                                         const frame& input,
                                         const block& cu)
{
    reference_samples ref;

    const frame& source =
        !reconstructed.empty() ? reconstructed : input;

    const int x0 = static_cast<int>(cu.x);
    const int y0 = static_cast<int>(cu.y);

    ref.top_left = read_luma_clamped(source, x0 - 1, y0 - 1);

    for (std::uint32_t i = 0; i < NUM_REF_SAMPLES; ++i) {
        ref.top[i] =
            read_luma_clamped(source, x0 + static_cast<int>(i), y0 - 1);

        ref.right[i] =
            read_luma_clamped(source,
                              x0 + static_cast<int>(cu.width),
                              y0 + static_cast<int>(i));

        ref.left[i] =
            read_luma_clamped(source, x0 - 1, y0 + static_cast<int>(i));

        ref.down[i] =
            read_luma_clamped(source,
                              x0 + static_cast<int>(i),
                              y0 + static_cast<int>(cu.height));
    }

    return ref;
}

std::uint8_t predict_dc_sample(const reference_samples& ref,
                               const block& cu)
{
    std::uint32_t sum = 0;
    std::uint32_t count = 0;

    const std::uint32_t width =
        std::min<std::uint32_t>(cu.width, NUM_REF_SAMPLES);

    const std::uint32_t height =
        std::min<std::uint32_t>(cu.height, NUM_REF_SAMPLES);

    for (std::uint32_t i = 0; i < width; ++i) {
        sum += ref.top[i];
        ++count;
    }

    for (std::uint32_t i = 0; i < height; ++i) {
        sum += ref.left[i];
        ++count;
    }

    if (count == 0) {
        return 128;
    }

    return static_cast<std::uint8_t>((sum + count / 2u) / count);
}

std::uint8_t predict_planar_sample(const reference_samples& ref,
                                   const block& cu,
                                   std::uint32_t local_x,
                                   std::uint32_t local_y)
{
    const std::uint32_t width = std::max(1u, cu.width);
    const std::uint32_t height = std::max(1u, cu.height);

    const std::uint32_t lx =
        std::min<std::uint32_t>(local_x, NUM_REF_SAMPLES - 1u);

    const std::uint32_t ly =
        std::min<std::uint32_t>(local_y, NUM_REF_SAMPLES - 1u);

    const std::uint32_t top_idx =
        std::min<std::uint32_t>(local_x, NUM_REF_SAMPLES - 1u);

    const std::uint32_t left_idx =
        std::min<std::uint32_t>(local_y, NUM_REF_SAMPLES - 1u);

    const std::uint32_t top_right_idx =
        std::min<std::uint32_t>(width - 1u, NUM_REF_SAMPLES - 1u);

    const std::uint32_t bottom_left_idx =
        std::min<std::uint32_t>(height - 1u, NUM_REF_SAMPLES - 1u);

    const int left = ref.left[left_idx];
    const int top = ref.top[top_idx];
    const int top_right = ref.top[top_right_idx];
    const int bottom_left = ref.left[bottom_left_idx];

    const int hor =
        static_cast<int>(width - 1u - lx) * left +
        static_cast<int>(lx + 1u) * top_right;

    const int ver =
        static_cast<int>(height - 1u - ly) * top +
        static_cast<int>(ly + 1u) * bottom_left;

    const int denom = static_cast<int>(width + height);
    const int value = (hor + ver + denom / 2) / std::max(1, denom);

    return clamp_u8(value);
}

std::uint8_t predict_angular_sample(const reference_samples& ref,
                                    std::uint32_t local_x,
                                    std::uint32_t local_y,
                                    std::uint32_t mode_index)
{
    const int mode = static_cast<int>(mode_index);
    const int center_mode = 18;
    const int delta = mode - center_mode;

    if (mode < center_mode) {
        const int projected =
            static_cast<int>(local_y) +
            (delta * static_cast<int>(local_x + 1u)) / 8;

        const std::uint32_t idx =
            static_cast<std::uint32_t>(
                std::clamp(projected, 0, static_cast<int>(NUM_REF_SAMPLES - 1u))
            );

        return ref.left[idx];
    }

    if (mode > center_mode) {
        const int projected =
            static_cast<int>(local_x) +
            (delta * static_cast<int>(local_y + 1u)) / 8;

        const std::uint32_t idx =
            static_cast<std::uint32_t>(
                std::clamp(projected, 0, static_cast<int>(NUM_REF_SAMPLES - 1u))
            );

        return ref.top[idx];
    }

    const std::uint32_t top_idx =
        std::min<std::uint32_t>(local_x, NUM_REF_SAMPLES - 1u);

    const std::uint32_t left_idx =
        std::min<std::uint32_t>(local_y, NUM_REF_SAMPLES - 1u);

    return static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(ref.top[top_idx]) +
         static_cast<std::uint32_t>(ref.left[left_idx])) / 2u
    );
}

// TLM equivalent of posi_prediction.
std::vector<std::uint8_t> generate_prediction(const reference_samples& ref,
                                              const block& cu,
                                              std::uint32_t mode_index)
{
    std::vector<std::uint8_t> predicted;
    predicted.resize(static_cast<std::size_t>(cu.area()), 128);

    for (std::uint32_t y = 0; y < cu.height; ++y) {
        for (std::uint32_t x = 0; x < cu.width; ++x) {
            std::uint8_t value = 128;

            if (mode_index == INTRA_PLANAR_MODE) {
                value = predict_planar_sample(ref, cu, x, y);
            } else if (mode_index == INTRA_DC_MODE) {
                value = predict_dc_sample(ref, cu);
            } else {
                value = predict_angular_sample(ref, x, y, mode_index);
            }

            predicted[static_cast<std::size_t>(y * cu.width + x)] = value;
        }
    }

    return predicted;
}

// TLM equivalent of posi_buffer:
// residual = original - prediction.
std::vector<std::int16_t> generate_residual(const frame& input,
                                            const block& cu,
                                            const std::vector<std::uint8_t>& predicted)
{
    std::vector<std::int16_t> residual;
    residual.resize(predicted.size(), 0);

    for (std::uint32_t y = 0; y < cu.height; ++y) {
        for (std::uint32_t x = 0; x < cu.width; ++x) {
            const std::size_t idx =
                static_cast<std::size_t>(y * cu.width + x);

            const int original =
                read_luma_clamped(input,
                                  static_cast<int>(cu.x + x),
                                  static_cast<int>(cu.y + y));

            residual[idx] =
                static_cast<std::int16_t>(original -
                                          static_cast<int>(predicted[idx]));
        }
    }

    return residual;
}

std::uint32_t satd_4x4(const std::array<int, 16>& src)
{
    std::array<int, 16> tmp{};

    for (int y = 0; y < 4; ++y) {
        const int s0 = src[y * 4 + 0] + src[y * 4 + 3];
        const int s1 = src[y * 4 + 1] + src[y * 4 + 2];
        const int s2 = src[y * 4 + 1] - src[y * 4 + 2];
        const int s3 = src[y * 4 + 0] - src[y * 4 + 3];

        tmp[y * 4 + 0] = s0 + s1;
        tmp[y * 4 + 1] = s3 + s2;
        tmp[y * 4 + 2] = s0 - s1;
        tmp[y * 4 + 3] = s3 - s2;
    }

    std::uint32_t sum = 0;

    for (int x = 0; x < 4; ++x) {
        const int s0 = tmp[0 * 4 + x] + tmp[3 * 4 + x];
        const int s1 = tmp[1 * 4 + x] + tmp[2 * 4 + x];
        const int s2 = tmp[1 * 4 + x] - tmp[2 * 4 + x];
        const int s3 = tmp[0 * 4 + x] - tmp[3 * 4 + x];

        sum += static_cast<std::uint32_t>(abs_int(s0 + s1));
        sum += static_cast<std::uint32_t>(abs_int(s3 + s2));
        sum += static_cast<std::uint32_t>(abs_int(s0 - s1));
        sum += static_cast<std::uint32_t>(abs_int(s3 - s2));
    }

    return (sum + 1u) >> 1u;
}

// TLM equivalent of posi_satd_cost + cost_engine + transpose.
std::uint32_t calculate_satd_cost(const std::vector<std::int16_t>& residual,
                                  std::uint32_t width,
                                  std::uint32_t height)
{
    std::uint64_t total = 0;

    for (std::uint32_t by = 0; by < height; by += 4u) {
        for (std::uint32_t bx = 0; bx < width; bx += 4u) {
            std::array<int, 16> block4{};

            for (std::uint32_t y = 0; y < 4u; ++y) {
                for (std::uint32_t x = 0; x < 4u; ++x) {
                    const std::uint32_t px = bx + x;
                    const std::uint32_t py = by + y;

                    if (px < width && py < height) {
                        const std::size_t idx =
                            static_cast<std::size_t>(py * width + px);

                        block4[static_cast<std::size_t>(y * 4u + x)] =
                            static_cast<int>(residual[idx]);
                    }
                }
            }

            total += satd_4x4(block4);
        }
    }

    return clamp_u32(total);
}

// TLM equivalent of posi_rate_estimation.
// This is not CABAC bit-exact. It keeps the RTL role: add mode/size/rate
// penalty before partition decision.
std::uint32_t estimate_intra_rate(std::uint32_t mode_index,
                                  const block& cu,
                                  std::uint32_t qp)
{
    std::uint32_t rate = 1u;

    if (mode_index == INTRA_PLANAR_MODE ||
        mode_index == INTRA_DC_MODE) {
        rate += 1u;
    } else {
        rate += 3u;
    }

    const std::uint32_t size = std::min(cu.width, cu.height);

    if (size <= 4u) {
        rate += 4u;
    } else if (size <= 8u) {
        rate += 3u;
    } else if (size <= 16u) {
        rate += 2u;
    } else {
        rate += 1u;
    }

    rate += qp / 16u;

    return rate;
}

block_eval evaluate_block_candidate(const frame& input,
                                    const frame& reconstructed,
                                    const block& cu,
                                    std::uint32_t mode_index,
                                    std::uint32_t qp)
{
    block_eval eval;
    eval.valid = false;
    eval.region = cu;
    eval.mode_index = mode_index;
    eval.mapped_mode = map_mode_index(mode_index);

    if (input.empty() || cu.area() == 0 || mode_index >= NUM_INTRA_MODES) {
        return eval;
    }

    const reference_samples ref =
        make_reference_samples(reconstructed, input, cu);

    eval.predicted =
        generate_prediction(ref, cu, mode_index);

    eval.residual =
        generate_residual(input, cu, eval.predicted);

    eval.distortion =
        calculate_satd_cost(eval.residual, cu.width, cu.height);

    eval.rate =
        estimate_intra_rate(mode_index, cu, qp);

    const std::uint32_t lambda = 1u + qp / 6u;

    eval.cost =
        safe_add_u32(eval.distortion, eval.rate * lambda);

    eval.valid = true;
    return eval;
}

void copy_block_to_region(const block& region,
                          const block& cu,
                          const std::vector<std::uint8_t>& block_pred,
                          const std::vector<std::int16_t>& block_res,
                          std::vector<std::uint8_t>& region_pred,
                          std::vector<std::int16_t>& region_res)
{
    for (std::uint32_t y = 0; y < cu.height; ++y) {
        for (std::uint32_t x = 0; x < cu.width; ++x) {
            const std::size_t src_idx =
                static_cast<std::size_t>(y * cu.width + x);

            const std::uint32_t dst_x = cu.x + x - region.x;
            const std::uint32_t dst_y = cu.y + y - region.y;

            if (dst_x >= region.width || dst_y >= region.height) {
                continue;
            }

            const std::size_t dst_idx =
                static_cast<std::size_t>(dst_y * region.width + dst_x);

            if (src_idx < block_pred.size() && dst_idx < region_pred.size()) {
                region_pred[dst_idx] = block_pred[src_idx];
            }

            if (src_idx < block_res.size() && dst_idx < region_res.size()) {
                region_res[dst_idx] = block_res[src_idx];
            }
        }
    }
}

// TLM equivalent of posi_partition_decision.
// For each CU size, choose the best mode per sub-block, then compare total
// cost across candidate partition sizes.
partition_eval evaluate_partition_size(const frame& input,
                                       const frame& reconstructed,
                                       const block& region,
                                       std::uint32_t size,
                                       const std::vector<std::uint32_t>& modes,
                                       std::uint32_t qp)
{
    partition_eval part;
    part.valid = false;
    part.size = size;

    if (input.empty() || region.area() == 0 ||
        size == 0 || modes.empty()) {
        return part;
    }

    part.predicted_region.resize(static_cast<std::size_t>(region.area()), 128);
    part.residual_region.resize(static_cast<std::size_t>(region.area()), 0);

    std::vector<std::uint32_t> selected_modes;

    for (std::uint32_t y = region.y; y < region.bottom(); y += size) {
        for (std::uint32_t x = region.x; x < region.right(); x += size) {
            block cu {
                x,
                y,
                std::min(size, region.right() - x),
                std::min(size, region.bottom() - y),
                block_type::cu,
                0u
            };

            block_eval best_block;
            best_block.cost = std::numeric_limits<std::uint32_t>::max();

            for (std::uint32_t mode_index : modes) {
                block_eval eval =
                    evaluate_block_candidate(input,
                                             reconstructed,
                                             cu,
                                             mode_index,
                                             qp);

                if (eval.valid && eval.cost < best_block.cost) {
                    best_block = std::move(eval);
                }
            }

            if (!best_block.valid) {
                continue;
            }

            part.total_distortion =
                safe_add_u32(part.total_distortion,
                             best_block.distortion);

            part.total_rate =
                safe_add_u32(part.total_rate,
                             best_block.rate);

            part.total_cost =
                safe_add_u32(part.total_cost,
                             best_block.cost);

            selected_modes.push_back(best_block.mode_index);

            copy_block_to_region(region,
                                 cu,
                                 best_block.predicted,
                                 best_block.residual,
                                 part.predicted_region,
                                 part.residual_region);
        }
    }

    if (selected_modes.empty()) {
        return part;
    }

    std::sort(selected_modes.begin(), selected_modes.end());

    std::uint32_t best_count = 0;
    std::uint32_t best_mode = selected_modes.front();

    for (std::uint32_t mode : selected_modes) {
        const std::uint32_t count =
            static_cast<std::uint32_t>(
                std::count(selected_modes.begin(), selected_modes.end(), mode)
            );

        if (count > best_count) {
            best_count = count;
            best_mode = mode;
        }
    }

    part.representative_mode_index = best_mode;
    part.representative_mode = map_mode_index(best_mode);
    part.valid = true;

    return part;
}

std::array<std::uint32_t, 4> posi_traversal_sizes()
{
    // Same traversal family as posi_ctrl SIZE_04/SIZE_08/SIZE_16/SIZE_32.
    return {4u, 8u, 16u, 32u};
}

} // namespace

prediction_result posi::run(const frame& input,
                            const block& region) const
{
    prei_result prei_info;
    prei_info.valid = true;
    prei_info.ctu = region;
    prei_info.qp = INIT_QP;

    frame reconstructed = input;

    return run(input, reconstructed, region, prei_info, INIT_QP);
}

prediction_result posi::run(const frame& input,
                            const frame& reconstructed,
                            const block& region,
                            const prei_result& prei_info,
                            std::uint32_t qp) const
{
    if (input.empty() ||
        reconstructed.empty() ||
        region.area() == 0 ||
        region.right() > input.width ||
        region.bottom() > input.height ||
        region.right() > reconstructed.width ||
        region.bottom() > reconstructed.height ||
        !prei_info.valid) {
        return prediction_result::invalid();
    }

    const std::uint32_t clamped_qp =
        std::clamp(qp, MIN_QP, MAX_QP);

    const std::vector<std::uint32_t> candidate_modes =
        collect_candidate_modes(prei_info, region);

    partition_eval best_partition;
    best_partition.total_cost = std::numeric_limits<std::uint32_t>::max();

    for (std::uint32_t size : posi_traversal_sizes()) {
        if (size > POSI_MAX_SIZE || size < POSI_MIN_SIZE) {
            continue;
        }

        if (size > region.width || size > region.height) {
            continue;
        }

        partition_eval part =
            evaluate_partition_size(input,
                                    reconstructed,
                                    region,
                                    size,
                                    candidate_modes,
                                    clamped_qp);

        if (part.valid && part.total_cost < best_partition.total_cost) {
            best_partition = std::move(part);
        }
    }

    if (!best_partition.valid) {
        return prediction_result::invalid();
    }

    prediction_result result =
        prediction_result::make_intra(best_partition.total_cost,
                                      best_partition.representative_mode,
                                      partition_mode::part_2nx2n,
                                      clamped_qp);

    result.valid = true;
    result.cost = best_partition.total_cost;
    result.distortion = best_partition.total_distortion;
    result.rate = best_partition.total_rate;
    result.qp = clamped_qp;
    result.predicted_luma = std::move(best_partition.predicted_region);
    result.residual_luma = std::move(best_partition.residual_region);

    return result;
}

} // namespace cdc::components

