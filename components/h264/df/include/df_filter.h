#pragma once

#include "df_types.h"

#include <systemc>

namespace h264::df {
struct DfFilter : sc_core::sc_module {
    SC_HAS_PROCESS(DfFilter);
    DfFilter(sc_core::sc_module_name name);

    void apply_filter(std::array<std::uint8_t, 16>& left_blk, 
                      std::array<std::uint8_t, 16>& right_blk, 
                      std::uint8_t bs, std::uint8_t qp);
};
} // namespace h264::df