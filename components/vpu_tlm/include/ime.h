#pragma once

#include <cstdint>
#include <vector>

#include "block.h"
#include "encoder_defs.h"
#include "frame.h"
#include "ime_result.h"
#include "prediction_result.h"

namespace cdc::components {

// -----------------------------------------------------------------------------
// IME: Integer Motion Estimation
//
// TLM equivalent of xk265 rtl/ime.
//
// Responsibilities:
// - Generate search addresses around a center MV
// - Fetch original/reference luma samples
// - Compute SAD for integer-pel MVs
// - Store MV/cost candidates
// - Decide best inter partition
// - Output best integer MV candidate for FME
// -----------------------------------------------------------------------------

class ime {
public:
    ime() = default;

    // Compatibility API for old skeleton code.
    prediction_result run(const frame& input,
                          const block& region) const;

    // Main IME API.
    ime_result run(const frame& input,
                   const frame& reference,
                   const block& ctu,
                   std::uint32_t qp = INIT_QP) const;

    ime_result run(const frame& input,
                   const frame& reference,
                   const block& ctu,
                   const ime_search_config& config,
                   std::uint32_t qp = INIT_QP) const;

private:
    std::vector<block> build_partition_list(const block& ctu,
                                            const ime_search_config& config) const;

    ime_candidate search_partition(const frame& input,
                                   const frame& reference,
                                   const block& pu,
                                   partition_mode partition,
                                   const ime_search_config& config,
                                   std::uint32_t qp) const;

    std::uint32_t compute_sad(const frame& input,
                              const frame& reference,
                              const block& pu,
                              motion_vector mv,
                              std::vector<std::uint8_t>* predicted_out = nullptr,
                              std::vector<std::int16_t>* residual_out = nullptr) const;

    std::uint32_t estimate_mv_rate(motion_vector mv,
                                   partition_mode partition,
                                   std::uint32_t qp) const;

    std::uint32_t compute_cost(std::uint32_t sad,
                               std::uint32_t rate,
                               std::uint32_t qp) const;

    std::uint8_t sample_reference_luma(const frame& reference,
                                       int x,
                                       int y) const;

    ime_candidate select_best_candidate(const std::vector<ime_candidate>& candidates) const;

    prediction_result make_prediction_result(const frame& input,
                                             const frame& reference,
                                             const ime_candidate& best,
                                             std::uint32_t qp) const;

    std::vector<motion_vector> build_search_points(const ime_search_config& config) const;

    std::uint32_t clamp_qp(std::uint32_t qp) const;
};

} // namespace cdc::components
