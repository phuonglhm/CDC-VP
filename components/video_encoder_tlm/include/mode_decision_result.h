#pragma once

#include <cstdint>

#include "encoder_defs.h"
#include "prediction_result.h"

namespace cdc::components {

// -----------------------------------------------------------------------------
// Final mode decision result
//
// TLM equivalent concept:
// - POSI already decides best intra candidate.
// - IME/FME already decide best inter candidate.
// - This block decides final coding mode:
//     I-frame: force intra
//     P-frame: choose intra-in-P, inter, or skip
// -----------------------------------------------------------------------------

enum class final_decision_type {
    invalid = 0,
    intra,
    inter,
    skip
};

struct mode_decision_config {
    frame_type current_frame_type = frame_type::inter;

    std::uint32_t qp = INIT_QP;

    bool enable_intra_in_p = true;
    bool enable_skip = true;

    // Biases are in cost units.
    // Positive bias makes that mode less likely.
    std::uint32_t intra_in_p_bias = 0;
    std::uint32_t inter_bias = 0;
    std::uint32_t skip_bias = 0;

    // Skip is only accepted when residual is small enough.
    // threshold = block_area * skip_residual_threshold_per_pixel
    std::uint32_t skip_residual_threshold_per_pixel = 1;

    // If true, I-frame always picks intra candidate.
    bool force_intra_for_i_frame = true;
};

struct mode_decision_result {
    bool valid = false;

    final_decision_type decision = final_decision_type::invalid;

    frame_type current_frame_type = frame_type::inter;

    std::uint32_t qp = INIT_QP;

    bool i_in_p = false;
    bool skip = false;
    bool merge = false;

    std::uint32_t final_cost = 0;
    std::uint32_t final_rate = 0;
    std::uint32_t final_distortion = 0;

    prediction_result intra_candidate;
    prediction_result inter_candidate;

    prediction_result final_prediction;

    static mode_decision_result invalid()
    {
        return mode_decision_result {};
    }

    // Compatibility with old pipeline code.
    operator prediction_result() const
    {
        return final_prediction;
    }
};

} // namespace cdc::components
