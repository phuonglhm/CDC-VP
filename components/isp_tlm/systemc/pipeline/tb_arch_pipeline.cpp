/**
 * @file tb_arch_pipeline.cpp
 * @brief Focused in-memory contract test for the line-granular pipeline.
 */

#include <systemc>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "sc_isp_pipeline.h"
#include "../hw/isp_arch_config.h"
#include "../hw/metrics.h"
#include "../../pipeline/include/isp_pipeline.h"
#include "../../pipeline/include/isp_regmap.h"

using namespace sc_core;
using namespace cdc::components;

namespace {

constexpr std::uint32_t kWidth = 31;
constexpr std::uint32_t kHeight = 33;
constexpr std::uint32_t kInputBitDepth = 12;
constexpr std::uint32_t kPixelsPerCycle = 2;
constexpr std::size_t kMaxSimulationSteps = 10000;

bool require(bool condition, const std::string& message) {
    if (condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

std::vector<std::uint16_t> make_input() {
    std::vector<std::uint16_t> input(
        static_cast<std::size_t>(kWidth) * kHeight);
    for (std::size_t index = 0; index < input.size(); ++index) {
        input[index] = static_cast<std::uint16_t>((index * 19u + 7u) & 0x0fffu);
    }
    return input;
}

}  // namespace

int sc_main(int argc, char* argv[]) {
    std::string metrics_path;
    bool print_metrics = false;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--metrics" && index + 1 < argc) {
            metrics_path = argv[++index];
        } else if (argument == "--metrics") {
            std::cerr << "usage: tb_arch_pipeline [--print-metrics] [--metrics <file>]\n";
            return 1;
        } else if (argument == "--print-metrics") {
            print_metrics = true;
        }
    }
    isp_arch_config arch_cfg;
    arch_cfg.init_defaults();
    arch_cfg.clock_freq_mhz = 200.0f;
    for (auto& block : arch_cfg.blocks) {
        block.pixels_per_cycle = kPixelsPerCycle;
    }

    isp_pipeline oracle;
    oracle.write_reg(REG_WIDTH, kWidth);
    oracle.write_reg(REG_HEIGHT, kHeight);
    oracle.write_reg(REG_BIT_DEPTH, kInputBitDepth);
    oracle.write_reg(REG_DPC_ENABLE, 0);
    oracle.write_reg(REG_YUV420_ENABLE, 1);
    const isp_config functional_cfg = oracle.config();

    const std::vector<std::uint16_t> input = make_input();
    std::vector<std::uint8_t> oracle_output;
    oracle.run(input.data(), oracle_output);
    const std::size_t expected_input_bytes = input.size() * sizeof(std::uint16_t);
    const std::size_t configured_output_bytes =
        static_cast<std::size_t>(kWidth) * kHeight +
        2u * static_cast<std::size_t>((kWidth + 1u) / 2u) *
            ((kHeight + 1u) / 2u);
    const bool oracle_has_configured_output =
        oracle_output.size() >= configured_output_bytes;
    if (oracle_has_configured_output) oracle_output.resize(configured_output_bytes);
    const std::size_t expected_output_bytes = configured_output_bytes;

    sc_fifo<std::uint16_t> raw_in("raw_in", input.size() + 16u);
    sc_fifo<std::uint8_t> yuv_out("yuv_out", expected_output_bytes + 16u);
    const std::vector<float> lsc_lut(8192, 1.0f);
    sc_isp_pipeline pipeline("isp_pipeline", functional_cfg, lsc_lut,
                             &raw_in, &yuv_out, kInputBitDepth,
                             cfa_types::RGGB, &arch_cfg);

    for (const auto sample : input) raw_in.write(sample);
    std::size_t steps = 0;
    while (static_cast<std::size_t>(yuv_out.num_available()) < expected_output_bytes &&
           steps++ < kMaxSimulationSteps) {
        sc_start(sc_time(1, SC_US));
    }

    std::vector<std::uint8_t> captured;
    while (yuv_out.num_available() > 0) captured.push_back(yuv_out.read());
    const isp_tlm::pipeline_metrics snapshot = pipeline.metrics();
    const auto& frame = snapshot.frame;

