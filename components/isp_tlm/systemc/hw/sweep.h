/**
 * @file sweep.h
 * @brief Parameter enumeration and presentation for architecture sweeps.
 */

#ifndef ISP_SWEEP_H
#define ISP_SWEEP_H

#include <cstdint>
#include <cstdio>
#include <atomic>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// ============================================================================
// Sweep parameter types
// ============================================================================
enum class sweep_param_type {
    RESOLUTION,
    FREQUENCY,
    PPC,
    PIXEL_II,
    LINK_DEPTH,
    BLOCK_ENABLE,
    CUSTOM
};

struct sweep_param {
    std::string name;
    sweep_param_type type = sweep_param_type::CUSTOM;
    std::string description;
    std::vector<std::string> discrete_values;
    double range_min = 0.0;
    double range_max = 0.0;
    double range_step = 1.0;
    std::string range_unit;
    std::string current_value;
};

struct resolution_spec {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    std::string to_string() const {
        return std::to_string(width) + "x" + std::to_string(height);
    }

    static resolution_spec from_string(const std::string& value) {
        resolution_spec result;
        std::sscanf(value.c_str(), "%ux%u", &result.width, &result.height);
        return result;
    }

    std::uint64_t pixels() const {
        return static_cast<std::uint64_t>(width) * height;
    }
};

// ============================================================================
// Sweep configuration
// ============================================================================
struct sweep_config {
    std::string name = "default_sweep";
    std::string description;
    std::vector<sweep_param> params;
    std::map<std::string, std::string> fixed_params;
    std::uint32_t max_points = 0;

    void add_resolution_sweep(const std::vector<resolution_spec>& resolutions) {
        sweep_param parameter;
        parameter.name = "resolution";
        parameter.type = sweep_param_type::RESOLUTION;
        parameter.description = "Image resolution";
        for (const auto& resolution : resolutions) {
            parameter.discrete_values.push_back(resolution.to_string());
        }
        params.push_back(parameter);
    }

    void add_frequency_sweep(double min_mhz, double max_mhz, double step_mhz) {
        sweep_param parameter;
        parameter.name = "frequency";
        parameter.type = sweep_param_type::FREQUENCY;
        parameter.description = "Architecture clock frequency";
        parameter.range_min = min_mhz;
        parameter.range_max = max_mhz;
        parameter.range_step = step_mhz;
        parameter.range_unit = "MHz";
        params.push_back(parameter);
    }

    void add_ppc_sweep(const std::vector<std::uint32_t>& values) {
        sweep_param parameter;
        parameter.name = "ppc";
        parameter.type = sweep_param_type::PPC;
        parameter.description = "Pixels processed per cycle";
        for (const auto value : values) {
            parameter.discrete_values.push_back(std::to_string(value));
        }
        params.push_back(parameter);
    }

    void add_pixel_ii_sweep(const std::vector<std::uint32_t>& values) {
        sweep_param parameter;
        parameter.name = "pixel_ii";
        parameter.type = sweep_param_type::PIXEL_II;
        parameter.description = "Pixel initiation interval in cycles";
        for (const auto value : values) {
            parameter.discrete_values.push_back(std::to_string(value));
        }
        params.push_back(parameter);
    }

    void add_link_depth_sweep(const std::vector<std::uint32_t>& values) {
        sweep_param parameter;
        parameter.name = "link_depth";
        parameter.type = sweep_param_type::LINK_DEPTH;
        parameter.description = "Internal line-link depth";
        for (const auto value : values) {
            parameter.discrete_values.push_back(std::to_string(value));
        }
        params.push_back(parameter);
    }

    void add_block_enable_sweep(
        const std::string& block_name,
        const std::vector<std::string>& values = {"on", "off"}) {
        sweep_param parameter;
        parameter.name = block_name;
        parameter.type = sweep_param_type::BLOCK_ENABLE;
        parameter.description = "Functional enable for " + block_name;
        parameter.discrete_values = values;
        params.push_back(parameter);
    }

    void add_custom_sweep(const std::string& name,
                          const std::vector<std::string>& values,
                          const std::string& description = "") {
        sweep_param parameter;
        parameter.name = name;
        parameter.type = sweep_param_type::CUSTOM;
        parameter.description = description;
        parameter.discrete_values = values;
        params.push_back(parameter);
    }

    void add_fixed_param(const std::string& name, const std::string& value) {
        fixed_params[name] = value;
    }

    std::uint64_t total_points() const {
        std::uint64_t total = 1;
        for (const auto& parameter : params) {
            std::uint64_t count = parameter.discrete_values.size();
            if (count == 0 && parameter.range_step > 0.0 &&
                parameter.range_max >= parameter.range_min) {
                count = static_cast<std::uint64_t>(
                    (parameter.range_max - parameter.range_min) /
                    parameter.range_step) + 1;
            }
            total *= count;
        }
        return total;
    }
};

// ============================================================================
// Unified pipeline metric presentation
// ============================================================================
struct sweep_result {
    std::map<std::string, std::string> params;

