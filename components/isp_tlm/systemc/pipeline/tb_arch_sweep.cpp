/**
 * @file tb_arch_sweep.cpp
 * @brief Real line-pipeline architecture sweeps.
 *
 * Every candidate is constructed before simulation starts.  Each callback then
 * feeds a real frame through the SystemC model and consumes its metric snapshot.
 */

#include <systemc>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
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
#include "../hw/sweep.h"
#include "sc_isp_pipeline.h"
#include "../../pipeline/include/isp_regmap.h"

using namespace sc_core;
using namespace cdc::components;

namespace {

constexpr std::uint32_t kInputBitDepth = 12;
constexpr std::uint32_t kDefaultPpc = 1;
constexpr std::uint32_t kDefaultPixelIi = 1;
constexpr std::uint32_t kDefaultLinkDepth = 256;
constexpr std::size_t kMaxSimulationSteps = 10000;
const sc_time kSimulationStep(1, SC_US);
constexpr std::uint32_t kDefaultPipelineLatency = 1;
constexpr std::uint32_t kDefaultMaxInFlight = 2;
using candidate_params = std::map<std::string, std::string>;

std::vector<std::uint16_t> make_frame(std::uint32_t width,
                                      std::uint32_t height) {
    std::vector<std::uint16_t> frame(static_cast<std::size_t>(width) * height);
    for (std::size_t index = 0; index < frame.size(); ++index) {
        frame[index] = static_cast<std::uint16_t>((index * 37u + 113u) & 0x0fffu);
    }
    return frame;
}

std::uint32_t parameter_u32(const std::map<std::string, std::string>& params,
                            const char* name, std::uint32_t fallback) {
    const auto it = params.find(name);
    if (it == params.end()) return fallback;
    const unsigned long value = std::strtoul(it->second.c_str(), nullptr, 10);
    if (value == 0 || value > 0xfffffffful) {
        throw std::invalid_argument(std::string("invalid ") + name);
    }
    return static_cast<std::uint32_t>(value);
}

float parameter_frequency(const std::map<std::string, std::string>& params) {
    const auto it = params.find("frequency");
    const float frequency = it == params.end()
        ? 200.0f
        : static_cast<float>(std::atof(it->second.c_str()));
    if (frequency <= 0.0f) {
        throw std::invalid_argument("invalid frequency");
    }
    return frequency;
}

bool enabled_value(const std::map<std::string, std::string>& params,
                   const char* name) {
    const auto it = params.find(name);
    return it == params.end() || it->second != "off";
}

void set_functional_enable(isp_pipeline& pipeline, const std::string& name,
                           bool enable) {
    const std::uint32_t value = enable ? 1u : 0u;
    if (name == "dpc") pipeline.write_reg(REG_DPC_ENABLE, value);
    else if (name == "bnr") pipeline.write_reg(REG_BNR_ENABLE, value);
    else if (name == "sharpen") pipeline.write_reg(REG_SHARPEN_ENABLE, value);
}
std::map<std::string, std::string> merge_params(
    const candidate_params& fixed,
    const candidate_params& current) {
    candidate_params result = fixed;
    result.insert(current.begin(), current.end());
    return result;
}

bool has_param(const candidate_params& params, const char* name) {
    return params.find(name) != params.end();
}
struct pipeline_point {
    candidate_params params;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    float frequency_mhz = 0.0f;
    std::vector<std::uint16_t> input;
    std::vector<std::uint8_t> oracle_output;
    isp_config functional_config;
    isp_arch_config architecture_config;
    std::unique_ptr<sc_fifo<std::uint16_t>> raw_fifo;
    std::unique_ptr<sc_fifo<std::uint8_t>> yuv_fifo;
    std::unique_ptr<sc_isp_pipeline> pipeline;

