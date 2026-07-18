/**
 * @file tb_arch_sweep.cpp
 * @brief Bounded architecture sweeps over the line-granular SystemC ISP.
 *
 * Every point is elaborated before the first sc_start call.  This is
 * intentional: SystemC does not permit creating another module after
 * elaboration has begun, while sweep_runner invokes its callback synchronously.
 */

#include <systemc>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../hw/isp_arch_config.h"
#include "../hw/metrics.h"
#include "../hw/power.h"
#include "../hw/sweep.h"
#include "../pipeline/sc_isp_pipeline.h"
#include "../../pipeline/include/isp_regmap.h"

using namespace sc_core;
using namespace cdc::components;

namespace {

constexpr std::uint32_t kInputBitDepth = 12;
constexpr std::size_t kFifoDepth = 256;
const sc_core::sc_time kSimulationStep(1, sc_core::SC_US);
constexpr std::size_t kMaxSimulationSteps = 10000;

std::vector<std::uint16_t> make_frame(std::uint32_t width, std::uint32_t height) {
    std::vector<std::uint16_t> frame(static_cast<std::size_t>(width) * height);
    for (std::size_t i = 0; i < frame.size(); ++i) {
        // A deterministic pattern that exercises both low and high sensor values.
        frame[i] = static_cast<std::uint16_t>((i * 37u + 113u) & 0x0fffu);
    }
    return frame;
}

bool enabled_value(const std::map<std::string, std::string>& params,
                   const char* name) {
    const auto enabled = [&](const std::string& key) {
        const auto it = params.find(key);
        return it == params.end() || it->second != "off";
    };
    const std::string plain(name);
    const std::string suffixed = plain + "_enable";
    return enabled(plain) && enabled(suffixed);
}

std::uint32_t block_id_for_name(const std::string& name) {
    if (name == "dpc") return isp_blocks::DPC;
    if (name == "bnr") return isp_blocks::BNR;
    if (name == "sharpen") return isp_blocks::SHARPEN;
    if (name == "lsc") return isp_blocks::LSC;
    if (name == "demosaic") return isp_blocks::DEMOSAIC;
    if (name == "2dnr") return isp_blocks::TWO_DNR;
    if (name == "scale") return isp_blocks::SCALE;
    if (name == "yuv420") return isp_blocks::YUV420;
    return isp_blocks::COUNT;
}

void set_functional_enable(isp_pipeline& pipeline, const std::string& name,
                           bool enable) {
    const std::uint32_t value = enable ? 1u : 0u;
    if (name == "dpc") pipeline.write_reg(REG_DPC_ENABLE, value);
    else if (name == "bnr") pipeline.write_reg(REG_BNR_ENABLE, value);
    else if (name == "sharpen") pipeline.write_reg(REG_SHARPEN_ENABLE, value);
    else if (name == "lsc") pipeline.write_reg(REG_LSC_ENABLE, value);
    else if (name == "demosaic") pipeline.write_reg(REG_DEMOSAIC_ENABLE, value);
    else if (name == "2dnr") pipeline.write_reg(REG_2DNR_ENABLE, value);
    else if (name == "scale") pipeline.write_reg(REG_SCALE_ENABLE, value);
    else if (name == "yuv420") pipeline.write_reg(REG_YUV420_ENABLE, value);
}

std::map<std::string, std::string> merge_params(
    const std::map<std::string, std::string>& fixed,
    const std::map<std::string, std::string>& current) {
    std::map<std::string, std::string> result = fixed;
    result.insert(current.begin(), current.end());
    return result;
}

std::string point_name(std::size_t index) {
    return "arch_point_" + std::to_string(index);
}

struct pipeline_point {
    std::map<std::string, std::string> params;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint16_t> input;
    std::vector<std::uint8_t> oracle_output;
    isp_config functional_config;
    isp_arch_config architecture_config;
    hw_params hardware;
    std::unique_ptr<sc_fifo<std::uint16_t>> raw_fifo;
    std::unique_ptr<sc_fifo<std::uint8_t>> yuv_fifo;
    std::unique_ptr<sc_isp_pipeline> pipeline;

