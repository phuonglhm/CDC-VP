#pragma once

#include <cstdint>
#include <vector>

#include "block.h"
#include "encoder_defs.h"
#include "prediction_result.h"

namespace cdc::components {

// -----------------------------------------------------------------------------
// FME result structures
//
// TLM equivalent concepts:
// - fme_mv_candidate_prepare.v -> candidate MV list
// - fme_interpolator*.v        -> fractional-pel predicted samples
// - fme_satd_8x8.v             -> SATD cost
// - fme_cost.v                 -> RD cost with QP lambda
// - fme_skip.v                 -> skip candidate decision
// - fme_pred.v / fme_top.v     -> final inter prediction output
// -----------------------------------------------------------------------------

struct fme_refine_config {
    bool enable_half_pel = true;
    bool enable_quarter_pel = true;
    bool enable_skip_decision = true;

    // Search around IME integer MV in quarter-pel units.
    // Half-pel offsets are multiples of 2.
    std::int32_t half_pel_radius_qpel = 2;

    // Quarter-pel final refinement around the best half-pel candidate.
    std::int32_t quarter_pel_radius_qpel = 1;

    // Enable HEVC-style 8-tap interpolation for luma.
    bool use_hevc_luma_filter = true;
};

struct fme_candidate {
    bool valid = false;

    block pu {};
    partition_mode partition = partition_mode::part_2nx2n;

    // Quarter-pel unit motion vector.
    motion_vector mv {0, 0};

    std::uint32_t satd = 0;
    std::uint32_t rate = 0;
    std::uint32_t cost = 0;

    bool half_pel = false;
    bool quarter_pel = false;
    bool skip = false;
};

struct fme_result {
    bool valid = false;

    block ctu {};
    std::uint32_t qp = INIT_QP;

    partition_mode best_partition = partition_mode::part_2nx2n;
    motion_vector best_mv {0, 0};

    std::uint32_t best_satd = 0;
    std::uint32_t best_rate = 0;
    std::uint32_t best_cost = 0;

    bool skip = false;

    std::vector<fme_candidate> candidates;

    prediction_result best_inter_result;

    static fme_result invalid()
    {
        return fme_result {};
    }

    // Compatibility for old TOP code.
    operator prediction_result() const
    {
        return best_inter_result;
    }
};

} // namespace cdc::components