    pipeline_point(std::size_t index,
                   const candidate_params& candidate)
        : params(candidate), frequency_mhz(parameter_frequency(candidate)) {
        const auto resolution_it = params.find("resolution");
        const resolution_spec resolution = resolution_it == params.end()
            ? resolution_spec{8, 6}
            : resolution_spec::from_string(resolution_it->second);
        width = resolution.width;
        height = resolution.height;
        if (width == 0 || height == 0) {
            throw std::invalid_argument("invalid sweep resolution");
        }

        isp_pipeline oracle;
        oracle.write_reg(REG_WIDTH, width);
        oracle.write_reg(REG_HEIGHT, height);
        oracle.write_reg(REG_BIT_DEPTH, kInputBitDepth);
        for (const char* block : {"dpc", "bnr", "sharpen"}) {
            set_functional_enable(oracle, block, enabled_value(params, block));
        }
        functional_config = oracle.config();

        architecture_config.init_defaults();
        architecture_config.clock_freq_mhz = frequency_mhz;
        const std::uint32_t ppc = parameter_u32(params, "ppc", kDefaultPpc);
        const std::uint32_t pixel_ii =
            parameter_u32(params, "pixel_ii", kDefaultPixelIi);
        const std::uint32_t link_depth =
            parameter_u32(params, "link_depth", kDefaultLinkDepth);
        const std::uint32_t pipeline_latency =
            parameter_u32(params, "pipeline_latency", kDefaultPipelineLatency);
        const std::uint32_t max_in_flight =
            parameter_u32(params, "max_in_flight", kDefaultMaxInFlight);
        const bool has_memory_profile =
            has_param(params, "memory_read_ports") ||
            has_param(params, "memory_write_ports") ||
            has_param(params, "memory_access_cycles");
        const std::uint32_t memory_read_ports =
            parameter_u32(params, "memory_read_ports", 8);
        const std::uint32_t memory_write_ports =
            parameter_u32(params, "memory_write_ports", 8);
        const std::uint32_t memory_access_cycles =
            parameter_u32(params, "memory_access_cycles", 1);
        const std::uint32_t downstream_latency =
            parameter_u32(params, "downstream_latency", pipeline_latency);
        const std::uint32_t downstream_max_in_flight =
            parameter_u32(params, "downstream_max_in_flight", max_in_flight);
        for (auto& block : architecture_config.blocks) {
            block.pixels_per_cycle = ppc;
            block.pixel_initiation_interval_cycles = pixel_ii;
            if (has_param(params, "pipeline_latency")) {
                block.pipeline_latency_cycles = pipeline_latency;
            }
            if (has_param(params, "max_in_flight")) {
                block.max_in_flight_lines = max_in_flight;
            }
            if (has_memory_profile) {
                block.workload = workload_profile{};
                block.workload.available = true;
                block.workload.provenance = metric_provenance::modeled;
                block.workload.additions_per_pixel = 1;
                block.workload.multiplications_per_pixel = 1;
                block.workload.comparisons_per_pixel = 1;
                block.workload.reads_per_pixel = 4;
                block.workload.writes_per_pixel = 2;
                block.local_memory = local_memory_service_profile{};
                block.local_memory.available = true;
                block.local_memory.provenance = metric_provenance::modeled;
                block.local_memory.read_ports = memory_read_ports;
                block.local_memory.write_ports = memory_write_ports;
                block.local_memory.access_cycles = memory_access_cycles;
            }
        }
        if (has_param(params, "downstream_latency")) {
            auto& downstream = architecture_config.blocks[isp_blocks::YUV420];
            downstream.pipeline_latency_cycles = downstream_latency;
            downstream.max_in_flight_lines = downstream_max_in_flight;
        }
        for (auto& link : architecture_config.links) {
            link.depth = link_depth;
        }

        input = make_frame(width, height);
        oracle.run(input.data(), oracle_output);

        raw_fifo = std::make_unique<sc_fifo<std::uint16_t>>(
            ("arch_point_" + std::to_string(index) + "_raw").c_str(),
            input.size() + 16u);
        yuv_fifo = std::make_unique<sc_fifo<std::uint8_t>>(
            ("arch_point_" + std::to_string(index) + "_yuv").c_str(),
            oracle_output.size() + 16u);
        pipeline = std::make_unique<sc_isp_pipeline>(
            ("arch_point_" + std::to_string(index)).c_str(), functional_config,
            std::vector<float>(8192, 1.0f), raw_fifo.get(), yuv_fifo.get(),
            kInputBitDepth, cfa_types::RGGB, &architecture_config);
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
        const std::vector<candidate_params>& candidates) {
        for (const auto& candidate : candidates) add_point(candidate);
    }