    pipeline_point(std::size_t index,
                   const std::map<std::string, std::string>& candidate)
        : params(candidate) {
        const auto resolution_it = params.find("resolution");
        const resolution_spec resolution =
            resolution_it == params.end()
                ? resolution_spec{8, 6}
                : resolution_spec::from_string(resolution_it->second);
        width = resolution.width;
        height = resolution.height;
        if (width == 0 || height == 0) {
            throw std::invalid_argument("invalid sweep resolution");
        }

        const auto frequency_it = params.find("frequency");
        const float frequency =
            frequency_it == params.end()
                ? 200.0f
                : static_cast<float>(std::atof(frequency_it->second.c_str()));
        if (frequency <= 0.0f) {
            throw std::invalid_argument("invalid sweep frequency");
        }

        isp_pipeline oracle;
        oracle.write_reg(REG_WIDTH, width);
        oracle.write_reg(REG_HEIGHT, height);
        oracle.write_reg(REG_BIT_DEPTH, kInputBitDepth);
        for (const char* block : {"dpc", "bnr", "sharpen"}) {
            set_functional_enable(oracle, block, enabled_value(params, block));
        }
        // The bounded sweep deliberately leaves rate-changing blocks disabled,
        // so resolution remains a direct frame-size parameter for both models.
        functional_config = oracle.config();

        architecture_config.init_defaults();
        architecture_config.clock_freq_mhz = frequency;
        architecture_config.enable_metrics = true;
        hardware.clk_mhz = frequency;
        hardware.fifo_depth = kFifoDepth;
        hardware.timed_mode = true;

        // Block enables are functional settings; all supported architectural
        // settings are still explicitly initialized for this candidate.
        for (const char* block : {"dpc", "bnr", "sharpen"}) {
            const std::size_t block_id = block_id_for_name(block);
            if (block_id < isp_blocks::COUNT && !enabled_value(params, block)) {
                architecture_config.blocks[block_id].clock_gating = true;
            }
        }

        input = make_frame(width, height);
        oracle.run(input.data(), oracle_output);

        raw_fifo = std::make_unique<sc_fifo<std::uint16_t>>(
            (point_name(index) + "_raw").c_str(), kFifoDepth);
        yuv_fifo = std::make_unique<sc_fifo<std::uint8_t>>(
            (point_name(index) + "_yuv").c_str(),
            std::max<std::size_t>(kFifoDepth,
                                  static_cast<std::size_t>(width) * height * 3u + 16u));
        pipeline = std::make_unique<sc_isp_pipeline>(
            point_name(index).c_str(), functional_config, std::vector<float>(8192, 1.0f),
            raw_fifo.get(), yuv_fifo.get(), kInputBitDepth, cfa_types::RGGB,
            &hardware, &architecture_config);
        pipeline->enable_arch_metrics("output/sweeps/arch_metrics/" +
                                      point_name(index));
    }
};

class pipeline_sweep_runtime {
public:
    explicit pipeline_sweep_runtime(const sweep_config& config)
        : m_config(config) {
        std::map<std::string, std::string> current;
        enumerate(current, 0);
    }

    explicit pipeline_sweep_runtime(
        const std::vector<std::map<std::string, std::string>>& candidates) {
        for (const auto& candidate : candidates) {
            add_point(candidate);
        }
    }

    sweep_result run(const std::map<std::string, std::string>& current) {
        const auto candidate = merge_params(m_config.fixed_params, current);
        const auto point_it = m_points.find(candidate);
        sweep_result result;
        result.params = candidate;
        if (point_it == m_points.end()) {
            result.passed = false;
            result.error_message = "candidate was not elaborated before sc_start";
            return result;
        }

        pipeline_point& point = *point_it->second;
        result = execute(point);
        result.params = candidate;
        return result;
    }

private:
    sweep_config m_config;
    std::map<std::map<std::string, std::string>,
             std::unique_ptr<pipeline_point>> m_points;

    void enumerate(std::map<std::string, std::string>& current,
                   std::size_t parameter_index) {
        if (parameter_index >= m_config.params.size()) {
            add_point(merge_params(m_config.fixed_params, current));
            return;
        }
        const sweep_param& parameter = m_config.params[parameter_index];
        if (!parameter.discrete_values.empty()) {
            for (const auto& value : parameter.discrete_values) {
                current[parameter.name] = value;
                enumerate(current, parameter_index + 1);
            }
        } else if (parameter.range_step > 0.0) {
            for (double value = parameter.range_min;
                 value <= parameter.range_max + 0.001;
                 value += parameter.range_step) {
                std::ostringstream value_stream;
                value_stream << std::fixed << std::setprecision(0) << value;
                current[parameter.name] = value_stream.str();
                enumerate(current, parameter_index + 1);
            }
        }
    }

    static std::size_t allocate_point_index() {
        static std::size_t next = 0;
        return next++;
    }

    void add_point(const std::map<std::string, std::string>& candidate) {
        if (m_points.find(candidate) != m_points.end()) {
            return;
        }
        m_points.emplace(candidate,
                         std::make_unique<pipeline_point>(
                             allocate_point_index(), candidate));
    }

