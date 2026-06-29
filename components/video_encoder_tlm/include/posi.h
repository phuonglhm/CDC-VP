#pragma once

#include <cstdint>
#include <vector>

#include "block.h"
#include "encoder_defs.h"
#include "frame.h"
#include "prediction_result.h"
#include "prei_result.h"

namespace cdc::components {

// -----------------------------------------------------------------------------
// POSI: Post-Intra prediction
//
// TLM equivalent of xk265 rtl/posi.
//
// Responsibilities:
// - Read PREI mode candidates
// - Fetch reconstructed top/left reference pixels
// - Generate intra prediction pixels
// - Compute residual
// - Estimate intra rate
// - Compute cost
// - Select best intra prediction result
// -----------------------------------------------------------------------------

class posi {
public:
    posi() = default;

    // Compatibility API for old skeleton code.
    prediction_result run(const frame& input,
                          const block& region) const;

    // Main POSI API.
    prediction_result run(const frame& input,
                          const frame& reconstructed,
                          const block& region,
                          const prei_result& prei_info,
                          std::uint32_t qp = INIT_QP) const;

private:
    prediction_result evaluate_candidate(const frame& input,
                                         const frame& reconstructed,
                                         const block& region,
                                         const prei_mode_entry& mode_entry,
                                         const prei_mode_candidate& candidate,
                                         std::uint32_t qp) const;

    std::vector<std::uint8_t> generate_prediction(const frame& input,
                                                  const frame& reconstructed,
                                                  const block& region,
                                                  std::uint32_t mode_index) const;

    std::uint8_t predict_sample(const frame& input,
                                const frame& reconstructed,
                                const block& region,
                                std::uint32_t local_x,
                                std::uint32_t local_y,
                                std::uint32_t mode_index) const;

    std::uint8_t predict_planar(const frame& input,
                                const frame& reconstructed,
                                const block& region,
                                std::uint32_t local_x,
                                std::uint32_t local_y) const;

    std::uint8_t predict_dc(const frame& input,
                            const frame& reconstructed,
                            const block& region) const;

    std::uint8_t predict_angular(const frame& input,
                                 const frame& reconstructed,
                                 const block& region,
                                 std::uint32_t local_x,
                                 std::uint32_t local_y,
                                 std::uint32_t mode_index) const;

    std::vector<std::int16_t> compute_residual(const frame& input,
                                               const block& region,
                                               const std::vector<std::uint8_t>& predicted) const;

    std::uint32_t compute_satd_like_cost(const std::vector<std::int16_t>& residual,
                                         const block& region) const;

    std::uint32_t estimate_rate(std::uint32_t mode_index,
                                partition_mode partition,
                                const block& region) const;

    partition_mode decide_partition(const block& region,
                                    std::uint32_t cost) const;

    std::uint8_t get_reference_luma(const frame& input,
                                    const frame& reconstructed,
                                    int x,
                                    int y) const;

    intra_prediction_mode map_intra_mode(std::uint32_t mode_index) const;
};

} // namespace cdc::components