    sweep_result run(const std::map<std::string, std::string>& current) {
        const auto candidate = merge_params(m_config.fixed_params, current);
        const auto point_it = m_points.find(candidate);
        sweep_result result;
        result.params = candidate;
        if (point_it == m_points.end()) {
            result.error_message = "candidate was not elaborated before sc_start";
            return result;
        }
        result = execute(*point_it->second);
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
                std::ostringstream stream;
                stream << std::fixed << std::setprecision(3) << value;
                current[parameter.name] = stream.str();
                enumerate(current, parameter_index + 1);
            }
        }
    }

    void add_point(const std::map<std::string, std::string>& candidate) {
        if (m_points.find(candidate) != m_points.end()) return;
        const std::size_t index = m_points.size();
        m_points.emplace(candidate,
                         std::make_unique<pipeline_point>(index, candidate));
    }

    static sweep_result execute(pipeline_point& point) {
        sweep_result result;
        result.params = point.params;
        for (const auto sample : point.input) point.raw_fifo->write(sample);

        const sc_time simulation_start = sc_time_stamp();
        std::size_t steps = 0;
        const std::size_t expected = point.oracle_output.size();
        while (static_cast<std::size_t>(point.yuv_fifo->num_available()) < expected &&
               steps++ < kMaxSimulationSteps) {
            sc_start(kSimulationStep);
        }
        const sc_time simulation_end = sc_time_stamp();
        result.simulation_time_seconds =
            (simulation_end - simulation_start).to_seconds();

        std::vector<std::uint8_t> actual;
        while (point.yuv_fifo->num_available() > 0) actual.push_back(point.yuv_fifo->read());
        const bool complete = actual.size() == expected;
        result.parity = complete && actual == point.oracle_output;

        const isp_tlm::pipeline_metrics snapshot = point.pipeline->metrics();
        const auto& frame = snapshot.frame;
        result.frame_cycles = frame.frame_cycles;
        result.frame_cycles_available = frame.frame_cycles_available;
        result.first_output_latency_cycles = frame.first_output_latency_cycles;
        result.first_output_latency_available =
            frame.first_output_latency_available;
        result.achieved_pixels_per_cycle = frame.achieved_pixels_per_cycle;
        result.achieved_pixels_per_cycle_available =
            frame.achieved_pixels_per_cycle_available;
        result.bandwidth_input_mbps = frame.input_bandwidth_mbps;
        result.bandwidth_input_available = frame.input_bandwidth_available;
        result.bandwidth_output_mbps = frame.output_bandwidth_mbps;
        result.bandwidth_output_available = frame.output_bandwidth_available;
        if (result.frame_cycles_available) {
            result.frame_time_us =
                static_cast<double>(result.frame_cycles) /
                static_cast<double>(point.frequency_mhz);
            result.frame_time_available = true;
        }

        double utilization_sum = 0.0;
        for (const auto& block : snapshot.blocks) utilization_sum += block.utilization;
        result.avg_block_utilization = snapshot.blocks.empty()
            ? 0.0
            : utilization_sum / static_cast<double>(snapshot.blocks.size());
        if (snapshot.bottlenecks.empty()) {
            result.top_bottleneck_type = "none";
            result.top_bottleneck_location = "none";
            result.top_bottleneck_severity = 0.0;
            result.top_bottleneck_evidence = "none";
        } else {
            const auto& bottleneck = snapshot.bottlenecks.front();
            result.top_bottleneck_type = bottleneck.type;
            result.top_bottleneck_location = bottleneck.location;
            result.top_bottleneck_severity = bottleneck.severity;
            result.top_bottleneck_evidence = bottleneck.evidence;
        }
        for (const auto& block : snapshot.blocks) {
            result.total_memory_wait_cycles += block.memory_wait_cycles;
            result.memory_wait_available =
                result.memory_wait_available || block.memory_wait_available;
            result.total_output_blocked_cycles += block.output_blocked_cycles;
            result.total_completion_wait_cycles += block.completion_wait_cycles;
            if (!result.effective_ii_available && block.effective_ii_available) {
                result.effective_ii_available = true;
                result.effective_ii = block.effective_ii;
            }
        }
        for (const auto& link : snapshot.links) {
            result.max_link_occupancy =
                std::max(result.max_link_occupancy, link.occupancy_high_water);
        }

        const bool metrics_available =
            result.frame_cycles_available && result.frame_time_available &&
            result.achieved_pixels_per_cycle_available &&
            result.bandwidth_input_available && result.bandwidth_output_available;
        result.passed = complete && result.parity && metrics_available;
        if (!complete) {
            result.error_message = "output incomplete: expected " +
                std::to_string(expected) + " bytes, got " +
                std::to_string(actual.size());
        } else if (!result.parity) {
            result.error_message = "output differs from isp_pipeline::run()";
        } else if (!metrics_available) {
            result.error_message = "pipeline metric snapshot unavailable";
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
    config.add_fixed_param("ppc", "1");
    config.add_fixed_param("pixel_ii", "1");
    return config;
}

sweep_config bounded_frequency_config() {
    sweep_config config;
    config.name = "frequency_sweep";
    config.description = "Bounded line-pipeline frequency sweep";
    config.add_custom_sweep("resolution", {"12x8"}, "Bounded frame");
    config.params.back().type = sweep_param_type::RESOLUTION;
    config.add_frequency_sweep(100, 300, 100);
    config.add_fixed_param("ppc", "1");
    config.add_fixed_param("pixel_ii", "1");
    return config;
}

sweep_config bounded_block_config() {
    sweep_config config;
    config.name = "block_enable_sweep";
    config.description = "Bounded line-pipeline functional-enable sweep";
    config.add_custom_sweep("resolution", {"12x8"}, "Bounded frame");
    config.params.back().type = sweep_param_type::RESOLUTION;
    config.add_fixed_param("frequency", "200");
    config.add_fixed_param("ppc", "1");
    config.add_fixed_param("pixel_ii", "1");
    config.add_fixed_param("link_depth", "256");
    for (const auto* block : {"dpc", "bnr", "sharpen"}) {
        config.add_block_enable_sweep(block);
    }
    return config;
}

void run_sweep(const sweep_config& config, pipeline_sweep_runtime& runtime) {
    sweep_runner runner(config);
    runner.set_run_callback([&runtime](const auto& params) {
        return runtime.run(params);
    });
    const sweep_results results = runner.run();
    results.print_summary();
}
std::vector<candidate_params> comparison_candidates() {
    const candidate_params base = {
        {"variant", "baseline"},
        {"resolution", "8x6"},
        {"frequency", "200"},
        {"ppc", "1"},
        {"pixel_ii", "1"},
        {"link_depth", "256"},
        {"pipeline_latency", "16"},
        {"max_in_flight", "64"},
        {"memory_read_ports", "8"},
        {"memory_write_ports", "8"},
        {"memory_access_cycles", "1"},
        {"dpc", "on"},
        {"bnr", "on"},
        {"sharpen", "on"},
    };
    const auto variant = [&base](const char* name) {
        candidate_params result = base;
        result["variant"] = name;
        return result;
    };
    std::vector<candidate_params> candidates;
    candidates.push_back(base);
    auto ppc = variant("ppc2");
    ppc["ppc"] = "2";
    candidates.push_back(ppc);
    auto frequency = variant("frequency400");
    frequency["frequency"] = "400";
    candidates.push_back(frequency);
    auto pixel_ii = variant("pixel_ii2");
    pixel_ii["pixel_ii"] = "2";
    candidates.push_back(pixel_ii);
    auto latency = variant("latency32");
    latency["pipeline_latency"] = "32";
    candidates.push_back(latency);
    auto capacity = variant("max_in_flight1");
    capacity["max_in_flight"] = "1";
    candidates.push_back(capacity);
    auto memory = variant("memory_ports1");
    memory["memory_read_ports"] = "1";
    memory["memory_write_ports"] = "1";
    candidates.push_back(memory);
    auto shallow = variant("link_shallow");
    shallow["link_depth"] = "1";
    shallow["downstream_latency"] = "64";
    shallow["downstream_max_in_flight"] = "1";
    candidates.push_back(shallow);
    auto deep = variant("link_deep");
    deep["downstream_latency"] = "64";
    deep["downstream_max_in_flight"] = "1";
    candidates.push_back(deep);
    return candidates;
}

bool run_quick_comparison(
    pipeline_sweep_runtime& runtime,
    const std::vector<candidate_params>& candidates) {
    std::vector<sweep_result> results;
    results.reserve(candidates.size());
    for (const auto& candidate : candidates) {
        results.push_back(runtime.run(candidate));
    }
    sweep_results presentation;
    presentation.sweep_name = "quick_comparison";
    for (const auto& result : results) presentation.add_result(result);
    presentation.print_summary();

    std::cout << "\n=== Comparison Candidate Matrix ===\n";
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const auto& candidate = candidates[index];
        const auto& result = results[index];
        std::cout << "candidate " << index << " " << candidate.at("variant")
                  << ": frequency=" << candidate.at("frequency")
                  << " ppc=" << candidate.at("ppc")
                  << " pixel_ii=" << candidate.at("pixel_ii")
                  << " link_depth=" << candidate.at("link_depth")
                  << " pipeline_latency=" << candidate.at("pipeline_latency")
                  << " max_in_flight=" << candidate.at("max_in_flight")
                  << " memory_ports=" << candidate.at("memory_read_ports")
                  << "/" << candidate.at("memory_write_ports")
                  << " cycles=" << result.frame_cycles
                  << " first_latency=" << result.first_output_latency_cycles
                  << " achieved_ppc=" << result.achieved_pixels_per_cycle
                  << " completion_wait=" << result.total_completion_wait_cycles
                  << " memory_wait=" << result.total_memory_wait_cycles
                  << " output_blocked=" << result.total_output_blocked_cycles
                  << " occupancy=" << result.max_link_occupancy
                  << " effective_ii=" << result.effective_ii
                  << " parity=" << (result.parity ? "pass" : "fail")
                  << "\n";
    }

    const auto find = [&results](const char* name) -> const sweep_result* {
        for (const auto& result : results) {
            const auto it = result.params.find("variant");
            if (it != result.params.end() && it->second == name) return &result;
        }
        return nullptr;
    };
    const sweep_result* baseline = find("baseline");
    const sweep_result* ppc = find("ppc2");
    const sweep_result* frequency = find("frequency400");
    const sweep_result* pixel_ii = find("pixel_ii2");
    const sweep_result* latency = find("latency32");
    const sweep_result* capacity = find("max_in_flight1");
    const sweep_result* memory = find("memory_ports1");
    const sweep_result* shallow = find("link_shallow");
    const sweep_result* deep = find("link_deep");

    bool all_passed = results.size() == candidates.size() && !results.empty();
    for (const auto& result : results) {
        all_passed = all_passed && result.passed && result.parity;
    }
    const bool ppc_sensitive =
        baseline && ppc && baseline->frame_cycles_available &&
        ppc->frame_cycles_available &&
        baseline->achieved_pixels_per_cycle_available &&
        ppc->achieved_pixels_per_cycle_available &&
        ppc->frame_cycles < baseline->frame_cycles &&
        ppc->achieved_pixels_per_cycle > baseline->achieved_pixels_per_cycle;
    const bool frequency_sensitive =
        baseline && frequency && baseline->frame_cycles_available &&
        frequency->frame_cycles_available && baseline->frame_time_available &&
        frequency->frame_time_available &&
        baseline->frame_cycles == frequency->frame_cycles &&
        baseline->frame_time_us > frequency->frame_time_us;
    const bool pixel_ii_sensitive =
        baseline && pixel_ii && baseline->frame_cycles_available &&
        pixel_ii->frame_cycles_available &&
        baseline->achieved_pixels_per_cycle_available &&
        pixel_ii->achieved_pixels_per_cycle_available &&
        pixel_ii->frame_cycles > baseline->frame_cycles &&
        pixel_ii->achieved_pixels_per_cycle < baseline->achieved_pixels_per_cycle;
    const bool latency_sensitive =
        baseline && latency && baseline->first_output_latency_available &&
        latency->first_output_latency_available &&
        baseline->effective_ii_available && latency->effective_ii_available &&
        latency->first_output_latency_cycles >
            baseline->first_output_latency_cycles &&
        std::abs(latency->effective_ii - baseline->effective_ii) < 1e-9;
    const bool capacity_sensitive =
        baseline && capacity && capacity->parity &&
        baseline->frame_cycles_available && capacity->frame_cycles_available &&
        capacity->frame_cycles > baseline->frame_cycles &&
        (capacity->total_completion_wait_cycles >
             baseline->total_completion_wait_cycles ||
         capacity->total_output_blocked_cycles >
             baseline->total_output_blocked_cycles);
    const bool memory_sensitive =
        baseline && memory && baseline->memory_wait_available &&
        memory->memory_wait_available &&
        memory->total_memory_wait_cycles >
            baseline->total_memory_wait_cycles &&
        memory->frame_cycles > baseline->frame_cycles;
    const bool link_sensitive =
        shallow && deep && shallow->parity && deep->parity &&
        shallow->total_output_blocked_cycles >
            deep->total_output_blocked_cycles &&
        shallow->max_link_occupancy ==
            parameter_u32(shallow->params, "link_depth", 0) &&
        deep->max_link_occupancy <
            parameter_u32(deep->params, "link_depth", 0);

    std::cout << "\n=== Comparison Invariants ===\n"
              << "PPC sensitivity: " << (ppc_sensitive ? "PASS" : "FAIL") << "\n"
              << "frequency changes time, not cycles: "
              << (frequency_sensitive ? "PASS" : "FAIL") << "\n"
              << "pixel-II sensitivity: " << (pixel_ii_sensitive ? "PASS" : "FAIL")
              << "\n"
              << "latency changes first output, not effective II: "
              << (latency_sensitive ? "PASS" : "FAIL") << "\n"
              << "max-in-flight capacity adds frame/pipeline pressure: "
              << (capacity_sensitive ? "PASS" : "FAIL") << "\n"
              << "modeled memory ports increase waits and frame cycles: "
              << (memory_sensitive ? "PASS" : "FAIL") << "\n"
              << "shallow link increases output blocking/occupancy: "
              << (link_sensitive ? "PASS" : "FAIL") << "\n";
    if (!ppc_sensitive || !frequency_sensitive || !pixel_ii_sensitive ||
        !latency_sensitive || !capacity_sensitive || !memory_sensitive ||
        !link_sensitive) {
        std::cerr << "comparison failed: required sensitivity invariant missing\n";
        all_passed = false;
    }
    return all_passed;
}

}  // namespace