    static sweep_result execute(pipeline_point& point) {
        sweep_result result;
        result.params = point.params;

        for (const std::uint16_t sample : point.input) {
            point.raw_fifo->write(sample);
        }

        std::size_t steps = 0;
        const std::size_t expected = point.oracle_output.size();
        while (static_cast<std::size_t>(point.yuv_fifo->num_available()) < expected &&
               steps++ < kMaxSimulationSteps) {
            sc_start(kSimulationStep);
        }

        std::vector<std::uint8_t> actual;
        while (point.yuv_fifo->num_available() > 0) {
            actual.push_back(point.yuv_fifo->read());
        }

        const bool complete = actual.size() == expected;
        const bool parity = complete && actual == point.oracle_output;
        if (!complete || !parity) {
            result.passed = false;
            result.error_message = !complete
                ? "line pipeline output incomplete: expected " +
                      std::to_string(expected) + " bytes, got " +
                      std::to_string(actual.size())
                : "line pipeline output differs from isp_pipeline::run()";
        }

        const sc_time frame_time = point.pipeline->get_frame_time();
        result.frame_time_us = frame_time.to_seconds() / 1e-6;
        result.fps = result.frame_time_us > 0.0 ? 1e6 / result.frame_time_us : 0.0;
        result.sim_time_seconds = frame_time.to_seconds();
        result.sim_cycles = point.hardware.cycle_ns() > 0.0f
            ? static_cast<std::uint64_t>(
                  std::ceil(frame_time.to_seconds() /
                            (point.hardware.cycle_ns() * 1e-9f)))
            : 0;
        result.throughput_mpixel_s =
            result.frame_time_us > 0.0
                ? static_cast<double>(point.width) * point.height /
                      result.frame_time_us
                : 0.0;
        result.bandwidth_input_mbps =
            result.frame_time_us > 0.0
                ? static_cast<double>(point.input.size() * sizeof(std::uint16_t) * 8u) /
                      result.frame_time_us / 1000.0
                : 0.0;
        result.bandwidth_output_mbps =
            result.frame_time_us > 0.0
                ? static_cast<double>(actual.size() * sizeof(std::uint8_t) * 8u) /
                      result.frame_time_us / 1000.0
                : 0.0;

        point.pipeline->collect_block_metrics();
        arch_metrics_collector* metrics = point.pipeline->get_arch_metrics();
        metrics->set_total_cycles(result.sim_cycles);
        power_estimator estimator;
        metrics->set_power_estimator(&estimator);
        metrics->calculate_block_power();
        const pipeline_power_summary power = metrics->get_power_summary();
        result.avg_power_mw = power.total.total_mw;
        result.peak_power_mw = power.peak_power_mw;
        result.frame_energy_nj =
            estimator.energy_per_frame(power.total, result.frame_time_us);

        double utilization_sum = 0.0;
        for (const auto& block : metrics->blocks()) {
            utilization_sum += block.block_utilization;
        }
        result.avg_block_utilization = metrics->blocks().empty()
            ? 0.0
            : utilization_sum / metrics->blocks().size();
        const bottleneck_report bottleneck = metrics->analyze_bottleneck();
        result.bottleneck_location = bottleneck.location;
        result.bottleneck_severity = bottleneck.severity;
        if (result.bottleneck_location.empty()) {
            result.bottleneck_location = "none";
        }
        return result;
    }
};

sweep_config bounded_resolution_config() {
    sweep_config config;
    config.name = "resolution_sweep";
    config.description = "Bounded line-pipeline resolution sweep";
    config.add_resolution_sweep({{8, 6}, {12, 8}, {16, 10}});
    config.add_fixed_param("frequency", "200");
    config.output_dir = "output/sweeps/resolution";
    return config;
}

sweep_config bounded_frequency_config() {
    sweep_config config;
    config.name = "frequency_sweep";
    config.description = "Bounded line-pipeline frequency sweep";
    config.add_custom_sweep("resolution", {"12x8"}, "Bounded frame");
    config.params.back().type = sweep_param_type::RESOLUTION;
    config.add_frequency_sweep(100, 300, 100);
    config.output_dir = "output/sweeps/frequency";
    return config;
}

sweep_config bounded_block_config() {
    sweep_config config;
    config.name = "block_enable_sweep";
    config.description = "Bounded line-pipeline block-enable sweep";
    config.add_custom_sweep("resolution", {"12x8"}, "Bounded frame");
    config.params.back().type = sweep_param_type::RESOLUTION;
    config.add_fixed_param("frequency", "200");
    for (const auto& block : {"dpc", "bnr", "sharpen"}) {
        config.add_block_enable_sweep(block, {"on", "off"});
    }
    config.output_dir = "output/sweeps/block_enable";
    return config;
}

void run_sweep(const sweep_config& config, pipeline_sweep_runtime& runtime) {
    sweep_runner runner(config);
    runner.set_run_callback([&runtime](
        const std::map<std::string, std::string>& params) {
        return runtime.run(params);
    });
    sweep_results results = runner.run();
    const std::string filename = config.output_dir + "/" + config.name + ".csv";
    results.print_summary();
    std::filesystem::create_directories(config.output_dir);
    results.export_csv(filename);
    std::cout << "\nExported to: " << filename << "\n";
}

void run_quick_comparison(pipeline_sweep_runtime& runtime) {
    const std::vector<std::map<std::string, std::string>> candidates = {
        {{"resolution", "8x6"}, {"frequency", "200"},
         {"dpc", "on"}, {"bnr", "on"}, {"sharpen", "on"}},
        {{"resolution", "12x8"}, {"frequency", "200"},
         {"dpc", "on"}, {"bnr", "off"}, {"sharpen", "on"}},
        {{"resolution", "12x8"}, {"frequency", "400"},
         {"dpc", "on"}, {"bnr", "on"}, {"sharpen", "off"}},
    };
    sweep_results results;
    results.sweep_name = "quick_comparison";
    for (const auto& candidate : candidates) {
        const sweep_result result = runtime.run(candidate);
        results.add_result(result);
    }
    results.print_summary();
    std::filesystem::create_directories("output/sweeps/quick_comparison");
    results.export_csv("output/sweeps/quick_comparison/results.csv");
}

}  // namespace

