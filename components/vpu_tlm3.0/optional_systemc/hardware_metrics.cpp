#include "systemc_vpu/hardware_metrics.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <ostream>
#include <string_view>
#include <tuple>

namespace model::systemc_native {
namespace {

double percent(std::uint64_t numerator, std::uint64_t denominator) {
    return denominator == 0
        ? 0.0
        : 100.0 * static_cast<double>(numerator) /
              static_cast<double>(denominator);
}

double ratio(std::uint64_t numerator, std::uint64_t denominator) {
    return denominator == 0
        ? 0.0
        : static_cast<double>(numerator) /
              static_cast<double>(denominator);
}

std::uint64_t relative_cycle(std::uint64_t event,
                             std::uint64_t job_start) {
    return event >= job_start ? event - job_start : 0;
}

void section(std::ostream& stream, std::string_view title) {
    stream << "\n[" << title << "]\n";
}

void metric(std::ostream& stream, std::string_view name, std::string_view source,
            double value, std::string_view unit, int precision = 2) {
    stream << "  " << std::left << std::setw(31) << name << std::setw(5)
           << source << std::right << std::fixed << std::setprecision(precision)
           << std::setw(14) << value << " " << unit << '\n';
}

void metric_u64(std::ostream& stream, std::string_view name,
                std::string_view source, std::uint64_t value,
                std::string_view unit = {}) {
    stream << "  " << std::left << std::setw(31) << name << std::setw(5)
           << source << std::right << std::setw(14) << value;
    if (!unit.empty()) stream << " " << unit;
    stream << '\n';
}

} // namespace

const char* encoder_mode_name(std::uint32_t mode) {
    using namespace vpu_reg;
    switch (mode) {
    case MODE_PCM: return "pcm";
    case MODE_INTRA_DC: return "intra-dc";
    case MODE_HYBRID_DC: return "hybrid-dc";
    case MODE_INTRA_DC_TQ: return "intra-dc-tq";
    case MODE_INTRA_FULL_TQ: return "intra-full-tq";
    case MODE_INTRA_FULL_TQ16: return "intra-full-tq16";
    case MODE_INTRA_ADAPTIVE_TQ: return "intra-adaptive-tq";
    case MODE_INTRA_DIRECTIONAL_TQ: return "intra-directional-tq";
    default: return "unknown";
    }
}

void print_hardware_report(std::ostream& stream,
                           const HardwareReportInput& input,
                           const NativePipelineStats& stats) {
    const auto pixels_per_frame =
        static_cast<std::uint64_t>(input.width) * input.height;
    const auto total_pixels = pixels_per_frame * input.frames;
    const auto raw_bytes = total_pixels * 3U / 2U;
    const auto clock_hz = input.clock_mhz * 1'000'000.0;
    const auto latency_seconds = input.total_cycles == 0 || clock_hz == 0.0
        ? 0.0
        : static_cast<double>(input.total_cycles) / clock_hz;
    const auto total_dma_bytes = stats.dma_read_bytes + stats.dma_write_bytes;
    const auto total_stalls = stats.stall_input_full +
        stats.stall_prediction_full + stats.stall_transform_full +
        stats.stall_cabac_full;
    const auto active_sum = stats.dma_read_active + stats.prediction_active +
        stats.transform_active + stats.cabac_active + stats.dma_write_active;
    const auto fill_cycles = relative_cycle(stats.first_output_cycle,
                                            stats.job_start_cycle);
    const auto first_input_cycles = relative_cycle(stats.first_input_cycle,
                                                   stats.job_start_cycle);
    const auto drain_cycles = stats.last_output_cycle >= stats.last_input_cycle
        ? stats.last_output_cycle - stats.last_input_cycle
        : 0;

    stream << "\n==============================================================================\n"
           << " VPU SystemC/TLM HARDWARE METRIC DASHBOARD\n"
           << "==============================================================================\n"
           << " Sources: [M] measured  [D] derived  [S] setting  [A] external analytic\n"
           << " Accuracy: architectural cycle model; not RTL-correlated silicon timing\n";

    section(stream, "1 CONFIGURATION");
    stream << "  Resolution                     [S]  " << input.width << 'x'
           << input.height << " YUV420p8\n"
           << "  Frames / mode / QP             [S]  " << input.frames << " / "
           << encoder_mode_name(input.encoder_mode) << " / " << input.qp
           << '\n';
    metric(stream, "Clock", "[S]", input.clock_mhz, "MHz", 3);
    metric_u64(stream, "FIFO depth", "[S]", input.fifo_depth, "packets");
    metric_u64(stream, "DMA burst", "[S]", input.dma_burst_bytes, "bytes");
    metric_u64(stream, "SRAM read latency", "[S]",
               input.sram_read_latency_cycles, "cycles/access");
    metric_u64(stream, "SRAM write latency", "[S]",
               input.sram_write_latency_cycles, "cycles/access");
    metric_u64(stream, "SRAM transfer width", "[S]",
               input.sram_bytes_per_cycle, "bytes/cycle");

    section(stream, "2 PERFORMANCE");
    metric_u64(stream, "Total latency", "[M]", input.total_cycles, "cycles");
    metric(stream, "Total latency", "[D]", latency_seconds * 1000.0, "ms");
    metric(stream, "Throughput", "[D]",
           latency_seconds == 0.0 ? 0.0 : input.frames / latency_seconds,
           "frame/s");
    metric(stream, "Pixel throughput", "[D]",
           latency_seconds == 0.0
               ? 0.0
               : static_cast<double>(total_pixels) / latency_seconds / 1.0e6,
           "Mpixel/s");
    metric(stream, "DMA bandwidth", "[D]",
           latency_seconds == 0.0
               ? 0.0
               : static_cast<double>(total_dma_bytes) / latency_seconds / 1.0e9,
           "GB/s");
    metric(stream, "Cycles per frame", "[D]",
           ratio(input.total_cycles, input.frames), "cycles/frame", 1);
    metric_u64(stream, "Pipeline fill (first output)", "[M]", fill_cycles,
               "cycles");
    metric_u64(stream, "Input DMA first-frame ready", "[M]",
               first_input_cycles, "cycles");
    metric_u64(stream, "Pipeline drain", "[M]", drain_cycles, "cycles");

    section(stream, "3 CLOCKED PIPELINE");
    stream << "  " << std::left << std::setw(18) << "Stage" << std::right
           << std::setw(14) << "Active cyc" << std::setw(12) << "Frames"
           << std::setw(14) << "Cyc/frame" << std::setw(14) << "First done"
           << std::setw(11) << "Util %"
           << '\n';
    const std::array<std::tuple<std::string_view, std::uint64_t,
                                std::uint64_t, std::uint64_t>, 5> stages{{
        {"Input DMA", stats.dma_read_active, stats.frames_input,
         stats.first_input_cycle},
        {"Prediction", stats.prediction_active, stats.frames_prediction,
         stats.first_prediction_cycle},
        {"Transform/TQ", stats.transform_active, stats.frames_transform,
         stats.first_transform_cycle},
        {"CABAC/packer", stats.cabac_active, stats.frames_cabac,
         stats.first_cabac_cycle},
        {"Output DMA", stats.dma_write_active, stats.frames_output,
         stats.first_output_cycle},
    }};
    for (const auto& [name, active, frames, first_done] : stages) {
        stream << "  " << std::left << std::setw(18) << name << std::right
               << std::setw(14) << active << std::setw(12) << frames
               << std::fixed << std::setprecision(1) << std::setw(14)
               << ratio(active, frames) << std::setw(14)
               << relative_cycle(first_done, stats.job_start_cycle)
               << std::setprecision(2)
               << std::setw(11) << percent(active, input.total_cycles) << '\n';
    }
    metric(stream, "Average pipeline utilization", "[D]",
           input.total_cycles == 0
               ? 0.0
               : 100.0 * static_cast<double>(active_sum) /
                     (5.0 * static_cast<double>(input.total_cycles)),
           "%");

    section(stream, "4 FIFO / BACKPRESSURE");
    stream << "  " << std::left << std::setw(23) << "FIFO" << std::right
           << std::setw(12) << "Peak" << std::setw(12) << "Depth"
           << std::setw(16) << "Producer stall" << '\n'
           << "  " << std::left << std::setw(23) << "input" << std::right
           << std::setw(12) << stats.max_input_occupancy << std::setw(12)
           << input.fifo_depth << std::setw(16) << stats.stall_input_full
           << '\n'
           << "  " << std::left << std::setw(23) << "residual" << std::right
           << std::setw(12) << stats.max_residual_occupancy << std::setw(12)
           << input.fifo_depth << std::setw(16)
           << stats.stall_prediction_full << '\n'
           << "  " << std::left << std::setw(23) << "coefficient" << std::right
           << std::setw(12) << stats.max_coefficient_occupancy << std::setw(12)
           << input.fifo_depth << std::setw(16)
           << stats.stall_transform_full << '\n'
           << "  " << std::left << std::setw(23) << "bitstream" << std::right
           << std::setw(12) << stats.max_output_occupancy << std::setw(12)
           << input.fifo_depth << std::setw(16) << stats.stall_cabac_full
           << '\n';
    metric_u64(stream, "Concurrent stall sum", "[M]", total_stalls, "cycles");
    stream << "  Note: concurrent stalls may exceed total latency.\n";

    section(stream, "5 TLM DMA / SRAM TARGET");
    metric_u64(stream, "DMA read payload", "[M]", stats.dma_read_bytes,
               "bytes");
    metric_u64(stream, "DMA write payload", "[M]", stats.dma_write_bytes,
               "bytes");
    metric_u64(stream, "SRAM read transactions", "[M]",
               stats.dma_read_bursts, "bursts");
    metric_u64(stream, "SRAM write transactions", "[M]",
               stats.dma_write_bursts, "bursts");
    metric_u64(stream, "SRAM read response delay", "[M]",
               stats.dma_read_wait_cycles, "cycles");
    metric_u64(stream, "SRAM write response delay", "[M]",
               stats.dma_write_wait_cycles, "cycles");

    section(stream, "6 CODEC OUTPUT");
    metric_u64(stream, "Raw input", "[D]", raw_bytes, "bytes");
    metric_u64(stream, "HEVC Annex-B bitstream", "[M]",
               input.bitstream_bytes, "bytes");
    metric(stream, "Compression ratio", "[D]",
           ratio(raw_bytes, input.bitstream_bytes), "raw:bitstream");
    metric(stream, "Bits per pixel", "[D]",
           total_pixels == 0
               ? 0.0
               : 8.0 * static_cast<double>(input.bitstream_bytes) /
                     static_cast<double>(total_pixels),
           "bit/pixel");

    section(stream, "7 IMPLEMENTATION STATUS");
    stream << "  Area                          [A]             N/A  needs synthesis\n"
           << "  Power / energy                [A]             N/A  needs calibration\n"
           << "  RTL correlation               [A]             N/A  no RTL trace oracle\n"
           << "  PE array utilization          [A]             N/A  stage-pipeline architecture\n"
           << "  Control completion            [M]        DONE/IRQ  MMIO-visible handshake\n"
           << "  Bitstream generation          [M]          native  no x265/FFmpeg encoder\n"
           << "==============================================================================\n";
}

} // namespace model::systemc_native
