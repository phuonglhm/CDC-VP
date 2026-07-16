#include "systemc_vpu/hardware_metrics.hpp"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

int main() {
    model::systemc_native::HardwareReportInput input;
    input.width = 100;
    input.height = 80;
    input.frames = 10;
    input.encoder_mode = model::vpu_reg::MODE_INTRA_DIRECTIONAL_TQ;
    input.clock_mhz = 500.0;
    input.total_cycles = 1'000'000;
    input.bitstream_bytes = 40'000;

    model::systemc_native::NativePipelineStats stats;
    stats.job_start_cycle = 100;
    stats.first_input_cycle = 200;
    stats.first_output_cycle = 1100;
    stats.last_input_cycle = 900'000;
    stats.last_output_cycle = 1'000'100;
    stats.frames_input = stats.frames_prediction = stats.frames_transform =
        stats.frames_cabac = stats.frames_output = 10;
    stats.dma_read_active = 100'000;
    stats.prediction_active = 200'000;
    stats.transform_active = 300'000;
    stats.cabac_active = 120'000;
    stats.dma_write_active = 50'000;
    stats.dma_read_bytes = 120'000;
    stats.dma_write_bytes = 40'000;
    stats.dma_read_bursts = 2'000;
    stats.dma_write_bursts = 700;
    stats.dma_read_wait_cycles = 12'000;
    stats.dma_write_wait_cycles = 4'000;

    std::ostringstream output;
    model::systemc_native::print_hardware_report(output, input, stats);
    const std::string report = output.str();

    assert(report.find("HARDWARE METRIC DASHBOARD") != std::string::npos);
    assert(report.find("intra-directional-tq") != std::string::npos);
    assert(report.find("500.000 MHz") != std::string::npos);
    assert(report.find("2.00 ms") != std::string::npos);
    assert(report.find("5000.00 frame/s") != std::string::npos);
    assert(report.find("1000 cycles") != std::string::npos);
    assert(report.find("needs synthesis") != std::string::npos);
    assert(report.find("no x265/FFmpeg encoder") != std::string::npos);

    std::cout << "hardware metric report tests passed\n";
}