int sc_main(int argc, char* argv[]) {
    bool run_all = false;
    bool run_res = false;
    bool run_freq = false;
    bool run_block = false;
    bool run_compare = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--all" || arg == "-a") run_all = true;
        else if (arg == "--resolution" || arg == "-r") run_res = true;
        else if (arg == "--frequency" || arg == "-f") run_freq = true;
        else if (arg == "--blocks" || arg == "-b") run_block = true;
        else if (arg == "--compare" || arg == "-c") run_compare = true;
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: tb_arch_sweep [options]\n"
                      << "  --all, -a          Run bounded sweeps (default)\n"
                      << "  --resolution, -r  Run resolution sweep\n"
                      << "  --frequency, -f   Run frequency sweep\n"
                      << "  --blocks, -b      Run block-enable sweep\n"
                      << "  --compare, -c     Run bounded line-pipeline comparison\n";
            return 0;
        }
    }

    if (run_compare) {
        const std::vector<std::map<std::string, std::string>> candidates = {
            {{"resolution", "8x6"}, {"frequency", "200"},
             {"dpc", "on"}, {"bnr", "on"}, {"sharpen", "on"}},
            {{"resolution", "12x8"}, {"frequency", "200"},
             {"dpc", "on"}, {"bnr", "off"}, {"sharpen", "on"}},
            {{"resolution", "12x8"}, {"frequency", "400"},
             {"dpc", "on"}, {"bnr", "on"}, {"sharpen", "off"}},
        };
        pipeline_sweep_runtime runtime(candidates);
        run_quick_comparison(runtime);
        sc_stop();
        return 0;
    }

    // Construct every module before the first run callback can call sc_start.
    // This keeps --all within SystemC's single-elaboration lifecycle.
    std::unique_ptr<pipeline_sweep_runtime> resolution_runtime;
    std::unique_ptr<pipeline_sweep_runtime> frequency_runtime;
    std::unique_ptr<pipeline_sweep_runtime> block_runtime;
    const sweep_config resolution_config = bounded_resolution_config();
    const sweep_config frequency_config = bounded_frequency_config();
    const sweep_config block_config = bounded_block_config();
    if (run_all || run_res) {
        resolution_runtime = std::make_unique<pipeline_sweep_runtime>(resolution_config);
    }
    if (run_all || run_freq) {
        frequency_runtime = std::make_unique<pipeline_sweep_runtime>(frequency_config);
    }
    if (run_all || run_block) {
        block_runtime = std::make_unique<pipeline_sweep_runtime>(block_config);
    }

    if (resolution_runtime) run_sweep(resolution_config, *resolution_runtime);
    if (frequency_runtime) run_sweep(frequency_config, *frequency_runtime);
    if (block_runtime) run_sweep(block_config, *block_runtime);

    std::cout << "\nSweep complete; CSV results are under output/sweeps/.\n";
    sc_stop();
    return 0;
}
