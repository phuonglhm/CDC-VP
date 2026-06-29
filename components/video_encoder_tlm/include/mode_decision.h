#pragma once

#include <cstdint>
#include <vector>

#include "encoder_defs.h"
#include "fme_result.h"
#include "ime_result.h"
#include "mode_decision_result.h"
#include "prediction_result.h"

namespace cdc::components {

// -----------------------------------------------------------------------------
// Mode Decision
//
// xk265 does not have one standalone RTL mode_decision.v file.
// The decision logic is distributed across:
// - PREI/POSI intra decision
// - IME/FME inter decision
// - FME skip decision
// - TOP pipeline control flags
//
// This TLM block collects the final intra/inter candidates and selects:
// - INTRA for I-frame
// - INTRA-in-P, INTER, or SKIP for P-frame
// -----------------------------------------------------------------------------

class mode_decision {
public:
    mode_decision() = default;

    // Compatibility API for old skeleton:
    //   best = mode_decision_.choose(candidates);
    prediction_result choose(const std::vector<prediction_result>& candidates) const;

    // Main API: choose between final intra and final inter candidate.
    mode_decision_result run(const prediction_result& intra_candidate,
                             const prediction_result& inter_candidate,
                             frame_type current_frame_type,
                             std::uint32_t qp = INIT_QP) const;

    mode_decision_result run(const prediction_result& intra_candidate,
                             const prediction_result& inter_candidate,
                             const mode_decision_config& config) const;

    // Helper API: use FME result directly as inter candidate.
    mode_decision_result run(const prediction_result& intra_candidate,
                             const fme_result& fme_info,
                             frame_type current_frame_type,
                             std::uint32_t qp = INIT_QP) const;

    // Helper API: if FME is not available yet, allow IME inter candidate.
    mode_decision_result run(const prediction_result& intra_candidate,
                             const ime_result& ime_info,
                             frame_type current_frame_type,
                             std::uint32_t qp = INIT_QP) const;

private:
    prediction_result select_best_by_cost(
        const std::vector<prediction_result>& candidates) const;

    mode_decision_result decide_i_frame(
        const prediction_result& intra_candidate,
        const prediction_result& inter_candidate,
        const mode_decision_config& config) const;

    mode_decision_result decide_p_frame(
        const prediction_result& intra_candidate,
        const prediction_result& inter_candidate,
        const mode_decision_config& config) const;

    bool is_valid_intra(const prediction_result& candidate) const;

    bool is_valid_inter(const prediction_result& candidate) const;

    bool is_skip_candidate(const prediction_result& candidate,
                           const mode_decision_config& config) const;

    std::uint32_t biased_cost(std::uint32_t base_cost,
                              std::uint32_t bias) const;

    std::uint32_t skip_threshold(const prediction_result& candidate,
                                 const mode_decision_config& config) const;

    prediction_result mark_final_prediction(prediction_result candidate,
                                            final_decision_type decision,
                                            const mode_decision_config& config) const;

    mode_decision_result make_result(final_decision_type decision,
                                     const prediction_result& intra_candidate,
                                     const prediction_result& inter_candidate,
                                     const prediction_result& final_prediction,
                                     const mode_decision_config& config) const;

    std::uint32_t clamp_qp(std::uint32_t qp) const;
};

} // namespace cdc::components
