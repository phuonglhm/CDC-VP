#pragma once

#include "df_types.h"

#include <systemc>

namespace h264::df {
struct DfLineMemory : sc_core::sc_module {
    SC_HAS_PROCESS(DfLineMemory);
    DfLineMemory(sc_core::sc_module_name name);

    void write_block(const std::array<std::uint8_t, 16>& block);
    void read_block(std::array<std::uint8_t, 16>& block) const;

private:
    std::array<std::uint8_t, 16> sram_buffer_{};
};
} // namespace h264::df