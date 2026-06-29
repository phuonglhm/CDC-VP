#pragma once

#include <cstdint>
#include <vector>

#include "block.h"
#include "encoder_defs.h"
#include "prediction_result.h"

namespace cdc::components {

// -----------------------------------------------------------------------------
// PREI result structures
//
// TLM equivalent concepts:
// - mode_write.v / md_ram.v  -> mode_entries
// - modebest64_w             -> modebest64_sum
// - rate_control.v output    -> qp
// -----------------------------------------------------------------------------

struct prei_mode_candidate {
    bool valid = false;

    // HEVC intra mode index:
    //   0  = Planar
    //   1  = DC
    //   2..34 = Angular modes
    std::uint32_t mode_index = INTRA_DC_MODE;

    intra_prediction_mode mapped_mode = intra_prediction_mode::dc;

    std::uint32_t distortion = 0;
    std::uint32_t rate = 0;
    std::uint32_t cost = 0;
};

struct prei_mode_entry {
    bool valid = false;

    block cu {};

    std::uint32_t best_mode_index = INTRA_DC_MODE;
    intra_prediction_mode best_mode = intra_prediction_mode::dc;

    std::uint32_t activity = 0;
    std::uint32_t best_cost = 0;

    // Candidate list for 35 HEVC intra modes.
    std::vector<prei_mode_candidate> candidates;
};

struct prei_rate_control_config {
    // Bit count information
    std::uint32_t target_bitnum = 0;
    std::uint32_t actual_bitnum = 0;

    // QP control
    std::uint32_t initial_qp = INIT_QP;
    std::uint32_t min_qp = MIN_QP;
    std::uint32_t max_qp = MAX_QP;
    std::uint32_t delta_qp = 0;

    // LCU-level rate control enable
    bool lcu_rc_enable = false;

    // ROI control
    bool roi_enable = false;
    std::uint32_t roi_x = 0;
    std::uint32_t roi_y = 0;
    std::uint32_t roi_width = 0;
    std::uint32_t roi_height = 0;

    // Frame-level rate-control parameters.
    std::uint32_t l1_frame_byte = 0;
    std::uint32_t l2_frame_byte = 0;
    std::uint32_t reg_k = 0;
};

struct prei_result {
    bool valid = false;

    block ctu {};

    std::uint32_t qp = INIT_QP;

    // Summary value equivalent to PREI modebest64 signal meaning.
    std::uint32_t modebest64_sum = 0;

    // TLM abstraction of mode RAM entries.
    std::vector<prei_mode_entry> mode_entries;

    // Best intra hint passed to POSI / TOP compatibility path.
    prediction_result best_intra_hint;

    static prei_result invalid()
    {
        return prei_result {};
    }

    // Compatibility: old TOP code can still push this into vector<prediction_result>.
    operator prediction_result() const
    {
        return best_intra_hint;
    }
};

} // namespace cdc::components
