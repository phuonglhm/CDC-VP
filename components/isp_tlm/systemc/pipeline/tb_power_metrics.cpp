/**
 * @file tb_power_metrics.cpp
 * @brief Testbench demonstrating power estimation for ISP pipeline
 *
 * Shows how to:
 *   - Configure power models for each block type
 *   - Calculate power based on utilization
 *   - Estimate frame energy and average power
 *   - Generate power reports
 */

#include <systemc>
using namespace sc_core;

#include <iostream>
#include <iomanip>
#include <string>

#include "../hw/power.h"
#include "../hw/metrics.h"

// Test configuration
constexpr std::uint32_t WIDTH = 1920;
constexpr std::uint32_t HEIGHT = 1080;
constexpr std::uint32_t N_PIXELS = WIDTH * HEIGHT;
constexpr float CLK_FREQ_MHZ = 200.0f;

int sc_main(int argc, char* argv[]) {
    std::cout << "============================================================\n";
    std::cout << "   ISP Pipeline Power Estimation Testbench\n";
    std::cout << "============================================================\n\n";

    // ----------------------------------------------------------------
    // 1. Setup power estimator
    // ----------------------------------------------------------------
    power_estimator estimator("output/power");

    std::cout << "--- Power Configuration ---\n";
    std::cout << "Clock frequency: " << CLK_FREQ_MHZ << " MHz\n";
    std::cout << "Image size: " << WIDTH << "x" << HEIGHT << " (" << N_PIXELS << " pixels)\n\n";

    // ----------------------------------------------------------------
    // 2. Define block configurations and estimated utilization
    // ----------------------------------------------------------------
    struct block_activity {
        std::string name;
        block_power_config config;
        double utilization;  // 0.0 - 1.0
        std::uint64_t cycles;
        std::uint64_t pixels;
    };

    std::vector<block_activity> blocks = {
        // RAW domain (12-bit processing)
        {"normalizer", block_power_defaults::normalizer(), 0.95, 2050000, N_PIXELS},
        {"blc",        block_power_defaults::blc(),        0.92, 1980000, N_PIXELS},
        {"dpc",        block_power_defaults::dpc(),        0.88, 1900000, N_PIXELS},
        {"lsc",        block_power_defaults::lsc(),        0.90, 1940000, N_PIXELS},
        {"dg",         block_power_defaults::dg(),         0.95, 2050000, N_PIXELS},
        {"bnr",        block_power_defaults::bnr(),        0.75, 1620000, N_PIXELS},

        // RGB domain
        {"demosaic",   block_power_defaults::demosaic(),  0.70, 1510000, N_PIXELS},
        {"awb",        block_power_defaults::awb(),        0.30, 650000,  N_PIXELS},
        {"wb",         block_power_defaults::wb(),         0.95, 2050000, N_PIXELS},
        {"ccm",        block_power_defaults::ccm(),        0.95, 2050000, N_PIXELS},
        {"gc",         block_power_defaults::point_op(),   0.95, 2050000, N_PIXELS},
        {"aec",        block_power_defaults::aec(),        0.25, 540000,  N_PIXELS},

        // YUV domain
        {"csc",        block_power_defaults::csc(),        0.90, 1940000, N_PIXELS},
        {"cse",        block_power_defaults::point_op(),   0.85, 1830000, N_PIXELS},
        {"sharpen",    block_power_defaults::sharpen(),   0.80, 1720000, N_PIXELS},
        {"2dnr",       block_power_defaults::tdnoise_reduction(), 0.70, 1510000, N_PIXELS},
        {"scale",      block_power_defaults::scale(),      0.85, 1830000, N_PIXELS/4},
        {"yuv420",     block_power_defaults::yuv420(),     0.90, 1940000, N_PIXELS/4},
    };

    // ----------------------------------------------------------------
    // 3. Calculate power for each block
    // ----------------------------------------------------------------
    pipeline_power_summary summary;

    std::cout << "--- Per-Block Power Estimation ---\n";
    std::cout << std::setw(14) << "Block"
              << std::setw(12) << "Util %"
              << std::setw(12) << "Static mW"
              << std::setw(12) << "Dynamic mW"
              << std::setw(12) << "Memory mW"
              << std::setw(12) << "Total mW"
              << "\n";
    std::cout << std::string(74, '-') << "\n";

    for (const auto& block : blocks) {
        // Calculate base power
        power_values base = estimator.calculate_power(
            block.config,
            block.cycles,
            block.cycles * 10,  // Assume 10% of time active
            block.pixels * 2,   // Memory accesses
            block.pixels
        );

        // Scale by utilization
        power_values scaled = estimator.scaled_power(base, block.utilization);

        // Add to summary
        summary.add_block(block.name, scaled);

        // Print
        std::cout << std::setw(14) << block.name
                  << std::setw(11) << std::fixed << std::setprecision(1)
                  << (block.utilization * 100) << "%"
                  << std::setw(12) << std::fixed << std::setprecision(3)
                  << scaled.static_mw
                  << std::setw(12) << scaled.dynamic_mw
                  << std::setw(12) << scaled.memory_mw
                  << std::setw(12) << scaled.total_mw
                  << "\n";
    }

    // ----------------------------------------------------------------
    // 4. Print total power summary
    // ----------------------------------------------------------------
    std::cout << std::string(74, '-') << "\n";
    std::cout << std::setw(14) << "TOTAL"
              << std::setw(11) << ""
              << std::setw(12) << summary.total.static_mw
              << std::setw(12) << summary.total.dynamic_mw
              << std::setw(12) << summary.total.memory_mw
              << std::setw(12) << summary.total.total_mw
              << "\n\n";

    // ----------------------------------------------------------------
    // 5. Frame energy calculations
    // ----------------------------------------------------------------
    // Estimate frame time based on pixel rate
    double pixels_per_us = CLK_FREQ_MHZ;  // 1 pixel per cycle @ 200MHz
    double frame_time_us = N_PIXELS / pixels_per_us;

    // FPS at this configuration
    double fps = 1e6 / frame_time_us;

    summary.calculate_frame_stats(frame_time_us, fps);

    std::cout << "--- Frame Energy Estimation ---\n";
    std::cout << "Frame time: " << std::fixed << std::setprecision(2)
              << frame_time_us << " us\n";
    std::cout << "FPS (estimated): " << fps << "\n";
    std::cout << "Frame energy: " << summary.frame_energy_nj << " nJ\n";
    std::cout << "Average power: " << summary.avg_power_mw << " mW\n";
    std::cout << "Peak power: " << summary.peak_power_mw << " mW\n\n";

    // ----------------------------------------------------------------
    // 6. Power breakdown by category
    // ----------------------------------------------------------------
    std::cout << "--- Power Breakdown ---\n";
    double total = summary.total.total_mw;
    std::cout << "Static power:  " << std::setw(8) << summary.total.static_mw << " mW  ("
              << std::fixed << std::setprecision(1)
              << (summary.total.static_mw / total * 100) << "%)\n";
    std::cout << "Dynamic power: " << std::setw(8) << summary.total.dynamic_mw << " mW  ("
              << (summary.total.dynamic_mw / total * 100) << "%)\n";
    std::cout << "Memory power:  " << std::setw(8) << summary.total.memory_mw << " mW  ("
              << (summary.total.memory_mw / total * 100) << "%)\n";
    std::cout << "Total:        " << std::setw(8) << total << " mW\n\n";

    // ----------------------------------------------------------------
    // 7. Compare different configurations
    // ----------------------------------------------------------------
    std::cout << "--- Configuration Comparison ---\n";
    std::cout << std::setw(20) << "Config"
              << std::setw(15) << "FPS"
              << std::setw(15) << "Frame Energy"
              << std::setw(15) << "Avg Power"
              << "\n";
    std::cout << std::string(65, '-') << "\n";

    // Config 1: 1080p @ 200MHz
    {
        double ft = (1920.0 * 1080.0) / CLK_FREQ_MHZ;
        double f = 1e6 / ft;
        double e = total * ft / 1000.0;
        double p = e * 1000.0 / ft;
        std::cout << std::setw(20) << "1080p@200MHz"
                  << std::setw(15) << std::fixed << std::setprecision(1) << f
                  << std::setw(14) << std::fixed << std::setprecision(1) << e << " nJ"
                  << std::setw(14) << std::fixed << std::setprecision(1) << p << " mW\n";
    }

    // Config 2: 4K @ 200MHz
    {
        double ft = (3840.0 * 2160.0) / CLK_FREQ_MHZ;
        double f = 1e6 / ft;
        double e = total * ft / 1000.0;
        double p = e * 1000.0 / ft;
        std::cout << std::setw(20) << "4K@200MHz"
                  << std::setw(15) << std::fixed << std::setprecision(1) << f
                  << std::setw(14) << std::fixed << std::setprecision(1) << e << " nJ"
                  << std::setw(14) << std::fixed << std::setprecision(1) << p << " mW\n";
    }

    // Config 3: 1080p @ 400MHz
    {
        double ft = (1920.0 * 1080.0) / (CLK_FREQ_MHZ * 2);
        double f = 1e6 / ft;
        double e = total * ft / 1000.0;
        double p = e * 1000.0 / ft;
        std::cout << std::setw(20) << "1080p@400MHz"
                  << std::setw(15) << std::fixed << std::setprecision(1) << f
                  << std::setw(14) << std::fixed << std::setprecision(1) << e << " nJ"
                  << std::setw(14) << std::fixed << std::setprecision(1) << p << " mW\n";
    }

    // Config 4: 720p @ 200MHz
    {
        double ft = (1280.0 * 720.0) / CLK_FREQ_MHZ;
        double f = 1e6 / ft;
        double e = total * ft / 1000.0;
        double p = e * 1000.0 / ft;
        std::cout << std::setw(20) << "720p@200MHz"
                  << std::setw(15) << std::fixed << std::setprecision(1) << f
                  << std::setw(14) << std::fixed << std::setprecision(1) << e << " nJ"
                  << std::setw(14) << std::fixed << std::setprecision(1) << p << " mW\n";
    }

    std::cout << "\n============================================================\n";
    std::cout << "Power estimation complete.\n";
    std::cout << "============================================================\n";

    return 0;
}
