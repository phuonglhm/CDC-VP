#pragma once
#include <systemc>
#include <array>
#include <cstdint>

namespace h264::tq {

class TransposeRam : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(TransposeRam);
    explicit TransposeRam(sc_core::sc_module_name name);

    void write_row(unsigned row, const std::array<std::int32_t, 16>& data);
    std::array<std::int32_t, 16> read_column(unsigned col) const;

private:
    std::array<std::int32_t, 16> mem_{};
};

    
} // namespace h264::tq

