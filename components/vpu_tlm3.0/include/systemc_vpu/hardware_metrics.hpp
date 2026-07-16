#pragma once

#include "systemc_vpu/packets.hpp"

#include <cstdint>
#include <iosfwd>

namespace model::systemc_native {

struct HardwareReportInput {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t frames = 0;
    std::uint32_t qp = 26;
    std::uint32_t encoder_mode = vpu_reg::MODE_PCM;
    std::uint32_t fifo_depth = 4;
    std::uint32_t dma_burst_bytes = 64;
    std::uint32_t sram_read_latency_cycles = 1;
    std::uint32_t sram_write_latency_cycles = 1;
    std::uint32_t sram_bytes_per_cycle = 16;
    double clock_mhz = 1000.0;
    std::uint64_t total_cycles = 0;
    std::uint64_t bitstream_bytes = 0;
};

[[nodiscard]] const char* encoder_mode_name(std::uint32_t mode);

void print_hardware_report(std::ostream& stream,
                           const HardwareReportInput& input,
                           const NativePipelineStats& stats);

} // namespace model::systemc_native
