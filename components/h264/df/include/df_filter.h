#pragma once

#include "df_types.h"

#include <systemc>

namespace h264::df {
// One reference per side, progressive frame edges. Not B bi-prediction.
struct EdgeSide {
    bool intra=false, nonzero=false;
    std::uint64_t reference=0;
    int mv_x=0,mv_y=0; // Quarter-luma-sample units.
};
unsigned boundary_strength(const EdgeSide& p,const EdgeSide& q,bool external);
unsigned chroma_qp(unsigned luma_qp); // Zero chroma_qp_index_offset.

struct DfFilter : sc_core::sc_module {
    SC_HAS_PROCESS(DfFilter);
    DfFilter(sc_core::sc_module_name name);

    void apply_filter(std::array<std::uint8_t, 16>& left_blk,
                      std::array<std::uint8_t, 16>& right_blk,
                      std::uint8_t bs, std::uint8_t qp, bool chroma = false,
                      int alpha_offset = 0, int beta_offset = 0);
};
} // namespace h264::df
