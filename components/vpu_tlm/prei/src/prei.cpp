#include "prei.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace cdc::components {

namespace {

constexpr std::uint32_t DC8  = 288u;
constexpr std::uint32_t DC16 = 1152u;
constexpr std::uint32_t DC32 = 4608u;
constexpr std::uint32_t DC64 = 18432u;

constexpr std::uint32_t PLAN8  = 32u;
constexpr std::uint32_t PLAN16 = 32u;
constexpr std::uint32_t PLAN32 = 32u;
constexpr std::uint32_t PLAN64 = 32u;

constexpr std::uint32_t MODE_FIRST_ANGULAR = 2u;
constexpr std::uint32_t MODE_LAST_ANGULAR  = 33u;
constexpr std::uint32_t MODE_COUNT = 35u;

struct mode_weight {
    int gx_weight = 0;
    int gy_weight = 0;
};

struct prei_block_metrics {
    bool valid = false;
    std::array<std::uint32_t, MODE_COUNT> mode_cost{};
    std::uint32_t modedata = 0;
    std::uint32_t best_angular_mode = MODE_FIRST_ANGULAR;
    std::uint32_t best_angular_cost = std::numeric_limits<std::uint32_t>::max();
};

struct rc_state {
    std::uint64_t frame_bit = 0;
    std::array<std::uint64_t, 7> modebest_history{};
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

std::uint32_t safe_mul_u32(std::uint32_t a, std::uint32_t b)
{
    return clamp_u32(static_cast<std::uint64_t>(a) *
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

mode_weight weight_for_mode(std::uint32_t mode)
{
    switch (mode) {
    case 2:  return { 90,   90};
    case 3:  return { 99,   81};
    case 4:  return {107,   70};
    case 5:  return {113,   60};
    case 6:  return {118,   48};
    case 7:  return {123,   35};
    case 8:  return {126,   20};
    case 9:  return {127,    8};
    case 10: return {128,    0};
    case 11: return {127,   -8};
    case 12: return {126,  -20};
    case 13: return {123,  -35};
    case 14: return {118,  -48};
    case 15: return {113,  -60};
    case 16: return {107,  -70};
    case 17: return { 99,  -81};
    case 18: return { 90,  -90};
    case 19: return { 81,  -99};
    case 20: return { 70, -107};
    case 21: return { 60, -113};
    case 22: return { 48, -118};
    case 23: return { 35, -123};
    case 24: return { 20, -126};
    case 25: return {  8, -127};
    case 26: return {  0,  128};
    case 27: return {  8,  127};
    case 28: return { 20,  126};
    case 29: return { 35,  123};
    case 30: return { 48,  118};
    case 31: return { 60,  113};
    case 32: return { 70,  107};
    case 33: return { 81,   99};
    default: return {0, 0};
    }
}

std::uint32_t dc_threshold_for_size(std::uint32_t size)
{
    if (size <= 8u) {
        return DC8;
    }

    if (size <= 16u) {
        return DC16;
    }

    if (size <= 32u) {
        return DC32;
    }

    return DC64;
}

std::uint32_t planar_threshold_for_size(std::uint32_t size)
{
    if (size <= 8u) {
        return PLAN8;
    }

    if (size <= 16u) {
        return PLAN16;
    }

    if (size <= 32u) {
        return PLAN32;
    }

    return PLAN64;
}

// TLM equivalent of gxgy.v.
// RTL:
// gy = top_row_weighted - bottom_row_weighted
// gx = left_col_weighted - right_col_weighted
void calculate_gxgy_at(const frame& input,
                       int x,
                       int y,
                       int& gx,
                       int& gy)
{
    const int p00 = read_luma_clamped(input, x + 0, y + 0);
    const int p01 = read_luma_clamped(input, x + 1, y + 0);
    const int p02 = read_luma_clamped(input, x + 2, y + 0);

    const int p10 = read_luma_clamped(input, x + 0, y + 1);
    const int p12 = read_luma_clamped(input, x + 2, y + 1);

    const int p20 = read_luma_clamped(input, x + 0, y + 2);
    const int p21 = read_luma_clamped(input, x + 1, y + 2);
    const int p22 = read_luma_clamped(input, x + 2, y + 2);

    gy = p00 + 2 * p01 + p02 - p20 - 2 * p21 - p22;
    gx = p00 + 2 * p10 + p20 - p22 - 2 * p12 - p02;
}

// TLM equivalent of counter.v for one fetched 8x8 block.
// It scans 36 inner 3x3 windows and accumulates angular mode costs.
prei_block_metrics calculate_8x8_metrics(const frame& input,
                                         const block& b8)
{
    prei_block_metrics metrics;
    metrics.valid = !input.empty() && b8.area() != 0;

    if (!metrics.valid) {
        return metrics;
    }

    for (std::uint32_t local_y = 0; local_y < 6u; ++local_y) {
        for (std::uint32_t local_x = 0; local_x < 6u; ++local_x) {
            int gx = 0;
            int gy = 0;

            calculate_gxgy_at(input,
                              static_cast<int>(b8.x + local_x),
                              static_cast<int>(b8.y + local_y),
                              gx,
                              gy);

            metrics.modedata =
                safe_add_u32(metrics.modedata,
                             static_cast<std::uint32_t>(abs_int(gx) + abs_int(gy)));

            for (std::uint32_t mode = MODE_FIRST_ANGULAR;
                 mode <= MODE_LAST_ANGULAR;
                 ++mode) {
                const mode_weight w = weight_for_mode(mode);
                const int value = w.gx_weight * gx + w.gy_weight * gy;

                metrics.mode_cost[mode] =
                    safe_add_u32(metrics.mode_cost[mode],
                                 static_cast<std::uint32_t>(abs_int(value)));
            }
        }
    }

    for (std::uint32_t mode = MODE_FIRST_ANGULAR;
         mode <= MODE_LAST_ANGULAR;
         ++mode) {
        if (metrics.mode_cost[mode] < metrics.best_angular_cost) {
            metrics.best_angular_cost = metrics.mode_cost[mode];
            metrics.best_angular_mode = mode;
        }
    }

    return metrics;
}

prei_block_metrics calculate_block_metrics(const frame& input,
                                           const block& cu)
{
    prei_block_metrics total;
    total.valid = !input.empty() && cu.area() != 0;

    if (!total.valid) {
        return total;
    }

    for (std::uint32_t y = cu.y; y < cu.bottom(); y += 8u) {
        for (std::uint32_t x = cu.x; x < cu.right(); x += 8u) {
            block b8 {
                x,
                y,
                std::min(8u, cu.right() - x),
                std::min(8u, cu.bottom() - y),
                block_type::cu,
                3u
            };

            const prei_block_metrics sub = calculate_8x8_metrics(input, b8);

            if (!sub.valid) {
                continue;
            }

            total.modedata = safe_add_u32(total.modedata, sub.modedata);

            for (std::uint32_t mode = MODE_FIRST_ANGULAR;
                 mode <= MODE_LAST_ANGULAR;
                 ++mode) {
                total.mode_cost[mode] =
                    safe_add_u32(total.mode_cost[mode], sub.mode_cost[mode]);
            }
        }
    }

    for (std::uint32_t mode = MODE_FIRST_ANGULAR;
         mode <= MODE_LAST_ANGULAR;
         ++mode) {
        if (total.mode_cost[mode] < total.best_angular_cost) {
            total.best_angular_cost = total.mode_cost[mode];
            total.best_angular_mode = mode;
        }
    }

    return total;
}

// TLM equivalent of dc_planar.v final override.
// Priority:
// 1. low modedata => DC
// 2. high angular cost compared with Plan threshold => Planar
// 3. otherwise keep best angular mode
std::uint32_t apply_dc_planar_override(const prei_block_metrics& metrics,
                                       const block& cu)
{
    const std::uint32_t size = std::min(cu.width, cu.height);

    if (metrics.modedata < dc_threshold_for_size(size)) {
        return INTRA_DC_MODE;
    }

    const std::uint32_t planar_cmp =
        safe_mul_u32(planar_threshold_for_size(size), metrics.modedata);

    if (metrics.best_angular_cost > planar_cmp) {
        return INTRA_PLANAR_MODE;
    }

    return metrics.best_angular_mode;
}

std::uint32_t qp_subtract_clamped_zero(std::uint32_t qp,
                                       std::uint32_t delta)
{
    return delta > qp ? 0u : qp - delta;
}

std::uint32_t ctu_index_x(const block& ctu)
{
    return CTU_SIZE == 0u ? 0u : ctu.x / CTU_SIZE;
}

std::uint32_t ctu_index_y(const block& ctu)
{
    return CTU_SIZE == 0u ? 0u : ctu.y / CTU_SIZE;
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
 if (input.empty() ||
        ctu.area() == 0 ||
        ctu.right() > input.width ||
        ctu.bottom() > input.height) {
        return prei_result::invalid();
    }


    if (input.empty() || ctu.area() == 0) {
        return prei_result::invalid();
    }

    prei_result result;
    result.valid = true;
    result.ctu = ctu;
    result.qp = clamp_qp(rc_config.initial_qp,
                         rc_config.min_qp,
                         rc_config.max_qp);

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
            result.best_intra_hint.rate =
                estimate_mode_rate(best_it->best_mode_index, best_it->cu);
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

    // mode_write writes 8x8, 16x16, 32x32, then 64x64 modes.
    const std::uint32_t sizes[] = {
        8u,
        16u,
        32u,
        CTU_SIZE
    };

    for (std::uint32_t size : sizes) {
        if (size > ctu.width || size > ctu.height) {
            continue;
        }

        std::uint32_t depth = 0;

        if (size == 8u) {
            depth = 3u;
        } else if (size == 16u) {
            depth = 2u;
        } else if (size == 32u) {
            depth = 1u;
        } else {
            depth = 0u;
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
    entry.valid = false;
    entry.cu = cu;

    if (input.empty() || cu.area() == 0) {
        return entry;
    }

    const prei_block_metrics metrics =
        calculate_block_metrics(input, cu);

    if (!metrics.valid) {
        return entry;
    }

    const std::uint32_t final_mode_index =
        apply_dc_planar_override(metrics, cu);

    prei_mode_candidate final_candidate =
        evaluate_intra_mode(input, cu, final_mode_index, qp);

    if (!final_candidate.valid) {
        return entry;
    }

    // In RTL, modebest64 for RC comes from compare path.
    // DC/Planar only overrides the written mode, not modebest64.
    final_candidate.cost = metrics.best_angular_cost;
    final_candidate.distortion = metrics.modedata;
    final_candidate.rate = estimate_mode_rate(final_mode_index, cu);

    entry.valid = true;
    entry.best_mode_index = final_mode_index;
    entry.best_mode = final_candidate.mapped_mode;
    entry.best_cost = metrics.best_angular_cost;
    entry.activity = metrics.modedata;

    // mode_write/md_ram store selected mode, not all software candidates.
    entry.candidates.clear();
    entry.candidates.push_back(final_candidate);

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
                read_luma_clamped(input,
                                  static_cast<int>(cu.x + bx),
                                  static_cast<int>(cu.y + by));

            const std::uint8_t predicted =
                predict_intra_sample(input, cu, bx, by, mode_index);

            distortion += static_cast<std::uint32_t>(
                abs_int(static_cast<int>(original) -
                        static_cast<int>(predicted))
            );
        }
    }

    const std::uint32_t distortion32 = clamp_u32(distortion);
    const std::uint32_t rate = estimate_mode_rate(mode_index, cu);
    const std::uint32_t lambda = 1u + qp / 6u;
    const std::uint32_t cost = safe_add_u32(distortion32, rate * lambda);

    candidate.valid = true;
    candidate.mode_index = mode_index;
    candidate.mapped_mode = map_intra_mode(mode_index);
    candidate.distortion = distortion32;
    candidate.rate = rate;
    candidate.cost = cost;

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
                sum += read_luma_clamped(input,
                                         static_cast<int>(cu.x + bx),
                                         static_cast<int>(cu.y + by));
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

    const std::uint32_t size = std::min(cu.width, cu.height);

    if (size <= 8u) {
        rate += 2u;
    } else if (size <= 16u) {
        rate += 1u;
    }

    return rate;
}

std::uint32_t prei::calculate_modebest64_sum(const prei_result& result) const
{
    for (const prei_mode_entry& entry : result.mode_entries) {
        if (!entry.valid) {
            continue;
        }

        const bool is_ctu_entry =
            entry.cu.depth == 0 ||
            (entry.cu.width == result.ctu.width &&
             entry.cu.height == result.ctu.height);

        if (is_ctu_entry) {
            return entry.best_cost;
        }
    }

    std::uint64_t sum = 0;

    for (const prei_mode_entry& entry : result.mode_entries) {
        if (!entry.valid) {
            continue;
        }

        sum += entry.best_cost;
    }

    return clamp_u32(sum);
}

std::uint32_t prei::run_rate_control(
    const block& ctu,
    const prei_result& md_result,
    const prei_rate_control_config& rc_config) const
{
    static rc_state state;

    std::uint32_t qp = clamp_qp(rc_config.initial_qp,
                                rc_config.min_qp,
                                rc_config.max_qp);

    const std::uint32_t modebest64_i =
        md_result.modebest64_sum & ((1u << 28u) - 1u);

    const std::uint32_t actual_bitnum_i =
        rc_config.actual_bitnum & 0xFFFFu;

    // RTL cnt == 1:
    // modebest_1 <= modebest64_i + modebest_1;
    // modebest_2 <= modebest_1;
    // ...
    // modebest_7 <= modebest_6;
    const std::uint64_t old_1 = state.modebest_history[0];
    const std::uint64_t old_2 = state.modebest_history[1];
    const std::uint64_t old_3 = state.modebest_history[2];
    const std::uint64_t old_4 = state.modebest_history[3];
    const std::uint64_t old_5 = state.modebest_history[4];
    const std::uint64_t old_6 = state.modebest_history[5];

    state.modebest_history[0] = old_1 + modebest64_i;
    state.modebest_history[1] = old_1;
    state.modebest_history[2] = old_2;
    state.modebest_history[3] = old_3;
    state.modebest_history[4] = old_4;
    state.modebest_history[5] = old_5;
    state.modebest_history[6] = old_6;

    state.frame_bit += actual_bitnum_i;

    const std::uint64_t reg_k =
        rc_config.reg_k != 0u ? rc_config.reg_k : 256u;

    const std::uint64_t multiply_tmp =
        state.modebest_history[6] * reg_k;

    const std::uint64_t predict_bit =
        multiply_tmp >> 28u;

    const bool actual_big =
        state.frame_bit > predict_bit;

    const std::uint64_t diff_abs =
        actual_big ? (state.frame_bit - predict_bit)
                   : (predict_bit - state.frame_bit);

    std::uint32_t diff_level = 0;

    if (diff_abs > rc_config.l2_frame_byte) {
        diff_level = 2u;
    } else if (diff_abs > rc_config.l1_frame_byte) {
        diff_level = 1u;
    }

    const std::uint32_t ctu_y = ctu_index_y(ctu);

    if (!rc_config.lcu_rc_enable || ctu_y == 0u) {
        qp = rc_config.initial_qp;
    } else if (actual_big) {
        qp = safe_add_u32(rc_config.initial_qp, diff_level);
    } else {
        qp = qp_subtract_clamped_zero(rc_config.initial_qp, diff_level);
    }

    if (rc_config.roi_enable && is_inside_roi(ctu, rc_config)) {
        qp = qp_subtract_clamped_zero(qp, rc_config.delta_qp);
    }

    return clamp_qp(qp, rc_config.min_qp, rc_config.max_qp);
}

bool prei::is_inside_roi(const block& ctu,
                         const prei_rate_control_config& rc_config) const
{
    if (!rc_config.roi_enable ||
        rc_config.roi_width == 0 ||
        rc_config.roi_height == 0) {
        return false;
    }

    const std::uint32_t x = ctu_index_x(ctu);
    const std::uint32_t y = ctu_index_y(ctu);

    const bool hit_x =
        (x + 1u > rc_config.roi_x) &&
        (x < rc_config.roi_x + rc_config.roi_width);

    const bool hit_y =
        (y + 1u > rc_config.roi_y) &&
        (y < rc_config.roi_y + rc_config.roi_height);

    return hit_x && hit_y;
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

