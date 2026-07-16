/**
 * @file sweep.h
 * @brief Architecture sweep runner for parameter exploration
 *
 * Provides infrastructure for running architecture sweeps:
 *   - Sweep configuration (resolution, frequency, block enables)
 *   - Parameter sweep execution
 *   - Results aggregation and comparison
 *   - CSV export for analysis
 */

#ifndef ISP_SWEEP_H
#define ISP_SWEEP_H

#include <systemc>
using namespace sc_core;

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <sstream>

// ============================================================================
// Sweep Parameter Types
// ============================================================================
enum class sweep_param_type {
    RESOLUTION,
    FREQUENCY,
    BLOCK_ENABLE,
    BLOCK_BYPASS,
    BANDWIDTH_LIMIT,
    CUSTOM
};

struct sweep_param {
    std::string name;
    sweep_param_type type;
    std::string description;

    // For discrete sweeps
    std::vector<std::string> discrete_values;

    // For range sweeps
    double range_min = 0;
    double range_max = 0;
    double range_step = 1;
    std::string range_unit = "";

    // Current value
    std::string current_value;
};

struct resolution_spec {
    std::uint32_t width;
    std::uint32_t height;

    std::string to_string() const {
        return std::to_string(width) + "x" + std::to_string(height);
    }

    static resolution_spec from_string(const std::string& s) {
        resolution_spec r = {0, 0};
        std::sscanf(s.c_str(), "%ux%u", &r.width, &r.height);
        return r;
    }

    std::uint64_t pixels() const { return static_cast<std::uint64_t>(width) * height; }
};

// ============================================================================
// Sweep Configuration
// ============================================================================
struct sweep_config {
    std::string name = "default_sweep";
    std::string description = "";

    // Sweep parameters
    std::vector<sweep_param> params;

    // Fixed parameters
    std::map<std::string, std::string> fixed_params;

    // Output directory
    std::string output_dir = "output/sweeps";

    // Early termination
    std::uint32_t max_points = 0;  // 0 = unlimited
    double timeout_seconds = 0;     // 0 = no timeout

    // Add parameter helpers
    void add_resolution_sweep(const std::vector<resolution_spec>& resolutions) {
        sweep_param p;
        p.name = "resolution";
        p.type = sweep_param_type::RESOLUTION;
        p.description = "Image resolution";
        for (const auto& r : resolutions) {
            p.discrete_values.push_back(r.to_string());
        }
        params.push_back(p);
    }

    void add_frequency_sweep(double min_mhz, double max_mhz, double step_mhz) {
        sweep_param p;
        p.name = "frequency";
        p.type = sweep_param_type::FREQUENCY;
        p.description = "Clock frequency";
        p.range_min = min_mhz;
        p.range_max = max_mhz;
        p.range_step = step_mhz;
        p.range_unit = "MHz";
        params.push_back(p);
    }

    void add_block_enable_sweep(const std::string& block_name,
                               const std::vector<std::string>& values = {"on", "off"}) {
        sweep_param p;
        p.name = block_name + "_enable";
        p.type = sweep_param_type::BLOCK_ENABLE;
        p.description = "Enable/disable " + block_name;
        p.discrete_values = values;
        params.push_back(p);
    }

    void add_custom_sweep(const std::string& name,
                         const std::vector<std::string>& values,
                         const std::string& description = "") {
        sweep_param p;
        p.name = name;
        p.type = sweep_param_type::CUSTOM;
        p.description = description;
        p.discrete_values = values;
        params.push_back(p);
    }

    void add_fixed_param(const std::string& name, const std::string& value) {
        fixed_params[name] = value;
    }

    std::uint64_t total_points() const {
        std::uint64_t total = 1;
        for (const auto& p : params) {
            std::uint64_t count = 0;
            if (!p.discrete_values.empty()) {
                count = p.discrete_values.size();
            } else if (p.range_step > 0 && p.range_max >= p.range_min) {
                count = static_cast<std::uint64_t>((p.range_max - p.range_min) / p.range_step) + 1;
            }
            total *= count;
        }
        return total;
    }
};