int sc_main(int argc, char* argv[]) {
    bool run_all = false;
    bool run_res = false;
    bool run_freq = false;
    bool run_block = false;
    bool run_compare = false;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--all" || argument == "-a") run_all = true;
        else if (argument == "--resolution" || argument == "-r") run_res = true;
        else if (argument == "--frequency" || argument == "-f") run_freq = true;
        else if (argument == "--blocks" || argument == "-b") run_block = true;
        else if (argument == "--compare" || argument == "-c") run_compare = true;
        else if (argument == "--help" || argument == "-h") {
            std::cout << "Usage: tb_arch_sweep [options]\n"
                      << "  --all, -a          Run bounded sweeps\n"
                      << "  --resolution, -r  Run resolution sweep\n"
                      << "  --frequency, -f   Run frequency sweep\n"
                      << "  --blocks, -b      Run functional-enable sweep\n"
                      << "  --compare, -c     Compare real architecture candidates\n";
            return 0;
        } else {
            std::cerr << "unknown or incomplete option: " << argument << '\n';
            return 2;
        }
    }

    if (run_compare) {
        const std::vector<candidate_params> candidates = comparison_candidates();
        pipeline_sweep_runtime runtime(candidates);
        const bool passed = run_quick_comparison(runtime, candidates);
        sc_stop();
        return passed ? 0 : 1;
    }

    sweep_config resolution_config = bounded_resolution_config();
    sweep_config frequency_config = bounded_frequency_config();
    sweep_config block_config = bounded_block_config();
    std::unique_ptr<pipeline_sweep_runtime> resolution_runtime;
    std::unique_ptr<pipeline_sweep_runtime> frequency_runtime;
    std::unique_ptr<pipeline_sweep_runtime> block_runtime;
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
    sc_stop();
    return 0;
}
