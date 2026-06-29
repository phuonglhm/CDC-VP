#include "mode_decision.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace cdc::components {

namespace {

std::uint32_t saturating_add(std::uint32_t a, std::uint32_t b)
{
    const std::uint64_t sum =
        static_cast<std::uint64_t>(a) + static_cast<std::uint64_t>(b);

    return sum > std::numeric_limits<std::uint32_t>::max()
               ? std::numeric_limits<std::uint32_t>::max()
               : static_cast<std::uint32_t>(sum);
}

} // namespace

prediction_result mode_decision::choose(
    const std::vector<prediction_result>& candidates) const
{
    return select_best_by_cost(candidates);
}

mode_decision_result mode_decision::run(
    const prediction_result& intra_candidate,
    const prediction_result& inter_candidate,
    frame_type current_frame_type,
    std::uint32_t qp) const
{
    mode_decision_config config;
    config.current_frame_type = current_frame_type;
    config.qp = qp;

    return run(intra_candidate, inter_candidate, config);
}

mode_decision_result mode_decision::run(
    const prediction_result& intra_candidate,
    const prediction_result& inter_candidate,
    const mode_decision_config& config) const
{
    if (config.current_frame_type == frame_type::intra) {
        return decide_i_frame(intra_candidate, inter_candidate, config);
    }

    return decide_p_frame(intra_candidate, inter_candidate, config);
}

mode_decision_result mode_decision::run(
    const prediction_result& intra_candidate,
    const fme_result& fme_info,
    frame_type current_frame_type,
    std::uint32_t qp) const
{
    prediction_result inter_candidate = prediction_result::invalid();

    if (fme_info.valid && fme_info.best_inter_result.valid) {
        inter_candidate = fme_info.best_inter_result;
    }

    return run(intra_candidate, inter_candidate, current_frame_type, qp);
}

mode_decision_result mode_decision::run(
    const prediction_result& intra_candidate,
    const ime_result& ime_info,
    frame_type current_frame_type,
    std::uint32_t qp) const
{
    prediction_result inter_candidate = prediction_result::invalid();

    if (ime_info.valid && ime_info.best_inter_result.valid) {
        inter_candidate = ime_info.best_inter_result;
    }

    return run(intra_candidate, inter_candidate, current_frame_type, qp);
}

prediction_result mode_decision::select_best_by_cost(
    const std::vector<prediction_result>& candidates) const
{
    prediction_result best = prediction_result::invalid();

    for (const prediction_result& candidate : candidates) {
        if (!candidate.valid) {
            continue;
        }

        if (!best.valid || candidate.cost < best.cost) {
            best = candidate;
        }
    }

    return best;
}

mode_decision_result mode_decision::decide_i_frame(
    const prediction_result& intra_candidate,
    const prediction_result& inter_candidate,
    const mode_decision_config& config) const
{
    if (config.force_intra_for_i_frame) {
        if (!is_valid_intra(intra_candidate)) {
            return mode_decision_result::invalid();
        }

        prediction_result final_prediction =
            mark_final_prediction(intra_candidate,
                                  final_decision_type::intra,
                                  config);

        return make_result(final_decision_type::intra,
                           intra_candidate,
                           inter_candidate,
                           final_prediction,
                           config);
    }

    // Optional fallback: choose by cost if force_intra_for_i_frame is disabled.
    std::vector<prediction_result> candidates;

    if (is_valid_intra(intra_candidate)) {
        candidates.push_back(intra_candidate);
    }

    if (is_valid_inter(inter_candidate)) {
        candidates.push_back(inter_candidate);
    }

    prediction_result best = select_best_by_cost(candidates);

    if (!best.valid) {
        return mode_decision_result::invalid();
    }

    final_decision_type decision =
        best.mode == prediction_mode::intra
            ? final_decision_type::intra
            : final_decision_type::inter;

    prediction_result final_prediction =
        mark_final_prediction(best, decision, config);

    return make_result(decision,
                       intra_candidate,
                       inter_candidate,
                       final_prediction,
                       config);
}