// ============================================================================
// Sweep Results
// ============================================================================
struct sweep_result {
    // Configuration
    std::map<std::string, std::string> params;

    // Architecture metrics
    double frame_time_us = 0;
    double fps = 0;
    double frame_energy_nj = 0;
    double avg_power_mw = 0;
    double peak_power_mw = 0;
    double bottleneck_severity = 0;
    std::string bottleneck_location;

    // Throughput
    double throughput_mpixel_s = 0;
    double bandwidth_input_mbps = 0;
    double bandwidth_output_mbps = 0;

    // Utilization
    double avg_block_utilization = 0;

    // Metadata
    std::uint64_t sim_cycles = 0;
    double sim_time_seconds = 0;
    bool passed = true;
    std::string error_message;
};

struct sweep_results {
    std::vector<sweep_result> results;
    std::string sweep_name;
    sweep_config config;

    void add_result(const sweep_result& r) {
        results.push_back(r);
    }

    // Export to CSV
    void export_csv(const std::string& filename) const {
        std::ofstream ofs(filename);
        if (!ofs) return;

        // Build header from first result
        if (results.empty()) return;

        std::vector<std::string> headers = {"config"};
        for (const auto& p : results[0].params) {
            headers.push_back(p.first);
        }
        headers.insert(headers.end(), {
            "frame_time_us", "fps", "frame_energy_nj", "avg_power_mw", "peak_power_mw",
            "throughput_Mpx/s", "bw_input_Mbps", "bw_output_Mbps", "avg_util",
            "bottleneck_severity", "bottleneck", "sim_cycles", "passed"
        });

        // Write header
        for (std::size_t i = 0; i < headers.size(); ++i) {
            if (i > 0) ofs << ",";
            ofs << headers[i];
        }
        ofs << "\n";

        // Write results
        for (std::size_t r = 0; r < results.size(); ++r) {
            const auto& res = results[r];
            ofs << "config_" << r;

            for (const auto& p : res.params) {
                ofs << "," << p.second;
            }

            ofs << std::fixed << std::setprecision(3)
                << "," << res.frame_time_us
                << "," << res.fps
                << "," << res.frame_energy_nj
                << "," << res.avg_power_mw
                << "," << res.peak_power_mw
                << "," << res.throughput_mpixel_s
                << "," << res.bandwidth_input_mbps
                << "," << res.bandwidth_output_mbps
                << "," << res.avg_block_utilization
                << "," << res.bottleneck_severity
                << "," << res.bottleneck_location
                << "," << res.sim_cycles
                << "," << (res.passed ? "1" : "0")
                << "\n";
        }

        ofs.close();
    }

    // Summary report
    void print_summary() const {
        std::cout << "\n=== Sweep Summary: " << sweep_name << " ===\n";
        std::cout << "Total configurations tested: " << results.size() << "\n\n";

        if (results.empty()) {
            std::cout << "No results to summarize.\n";
            return;
        }

        // Find best/worst
        double best_fps = 0, worst_fps = 1e9;
        double best_efficiency = 0;
        std::size_t best_idx = 0, worst_idx = 0;

        for (std::size_t i = 0; i < results.size(); ++i) {
            const auto& r = results[i];
            if (r.fps > best_fps) {
                best_fps = r.fps;
                best_idx = i;
            }
            if (r.fps < worst_fps) {
                worst_fps = r.fps;
                worst_idx = i;
            }
            double efficiency = r.fps / (r.avg_power_mw + 0.001);
            if (efficiency > best_efficiency) {
                best_efficiency = efficiency;
            }
        }

        std::cout << "--- Performance Range ---\n";
        std::cout << "Best FPS: " << std::fixed << std::setprecision(1)
                  << best_fps << " (config " << best_idx << ")\n";
        std::cout << "Worst FPS: " << worst_fps << " (config " << worst_idx << ")\n";
        std::cout << "FPS Range: " << (best_fps - worst_fps) << "\n\n";

        std::cout << "--- Power Range ---\n";
        double min_power = 1e9, max_power = 0;
        for (const auto& r : results) {
            min_power = std::min(min_power, r.avg_power_mw);
            max_power = std::max(max_power, r.avg_power_mw);
        }
        std::cout << "Min power: " << std::fixed << std::setprecision(2) << min_power << " mW\n";
        std::cout << "Max power: " << max_power << " mW\n\n";

        std::cout << "--- Energy Efficiency ---\n";
        std::cout << "Best FPS/mW: " << std::fixed << std::setprecision(3)
                  << best_efficiency << "\n\n";

        // Bottleneck analysis
        std::map<std::string, std::uint32_t> bottleneck_counts;
        for (const auto& r : results) {
            bottleneck_counts[r.bottleneck_location]++;
        }
        std::cout << "--- Bottleneck Distribution ---\n";
        for (const auto& p : bottleneck_counts) {
            std::cout << p.first << ": " << p.second << " configs ("
                      << std::fixed << std::setprecision(1)
                      << (p.second * 100.0 / results.size()) << "%)\n";
        }
    }
};

