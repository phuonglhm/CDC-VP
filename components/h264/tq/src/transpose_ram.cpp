#include "transpose_ram.h"

namespace h264::tq {

TransposeRam::TransposeRam(sc_core::sc_module_name name)
    : sc_core::sc_module(name) {}

void TransposeRam::write_row(unsigned row,
                            const std::array<std::int32_t,16>& data) {
    if (row >= 4) return;
    for (unsigned col = 0; col < 4; ++col) {
        mem_[row * 4 + col] = data[row * 4 + col];
    }
}

std::array<std::int32_t,16> TransposeRam::read_column(unsigned col) const {
    std::array<std::int32_t,16> out{};
    if (col >= 4) return out;
    for (unsigned row = 0; row < 4; ++row) {
        out[row * 4 + col] = mem_[row * 4 + col];
    }
    return out;
}
    
} // namespace h264::tq 