    bool passed = true;
    passed &= require(oracle_has_configured_output,
                      "oracle output contains configured YUV420 byte layout");
    passed &= require(captured.size() == expected_output_bytes,
                      "configured YUV output bytes complete");
    passed &= require(captured == oracle_output,
                      "output bytes preserve oracle parity");
    passed &= require(frame.frame_cycles_available,
                      "frame cycles are available");
    passed &= require(frame.input_bytes_available &&
                          frame.input_bytes == expected_input_bytes,
                      "RAW16 input byte count is available and exact");
    passed &= require(frame.output_bytes_available &&
                          frame.output_bytes == expected_output_bytes,
                      "configured YUV output byte count is available and exact");
    passed &= require(frame.achieved_pixels_per_cycle_available,
                      "achieved pixels per cycle are available");
    passed &= require(frame.input_bandwidth_available &&
                          frame.output_bandwidth_available,
                      "logical bandwidth metrics are available");
    passed &= require(snapshot.blocks.size() == isp_blocks::COUNT,
                      "snapshot contains exactly 18 blocks");
    passed &= require(snapshot.links.size() == isp_links::COUNT,
                      "snapshot contains exactly 18 links");

    if (snapshot.blocks.size() == isp_blocks::COUNT &&
        snapshot.links.size() == isp_links::COUNT) {
        for (std::size_t index = 0; index < snapshot.links.size(); ++index) {
            const auto& link = snapshot.links[index];
            passed &= require(link.published_lines == link.read_lines &&
                                  link.read_lines == link.released_lines,
                              "stage/link line conservation at link " +
                                  std::to_string(index));
            passed &= require(link.producer_wait_events == 0 &&
                                  link.producer_wait_cycles == 0,
                              "default deep link has no producer credit blocking at link " +
                                  std::to_string(index));
            passed &= require(snapshot.blocks[index].output_lines ==
                                  link.published_lines,
                              "stage output and link publication conserve lines at link " +
                                  std::to_string(index));
            if (index + 1 < snapshot.blocks.size()) {
                passed &= require(snapshot.blocks[index + 1].input_lines ==
                                      link.read_lines,
                                  "link read and next stage input conserve lines at link " +
                                      std::to_string(index));
            }
        }
        for (std::size_t index = 0; index < snapshot.blocks.size(); ++index) {
            const std::uint64_t full_width_beats =
                static_cast<std::uint64_t>(kHeight) *
                isp_tlm::processing_beats(kWidth, kPixelsPerCycle);
            const std::uint64_t expected_beats =
                index == isp_blocks::YUV420
                    ? full_width_beats +
                          static_cast<std::uint64_t>((kHeight + 1u) / 2u) *
                              isp_tlm::processing_beats(
                                  (kWidth + 1u) / 2u, kPixelsPerCycle)
                    : full_width_beats;
            passed &= require(
                snapshot.blocks[index].processing_beats == expected_beats,
                "canonical processing beats sum ceil(line pixels/PPC)");
        }

        const auto& disabled = snapshot.blocks[isp_blocks::DPC];
        passed &= require(!disabled.enabled && disabled.bypass_cycles > 0 &&
                              disabled.active_cycles == 0,
                          "disabled functional block reports bypass and no active cycles");
        passed &= require(!disabled.operations.available &&
                              disabled.operations.additions == 0 &&
                              disabled.operations.multiplications == 0 &&
                              disabled.operations.comparisons == 0 &&
                              disabled.operations.reads == 0 &&
                              disabled.operations.writes == 0,
                          "disabled functional block operations are unavailable and zero");
    }
    if (print_metrics) {
        passed &= require(isp_tlm::write_metrics(std::cout, snapshot),
                          "requested metric report print succeeds");
    }

    if (!metrics_path.empty()) {
        passed &= require(pipeline.write_metrics(metrics_path),
                          "requested metric report write succeeds");
    }

    sc_stop();
    std::cout << (passed ? "PASS" : "FAIL")
              << ": in-memory architecture metric contract\n";
    return passed ? 0 : 1;
}