// ============================================================================
// Pre-defined Sweep Configurations
// ============================================================================
namespace sweep_presets {

inline sweep_config resolution_sweep() {
    sweep_config cfg;
    cfg.name = "resolution_sweep";
    cfg.description = "Sweep across different image resolutions";

    cfg.add_resolution_sweep({
        {640, 480},    // VGA
        {1280, 720},   // 720p
        {1920, 1080},  // 1080p
        {2560, 1440},  // 1440p
        {3840, 2160},  // 4K
    });

    cfg.add_fixed_param("frequency", "200");
    cfg.output_dir = "output/sweeps/resolution";

    return cfg;
}

inline sweep_config frequency_sweep() {
    sweep_config cfg;
    cfg.name = "frequency_sweep";
    cfg.description = "Sweep across different clock frequencies";

    cfg.add_custom_sweep("resolution", {"1920x1080"}, "Fixed 1080p");
    cfg.params.back().type = sweep_param_type::RESOLUTION;

    cfg.add_frequency_sweep(50, 400, 50);  // 50-400 MHz in 50 MHz steps

    cfg.output_dir = "output/sweeps/frequency";

    return cfg;
}

inline sweep_config block_enable_sweep() {
    sweep_config cfg;
    cfg.name = "block_enable_sweep";
    cfg.description = "Sweep block enables to find critical path";

    cfg.add_custom_sweep("resolution", {"1920x1080"}, "Fixed 1080p");
    cfg.params.back().type = sweep_param_type::RESOLUTION;

    cfg.add_fixed_param("frequency", "200");

    // Each block enable sweep
    std::vector<std::string> blocks = {
        "dpc", "bnr", "demosaic", "sharpen", "2dnr", "lsc", "ccm", "gc", "csc"
    };

    for (const auto& block : blocks) {
        cfg.add_block_enable_sweep(block, {"on", "off"});
    }

    cfg.output_dir = "output/sweeps/block_enable";

    return cfg;
}

inline sweep_config bandwidth_limit_sweep() {
    sweep_config cfg;
    cfg.name = "bandwidth_limit_sweep";
    cfg.description = "Sweep input bandwidth to find bottlenecks";

    cfg.add_custom_sweep("resolution", {"1920x1080"}, "Fixed 1080p");
    cfg.params.back().type = sweep_param_type::RESOLUTION;

    cfg.add_fixed_param("frequency", "200");

    // Bandwidth limits
    sweep_param p;
    p.name = "input_bandwidth";
    p.type = sweep_param_type::BANDWIDTH_LIMIT;
    p.description = "Input bandwidth limit (Mbps)";
    p.range_min = 100;
    p.range_max = 1600;
    p.range_step = 100;
    p.range_unit = "Mbps";
    cfg.params.push_back(p);

    cfg.output_dir = "output/sweeps/bandwidth";

    return cfg;
}

inline sweep_config full_sweep() {
    sweep_config cfg;
    cfg.name = "full_sweep";
    cfg.description = "Comprehensive sweep: resolution x frequency x key blocks";

    cfg.add_resolution_sweep({
        {1920, 1080},  // 1080p
        {3840, 2160},  // 4K
    });

    cfg.add_frequency_sweep(100, 300, 100);  // 100, 200, 300 MHz

    cfg.add_custom_sweep("dpc", {"on", "off"}, "DPC enable");
    cfg.params.back().type = sweep_param_type::BLOCK_ENABLE;

    cfg.add_custom_sweep("sharpen", {"on", "off"}, "Sharpen enable");
    cfg.params.back().type = sweep_param_type::BLOCK_ENABLE;

    cfg.add_fixed_param("bnr", "on");
    cfg.add_fixed_param("demosaic", "on");

    cfg.output_dir = "output/sweeps/full";
    cfg.max_points = 100;  // Limit to prevent excessive runs

    return cfg;
}

}  // namespace sweep_presets