    std::uint64_t frame_cycles = 0;
    bool frame_cycles_available = false;
    double frame_time_us = 0.0;
    bool frame_time_available = false;
    double achieved_pixels_per_cycle = 0.0;
    bool achieved_pixels_per_cycle_available = false;
    double bandwidth_input_mbps = 0.0;
    bool bandwidth_input_available = false;
    double bandwidth_output_mbps = 0.0;
    bool bandwidth_output_available = false;
    std::uint64_t first_output_latency_cycles = 0;
    bool first_output_latency_available = false;
    std::uint64_t total_memory_wait_cycles = 0;
    bool memory_wait_available = false;
    std::uint64_t total_output_blocked_cycles = 0;
    std::uint64_t total_completion_wait_cycles = 0;
    std::uint32_t max_link_occupancy = 0;
    bool effective_ii_available = false;
    double effective_ii = 0.0;
    double avg_block_utilization = 0.0;
    std::string top_bottleneck_type;
    std::string top_bottleneck_location;
    double top_bottleneck_severity = 0.0;
    std::string top_bottleneck_evidence;
    double simulation_time_seconds = 0.0;

    bool parity = false;
    bool passed = false;
    std::string error_message;
};

struct sweep_results {
    std::vector<sweep_result> results;
    std::string sweep_name;
    sweep_config config;

    void add_result(const sweep_result& result) { results.push_back(result); }


    void print_summary() const {
        std::cout << "\n=== Sweep Summary: " << sweep_name << " ===\n"
                  << "Total configurations tested: " << results.size() << "\n";
        for (std::size_t index = 0; index < results.size(); ++index) {
            const auto& result = results[index];
            std::cout << "config " << index << ": cycles=" << result.frame_cycles
                      << " (" << (result.frame_cycles_available ? "available" : "unavailable")
                      << "), time_us=" << result.frame_time_us
                      << ", first_latency_cycles="
                      << result.first_output_latency_cycles
                      << ", achieved_ppc=" << result.achieved_pixels_per_cycle
                      << ", memory_wait_cycles="
                      << result.total_memory_wait_cycles
                      << ", output_blocked_cycles="
                      << result.total_output_blocked_cycles
                      << ", completion_wait_cycles="
                      << result.total_completion_wait_cycles
                      << ", max_link_occupancy="
                      << result.max_link_occupancy
                      << ", effective_ii=" << result.effective_ii
                      << ", parity=" << (result.parity ? "pass" : "fail")
                      << ", status=" << (result.passed ? "pass" : "fail");
            if (!result.top_bottleneck_location.empty()) {
                std::cout << ", bottleneck=" << result.top_bottleneck_type
                          << ':' << result.top_bottleneck_location
                          << " severity=" << result.top_bottleneck_severity;
                if (!result.top_bottleneck_evidence.empty()) {
                    std::cout << " [" << result.top_bottleneck_evidence << ']';
                }
            }
            if (!result.error_message.empty()) {
                std::cout << ", error=" << result.error_message;
            }
            std::cout << '\n';
        }
    }
};

// ============================================================================
// Sweep runner
// ============================================================================
class sweep_runner {
public:
    using run_function = std::function<sweep_result(
        const std::map<std::string, std::string>& params)>;

    explicit sweep_runner(const sweep_config& config)
        : m_config(config), m_abort(false) {}

    void set_run_callback(run_function function) { m_run_function = std::move(function); }

    sweep_results run() {
        sweep_results results;
        results.sweep_name = m_config.name;
        results.config = m_config;
        std::cout << "\n=== Starting Sweep: " << m_config.name << " ===\n"
                  << "Total points: " << m_config.total_points() << "\n";
        std::uint64_t point = 0;
        std::map<std::string, std::string> current;
        enumerate(results, point, current, 0);
        std::cout << "=== Sweep Complete (" << results.results.size()
                  << " configurations) ===\n";
        return results;
    }

    void abort() { m_abort = true; }

private:
    sweep_config m_config;
    run_function m_run_function;
    std::atomic<bool> m_abort;

    void enumerate(sweep_results& results, std::uint64_t& point,
                   std::map<std::string, std::string>& current,
                   std::size_t parameter_index) {
        if (m_abort) return;
        if (parameter_index == m_config.params.size()) {
            ++point;
            std::map<std::string, std::string> candidate = m_config.fixed_params;
            candidate.insert(current.begin(), current.end());
            sweep_result result;
            result.params = candidate;
            if (m_run_function) {
                result = m_run_function(candidate);
                result.params = candidate;
            } else {
                result.error_message = "no sweep callback";
            }
            results.add_result(result);
            if (m_config.max_points != 0 && point >= m_config.max_points) {
                m_abort = true;
            }
            return;
        }

        const auto& parameter = m_config.params[parameter_index];
        if (!parameter.discrete_values.empty()) {
            for (const auto& value : parameter.discrete_values) {
                current[parameter.name] = value;
                enumerate(results, point, current, parameter_index + 1);
                if (m_abort) return;
            }
        } else if (parameter.range_step > 0.0) {
            for (double value = parameter.range_min;
                 value <= parameter.range_max + 0.001;
                 value += parameter.range_step) {
                std::ostringstream stream;
                stream << std::fixed << std::setprecision(3) << value;
                current[parameter.name] = stream.str();
                enumerate(results, point, current, parameter_index + 1);
                if (m_abort) return;
            }
        }
    }
};

#endif  // ISP_SWEEP_H