mode_decision_result mode_decision::decide_p_frame(
    const prediction_result& intra_candidate,
    const prediction_result& inter_candidate,
    const mode_decision_config& config) const
{
    const bool has_intra =
        config.enable_intra_in_p && is_valid_intra(intra_candidate);

    const bool has_inter =
        is_valid_inter(inter_candidate);

    if (!has_intra && !has_inter) {
        return mode_decision_result::invalid();
    }

    if (has_intra && !has_inter) {
        prediction_result final_prediction =
            mark_final_prediction(intra_candidate,
                                  final_decision_type::intra,
                                  config);

        return make_result(final_decision_type::intra,
                           intra_candidate,
                           inter_candidate,
                           final_prediction,
                           config);
    }

    if (!has_intra && has_inter) {
        final_decision_type decision =
            is_skip_candidate(inter_candidate, config)
                ? final_decision_type::skip
                : final_decision_type::inter;

        prediction_result final_prediction =
            mark_final_prediction(inter_candidate,
                                  decision,
                                  config);

        return make_result(decision,
                           intra_candidate,
                           inter_candidate,
                           final_prediction,
                           config);
    }

    const std::uint32_t intra_cost =
        biased_cost(intra_candidate.cost, config.intra_in_p_bias);

    const std::uint32_t inter_cost =
        biased_cost(inter_candidate.cost, config.inter_bias);

    const bool inter_is_skip =
        is_skip_candidate(inter_candidate, config);

    const std::uint32_t skip_cost =
        inter_is_skip
            ? biased_cost(inter_candidate.cost, config.skip_bias)
            : std::numeric_limits<std::uint32_t>::max();

    final_decision_type decision = final_decision_type::invalid;
    prediction_result selected = prediction_result::invalid();

    if (inter_is_skip && skip_cost <= intra_cost && skip_cost <= inter_cost) {
        decision = final_decision_type::skip;
        selected = inter_candidate;
    } else if (inter_cost <= intra_cost) {
        decision = final_decision_type::inter;
        selected = inter_candidate;
    } else {
        decision = final_decision_type::intra;
        selected = intra_candidate;
    }

    prediction_result final_prediction =
        mark_final_prediction(selected, decision, config);

    return make_result(decision,
                       intra_candidate,
                       inter_candidate,
                       final_prediction,
                       config);
}

bool mode_decision::is_valid_intra(const prediction_result& candidate) const
{
    return candidate.valid &&
           candidate.mode == prediction_mode::intra;
}

bool mode_decision::is_valid_inter(const prediction_result& candidate) const
{
    return candidate.valid &&
           candidate.mode == prediction_mode::inter;
}

bool mode_decision::is_skip_candidate(
    const prediction_result& candidate,
    const mode_decision_config& config) const
{
    if (!config.enable_skip || !is_valid_inter(candidate)) {
        return false;
    }

    if (candidate.skip) {
        return true;
    }

    const std::uint32_t threshold =
        skip_threshold(candidate, config);

    const bool small_residual =
        candidate.distortion <= threshold;

    const bool small_rate =
        candidate.rate <= 2u;

    return small_residual && small_rate;
}

std::uint32_t mode_decision::biased_cost(std::uint32_t base_cost,
                                         std::uint32_t bias) const
{
    return saturating_add(base_cost, bias);
}

std::uint32_t mode_decision::skip_threshold(
    const prediction_result& candidate,
    const mode_decision_config& config) const
{
    std::uint32_t area = 0;

    if (!candidate.predicted_luma.empty()) {
        area = static_cast<std::uint32_t>(candidate.predicted_luma.size());
    } else if (!candidate.residual_luma.empty()) {
        area = static_cast<std::uint32_t>(candidate.residual_luma.size());
    } else {
        // Conservative default when no block-size payload exists.
        area = CTU_SIZE * CTU_SIZE;
    }

    const std::uint64_t threshold =
        static_cast<std::uint64_t>(area) *
        static_cast<std::uint64_t>(config.skip_residual_threshold_per_pixel);

    return threshold > std::numeric_limits<std::uint32_t>::max()
               ? std::numeric_limits<std::uint32_t>::max()
               : static_cast<std::uint32_t>(threshold);
}

prediction_result mode_decision::mark_final_prediction(
    prediction_result candidate,
    final_decision_type decision,
    const mode_decision_config& config) const
{
    candidate.qp = clamp_qp(config.qp);

    candidate.i_in_p = false;
    candidate.skip = false;
    candidate.merge = false;

    switch (decision) {
    case final_decision_type::intra:
        candidate.mode = prediction_mode::intra;
        candidate.i_in_p =
            config.current_frame_type == frame_type::inter;
        break;

    case final_decision_type::inter:
        candidate.mode = prediction_mode::inter;
        break;

    case final_decision_type::skip:
        candidate.mode = prediction_mode::inter;
        candidate.skip = true;
        candidate.merge = true;
        break;

    default:
        break;
    }

    return candidate;
}

mode_decision_result mode_decision::make_result(
    final_decision_type decision,
    const prediction_result& intra_candidate,
    const prediction_result& inter_candidate,
    const prediction_result& final_prediction,
    const mode_decision_config& config) const
{
    if (!final_prediction.valid) {
        return mode_decision_result::invalid();
    }

    mode_decision_result result;
    result.valid = true;

    result.decision = decision;
    result.current_frame_type = config.current_frame_type;
    result.qp = clamp_qp(config.qp);

    result.i_in_p = final_prediction.i_in_p;
    result.skip = final_prediction.skip;
    result.merge = final_prediction.merge;

    result.final_cost = final_prediction.cost;
    result.final_rate = final_prediction.rate;
    result.final_distortion = final_prediction.distortion;

    result.intra_candidate = intra_candidate;
    result.inter_candidate = inter_candidate;
    result.final_prediction = final_prediction;

    return result;
}

std::uint32_t mode_decision::clamp_qp(std::uint32_t qp) const
{
    return std::clamp(qp, MIN_QP, MAX_QP);
}

} // namespace cdc::components
