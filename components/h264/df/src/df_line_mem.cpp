#include "df_line_mem.h"

namespace h264::df {
DfLineMemory::DfLineMemory(sc_core::sc_module_name name) : sc_core::sc_module(name) {}

void DfLineMemory::write_block(const std::array<std::uint8_t, 16>& block) {
    sram_buffer_ = block;
}

void DfLineMemory::read_block(std::array<std::uint8_t, 16>& block) const {
    block = sram_buffer_;
}
} // namespace h264::df