// ============================================================================
// Sweep Runner
// ============================================================================
class sweep_runner {
public:
    using run_function = std::function<sweep_result(const std::map<std::string, std::string>& params)>;

    explicit sweep_runner(const sweep_config& config)
        : m_config(config), m_abort(false) {}

    // Set callback for each run
    void set_run_callback(run_function func) {
        m_run_func = func;
    }

    // Run all sweep points
    sweep_results run() {
        sweep_results results;
        results.sweep_name = m_config.name;
        results.config = m_config;

        std::cout << "\n=== Starting Sweep: " << m_config.name << " ===\n";
        std::cout << "Total points: " << m_config.total_points() << "\n\n";

        std::uint64_t point = 0;
        std::map<std::string, std::string> current_params;
        run_sweep_recursive(results, point, current_params, 0);

        std::cout << "\n=== Sweep Complete ===\n";
        std::cout << "Configurations tested: " << results.results.size() << "\n";

        return results;
    }

    void abort() { m_abort = true; }

private:
    sweep_config m_config;
    run_function m_run_func;
    std::atomic<bool> m_abort;

    void run_sweep_recursive(sweep_results& results, std::uint64_t& point,
                             std::map<std::string, std::string>& current_params,
                             std::size_t param_idx = 0) {
        if (m_abort) return;

        if (param_idx >= m_config.params.size()) {
            // Leaf node - run the simulation
            ++point;
            std::cout << "[" << point << "/" << m_config.total_points() << "] ";

            // Print params
            for (const auto& p : current_params) {
                std::cout << p.first << "=" << p.second << " ";
            }
            std::cout << "... ";

            if (m_run_func) {
                sweep_result r = m_run_func(current_params);
                r.params = current_params;
                results.add_result(r);

                if (r.passed) {
                    std::cout << "OK (FPS=" << std::fixed << std::setprecision(1)
                              << r.fps << ", Power=" << r.avg_power_mw << "mW)\n";
                } else {
                    std::cout << "FAILED: " << r.error_message << "\n";
                }
            } else {
                std::cout << "No run function set!\n";
            }

            // Check max points
            if (m_config.max_points > 0 && point >= m_config.max_points) {
                std::cout << "\nMax points reached (" << m_config.max_points << ")\n";
                m_abort = true;
            }

            return;
        }

        // Recursively iterate through parameter values
        const auto& param = m_config.params[param_idx];

        std::vector<std::string> values;
        if (!param.discrete_values.empty()) {
            values = param.discrete_values;
        } else if (param.range_step > 0) {
            for (double v = param.range_min; v <= param.range_max + 0.001; v += param.range_step) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(0) << v;
                values.push_back(oss.str());
            }
        }

        for (const auto& val : values) {
            current_params[param.name] = val;
            run_sweep_recursive(results, point, current_params, param_idx + 1);
            if (m_abort) return;
        }
    }
};

#endif  // ISP_SWEEP_H
