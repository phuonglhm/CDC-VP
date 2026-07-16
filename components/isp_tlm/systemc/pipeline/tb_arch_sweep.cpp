/**
 * @file tb_arch_sweep.cpp
 * @brief Testbench demonstrating architecture sweep execution
 *
 * Shows how to:
 *   - Configure sweep parameters
 *   - Run sweep with simulation function
 *   - Collect and export results
 *   - Compare configurations
 */

#include <systemc>
using namespace sc_core;

#include <iostream>
#include <iomanip>
#include <random>

#include "../hw/sweep.h"
#include "../hw/power.h"

// ============================================================================
// Mock Simulation Function (replace with actual pipeline simulation)
// ============================================================================
sweep_result run_mock_simulation(const std::map<std::string, std::string>& params,
                                const sweep_config& config) {
    sweep_result result;

    // Parse parameters
    resolution_spec res = {1920, 1080};
    double freq_mhz = 200.0;

    for (const auto& p : params) {
        if (p.first == "resolution") {
            res = resolution_spec::from_string(p.second);
        } else if (p.first == "frequency") {
            freq_mhz = std::atof(p.second.c_str());
        }
    }

    // Simulate processing
    std::uint64_t pixels = res.pixels();
    double cycles_per_pixel = 1.0;  // Simplified

    // Base time calculation
    double ideal_frame_us = pixels / freq_mhz;

    // Add some noise based on block enables
    double overhead = 1.0;
    for (const auto& p : params) {
        if (p.second == "off") {
            overhead -= 0.05;  // Skip disabled blocks
        }
    }
    overhead = std::max(0.8, overhead);

    result.frame_time_us = ideal_frame_us * overhead;
    result.fps = 1e6 / result.frame_time_us;

    // Bandwidth
    result.bandwidth_input_mbps = (pixels * 2.0 * 8.0) / result.frame_time_us / 1e3;
    result.bandwidth_output_mbps = (pixels * 1.5 * 8.0) / result.frame_time_us / 1e3;
    result.throughput_mpixel_s = pixels / result.frame_time_us;

    // Power (simplified)
    double base_power = 50.0;  // mW base
    double freq_factor = freq_mhz / 200.0;
    double res_factor = static_cast<double>(pixels) / (1920.0 * 1080.0);

    result.avg_power_mw = base_power * freq_factor * res_factor;
    result.peak_power_mw = result.avg_power_mw * 1.2;
    result.frame_energy_nj = result.avg_power_mw * result.frame_time_us / 1000.0;

    // Utilization
    result.avg_block_utilization = 0.7 + 0.2 * (1.0 - overhead);

    // Bottleneck
    if (freq_mhz < 150.0) {
        result.bottleneck_location = "frequency";
        result.bottleneck_severity = 1.0 - freq_mhz / 200.0;
    } else if (res.pixels() > 1920 * 1080) {
        result.bottleneck_location = "bandwidth";
        result.bottleneck_severity = 0.3;
    } else {
        result.bottleneck_location = "compute";
        result.bottleneck_severity = 0.1;
    }

    result.sim_cycles = static_cast<std::uint64_t>(pixels * cycles_per_pixel);
    result.passed = true;

    return result;
}

// ============================================================================
// Resolution Sweep
// ============================================================================
void run_resolution_sweep() {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "   RESOLUTION SWEEP\n";
    std::cout << "============================================================\n";

    sweep_config cfg = sweep_presets::resolution_sweep();

    sweep_runner runner(cfg);
    runner.set_run_callback([&](const std::map<std::string, std::string>& params) {
        return run_mock_simulation(params, cfg);
    });

    sweep_results results = runner.run();
    results.print_summary();

    // Export
    std::string filename = cfg.output_dir + "/resolution_sweep.csv";
    results.export_csv(filename);
    std::cout << "\nExported to: " << filename << "\n";
}

// ============================================================================
// Frequency Sweep
// ============================================================================
void run_frequency_sweep() {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "   FREQUENCY SWEEP\n";
    std::cout << "============================================================\n";

    sweep_config cfg = sweep_presets::frequency_sweep();

    sweep_runner runner(cfg);
    runner.set_run_callback([&](const std::map<std::string, std::string>& params) {
        return run_mock_simulation(params, cfg);
    });

    sweep_results results = runner.run();
    results.print_summary();

    // Export
    std::string filename = cfg.output_dir + "/frequency_sweep.csv";
    results.export_csv(filename);
    std::cout << "\nExported to: " << filename << "\n";
}

