#include "video_encoder_tlm.h"

#include <algorithm>

namespace cdc::components {

namespace {

bool is_valid_region_for_frame(const frame& input, const block& region)
{
    if (input.empty()) {
        return false;
    }

    if (region.width == 0 || region.height == 0) {
        return false;
    }

    return region.right() <= input.width && region.bottom() <= input.height;
}

bool has_reference_for_region(const frame& reference, const block& region)
{
    return !reference.empty() &&
           region.right() <= reference.width &&
           region.bottom() <= reference.height;
}

std::uint32_t clamp_qp(std::uint32_t qp)
{
    return std::clamp(qp, MIN_QP, MAX_QP);
}

prei_rate_control_config make_prei_config(std::uint32_t qp)
{
    prei_rate_control_config config;
    config.initial_qp = qp;
    config.min_qp = qp;
    config.max_qp = qp;
    return config;
}

} // namespace

prediction_result video_encoder_tlm::run_intra(const frame& input,
                                               const frame& reconstructed,
                                               const block& region,
                                               std::uint32_t qp) const
{
    if (!is_valid_region_for_frame(input, region)) {
        return prediction_result::invalid();
    }

    const std::uint32_t clamped_qp = clamp_qp(qp);
    const prei_result prei_info =
        prei_.run(input, region, make_prei_config(clamped_qp));
    return posi_.run(input, reconstructed, region, prei_info, clamped_qp);
}

fme_result video_encoder_tlm::run_inter(const frame& input,
                                        const frame& reference,
                                        const block& region,
                                        std::uint32_t qp) const
{
    if (!is_valid_region_for_frame(input, region) ||
        !has_reference_for_region(reference, region)) {
        return fme_result::invalid();
    }

    const std::uint32_t clamped_qp = clamp_qp(qp);
    const ime_result ime_info = ime_.run(input, reference, region, clamped_qp);
    return fme_.run(input, reference, ime_info, clamped_qp);
}

video_encoder_tlm_result video_encoder_tlm::run_prediction(
    const frame& input,
    const frame& reconstructed,
    const frame& reference,
    const block& region,
    std::uint32_t qp) const
{
    if (!is_valid_region_for_frame(input, region)) {
        return video_encoder_tlm_result::invalid();
    }

    video_encoder_tlm_result result;
    result.region = region;
    result.qp = clamp_qp(qp);
    const bool can_run_inter = has_reference_for_region(reference, region);

    result.trace.prei_info =
        prei_.run(input, region, make_prei_config(result.qp));
    result.trace.best_intra =
        posi_.run(input, reconstructed, region, result.trace.prei_info, result.qp);

    if (can_run_inter) {
        result.trace.ime_info = ime_.run(input, reference, region, result.qp);
        result.trace.fme_info = fme_.run(input, reference, result.trace.ime_info, result.qp);
        result.trace.best_inter = result.trace.fme_info.best_inter_result;
    } else {
        result.trace.ime_info = ime_result::invalid();
        result.trace.fme_info = fme_result::invalid();
        result.trace.best_inter = prediction_result::invalid();
    }

    result.final_decision = mode_decision_.run(result.trace.best_intra, result.trace.best_inter);
    result.selected_prediction = result.final_decision.selected;
    result.rec_request =
        prediction_to_rec_packet_.run(result.region, result.selected_prediction);

    result.valid = result.trace.prei_info.valid &&
                   result.trace.best_intra.valid &&
                   result.final_decision.valid &&
                   result.rec_request.valid;

    if (can_run_inter) {
        result.valid = result.valid &&
                       result.trace.ime_info.valid &&
                       result.trace.fme_info.valid &&
                       result.trace.best_inter.valid;
    }

    if (!result.valid) {
        return video_encoder_tlm_result::invalid();
    }

    return result;
}

} // namespace cdc::components
