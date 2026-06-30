#pragma once

#include <cstdint>
#include <vector>

#include "block.h"
#include "encoder_defs.h"
#include "fme_result.h"
#include "frame.h"
#include "ime_result.h"
#include "prediction_result.h"

namespace cdc::components {

// -----------------------------------------------------------------------------
// FME: Fractional Motion Estimation
//
// TLM equivalent of xk265 rtl/fme.
//
// Responsibilities:
// - Receive IME integer-pel MV candidates
// - Prepare half-pel / quarter-pel MV candidates
// - Generate fractional-pel prediction using HEVC luma interpolation filter
// - Compute residual and SATD
// - Compute RD cost with QP lambda
// - Decide best refined MV / skip
// - Output final inter prediction result
// -----------------------------------------------------------------------------

class fme {
public:
    fme() = default;

    // Compatibility API for old skeleton code:
    // fme_.refine(ime_.run(...))
    prediction_result refine(const prediction_result& ime_prediction) const;

    // Compatibility API with frames.
    prediction_result refine(const frame& input,
                             const frame& reference,
                             const block& region,
                             const prediction_result& ime_prediction,
                             std::uint32_t qp = INIT_QP) const;

    // Main FME API.
    fme_result run(const frame& input,
                   const frame& reference,
                   const ime_result& ime_info,
                   std::uint32_t qp = INIT_QP) const;

    fme_result run(const frame& input,
                   const frame& reference,
                   const ime_result& ime_info,
                   const fme_refine_config& config,
                   std::uint32_t qp = INIT_QP) const;

private:
    std::vector<fme_candidate> prepare_candidates(const ime_result& ime_info,
                                                  const fme_refine_config& config) const;

    std::vector<motion_vector> build_half_pel_offsets(const fme_refine_config& config) const;

    std::vector<motion_vector> build_quarter_pel_offsets(const fme_refine_config& config) const;

    fme_candidate evaluate_candidate(const frame& input,
                                     const frame& reference,
                                     const block& pu,
                                     partition_mode partition,
                                     motion_vector mv,
                                     std::uint32_t qp,
                                     const fme_refine_config& config) const;

    prediction_result make_prediction_result(const frame& input,
                                             const frame& reference,
                                             const fme_candidate& best,
                                             std::uint32_t qp,
                                             const fme_refine_config& config) const;

    std::uint32_t compute_satd_and_prediction(const frame& input,
                                              const frame& reference,
                                              const block& pu,
                                              motion_vector mv,
                                              const fme_refine_config& config,
                                              std::vector<std::uint8_t>* predicted_out = nullptr,
                                              std::vector<std::int16_t>* residual_out = nullptr) const;

    std::uint32_t compute_satd_4x4_blocks(const std::vector<std::int16_t>& residual,
                                          std::uint32_t width,
                                          std::uint32_t height) const;

    std::uint32_t hadamard_4x4_satd(const std::vector<std::int16_t>& residual,
                                    std::uint32_t width,
                                    std::uint32_t start_x,
                                    std::uint32_t start_y) const;

    std::uint8_t interpolate_luma_qpel(const frame& reference,
                                       int x,
                                       int y,
                                       motion_vector mv,
                                       const fme_refine_config& config) const;

    std::uint8_t interpolate_luma_hevc_filter(const frame& reference,
                                              int sample_x_qpel,
                                              int sample_y_qpel) const;

    std::uint8_t interpolate_luma_bilinear(const frame& reference,
                                           int sample_x_qpel,
                                           int sample_y_qpel) const;

    std::uint8_t sample_reference_luma(const frame& reference,
                                       int x,
                                       int y) const;

    std::uint32_t estimate_mv_rate(motion_vector mv,
                                   partition_mode partition) const;

    std::uint32_t signed_exp_golomb_bits(int value) const;

    std::uint32_t unsigned_exp_golomb_bits(std::uint32_t value) const;

    std::uint32_t lambda_from_qp(std::uint32_t qp) const;

    std::uint32_t compute_cost(std::uint32_t satd,
                               std::uint32_t rate,
                               std::uint32_t qp) const;

    bool detect_skip(const fme_candidate& candidate,
                     const block& pu) const;

    fme_candidate select_best_candidate(const std::vector<fme_candidate>& candidates) const;

    std::uint32_t clamp_qp(std::uint32_t qp) const;
};

} // namespace cdc::components