// ============================================================================
// Block Enable Sweep
// ============================================================================
void run_block_enable_sweep() {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "   BLOCK ENABLE SWEEP (Critical Path Analysis)\n";
    std::cout << "============================================================\n";

    // Simplified sweep - just 3 blocks for demo
    sweep_config cfg;
    cfg.name = "block_critical_path";
    cfg.description = "Find critical path blocks";
    cfg.output_dir = "output/sweeps/block_critical";

    cfg.add_custom_sweep("resolution", {"1920x1080"}, "1080p");
    cfg.params.back().type = sweep_param_type::RESOLUTION;
    cfg.add_fixed_param("frequency", "200");

    std::vector<std::string> blocks = {"dpc", "sharpen", "bnr"};
    for (const auto& b : blocks) {
        cfg.add_block_enable_sweep(b, {"on", "off"});
    }

    sweep_runner runner(cfg);
    runner.set_run_callback([&](const std::map<std::string, std::string>& params) {
        return run_mock_simulation(params, cfg);
    });

    sweep_results results = runner.run();
    results.print_summary();

    // Export
    std::string filename = cfg.output_dir + "/block_critical_path.csv";
    results.export_csv(filename);
    std::cout << "\nExported to: " << filename << "\n";
}

// ============================================================================
// Quick Comparison
// ============================================================================
void run_quick_comparison() {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "   QUICK CONFIGURATION COMPARISON\n";
    std::cout << "============================================================\n";

    struct config_t {
        std::string name;
        std::string resolution;
        std::string frequency;
        bool dpc_on, bnr_on, sharpen_on;
    };

    std::vector<config_t> configs = {
        {"Full 1080p@200", "1920x1080", "200", true, true, true},
        {"Full 4K@200", "3840x2160", "200", true, true, true},
        {"No BNR 1080p", "1920x1080", "200", true, false, true},
        {"No Sharpen 1080p", "1920x1080", "200", true, true, false},
        {"Fast 1080p@400", "1920x1080", "400", true, true, true},
    };

    sweep_results results;
    results.sweep_name = "quick_comparison";

    std::cout << std::setw(20) << "Config"
              << std::setw(12) << "Resolution"
              << std::setw(10) << "Freq"
              << std::setw(10) << "FPS"
              << std::setw(12) << "Power"
              << std::setw(12) << "Energy"
              << "\n";
    std::cout << std::string(76, '-') << "\n";

    for (const auto& c : configs) {
        std::map<std::string, std::string> params;
        params["resolution"] = c.resolution;
        params["frequency"] = c.frequency;
        params["dpc"] = c.dpc_on ? "on" : "off";
        params["bnr"] = c.bnr_on ? "on" : "off";
        params["sharpen"] = c.sharpen_on ? "on" : "off";

        sweep_result r = run_mock_simulation(params, sweep_config{});
        r.params = params;
        results.add_result(r);

        std::cout << std::setw(20) << c.name
                  << std::setw(12) << c.resolution
                  << std::setw(9) << c.frequency << "MHz"
                  << std::setw(10) << std::fixed << std::setprecision(1) << r.fps
                  << std::setw(11) << std::fixed << std::setprecision(1) << r.avg_power_mw << "mW"
                  << std::setw(11) << std::fixed << std::setprecision(1) << r.frame_energy_nj << "nJ"
                  << "\n";
    }

    std::cout << "\n";
    results.print_summary();
}

// ============================================================================
// Main
// ============================================================================
int sc_main(int argc, char* argv[]) {
    std::cout << "============================================================\n";
    std::cout << "   ISP Architecture Sweep Runner\n";
    std::cout << "============================================================\n";
    std::cout << "\nNote: This is a mock simulation for demonstration.\n";
    std::cout << "Replace run_mock_simulation() with actual pipeline simulation.\n\n";

    // Parse command line
    bool run_all = true;
    bool run_res = false;
    bool run_freq = false;
    bool run_block = false;
    bool run_compare = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--all" || arg == "-a") run_all = true;
        else if (arg == "--resolution" || arg == "-r") run_res = true;
        else if (arg == "--frequency" || arg == "-f") run_freq = true;
        else if (arg == "--blocks" || arg == "-b") run_block = true;
        else if (arg == "--compare" || arg == "-c") run_compare = true;
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: tb_arch_sweep [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --all, -a       Run all sweeps (default)\n";
            std::cout << "  --resolution, -r  Run resolution sweep\n";
            std::cout << "  --frequency, -f Run frequency sweep\n";
            std::cout << "  --blocks, -b    Run block enable sweep\n";
            std::cout << "  --compare, -c   Run quick comparison\n";
            std::cout << "  --help, -h      Show this help\n";
            return 0;
        }
    }

    if (run_compare) {
        run_quick_comparison();
        return 0;
    }

    if (run_all || run_res) {
        run_resolution_sweep();
    }

    if (run_all || run_freq) {
        run_frequency_sweep();
    }

    if (run_all || run_block) {
        run_block_enable_sweep();
    }

    std::cout << "\n============================================================\n";
    std::cout << "   SWEEP COMPLETE\n";
    std::cout << "============================================================\n";
    std::cout << "\nResults exported to: output/sweeps/\n";

    return 0;
}
