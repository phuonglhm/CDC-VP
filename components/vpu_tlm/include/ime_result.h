#pragma once

#include <cstdint>
#include <vector>

#include "block.h"
#include "encoder_defs.h"
#include "prediction_result.h"

namespace cdc::components {

// -----------------------------------------------------------------------------
// IME result structures
//
// TLM equivalent concepts:
// - ime_cost_store.v              -> candidates
// - ime_mv_dump.v                 -> best_mv / candidate MV data
// - ime_partition_decision.v      -> best_partition
// - ime_transfer.v                -> best_inter_result
// -----------------------------------------------------------------------------

struct ime_search_config {
    std::uint32_t search_range_x = SEARCH_RANGE;
    std::uint32_t search_range_y = SEARCH_RANGE;

    bool enable_2nx2n = true;
    bool enable_2nxn = true;
    bool enable_nx2n = true;
    bool enable_split = true;

    bool use_feedback = false;
    motion_vector center_mv {0, 0};

    bool downsample = false;
};

struct ime_candidate {
    bool valid = false;

    block pu {};
    partition_mode partition = partition_mode::part_2nx2n;

    // Quarter-pel unit, but IME only generates integer-pel MV,
    // so x/y are multiples of 4.
    motion_vector mv {0, 0};

    std::uint32_t sad = 0;
    std::uint32_t rate = 0;
    std::uint32_t cost = 0;
};

struct ime_result {
    bool valid = false;

    block ctu {};
    std::uint32_t qp = INIT_QP;

    partition_mode best_partition = partition_mode::part_2nx2n;
    motion_vector best_mv {0, 0};

    std::uint32_t best_sad = 0;
    std::uint32_t best_rate = 0;
    std::uint32_t best_cost = 0;

    std::vector<ime_candidate> candidates;

    prediction_result best_inter_result;

    static ime_result invalid()
    {
        return ime_result {};
    }

    // Compatibility for old TOP code:
    // candidates.push_back(ime_.run(...));
    operator prediction_result() const
    {
        return best_inter_result;
    }
};

} // namespace cdc::components
