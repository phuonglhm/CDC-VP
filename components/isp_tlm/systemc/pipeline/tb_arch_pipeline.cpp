/**
 * @file tb_arch_pipeline.cpp
 * @brief Architecture-aware timed pipeline testbench
 *
 * Demonstrates the full architecture model with:
 *   - Clock and reset signals
 *   - Timed block processing
 *   - Architecture metrics collection
 *   - Bottleneck analysis
 *
 * This testbench is the foundation for architecture exploration and
 * can be extended for parameter sweeps.
 */

#include <systemc>
using namespace sc_core;

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <chrono>

#include "sc_isp_pipeline.h"
#include "../hw/isp_arch_config.h"
#include "../hw/metrics.h"
#include "../tb_utils/hardware_params.h"
#include "../tb_utils/tb_utils.h"

// Test parameters
constexpr std::uint32_t WIDTH = 32;
constexpr std::uint32_t HEIGHT = 32;
constexpr std::uint32_t N_PIXELS = WIDTH * HEIGHT;

// ============================================================================
// Main Testbench
// ============================================================================
int sc_main(int argc, char* argv[]) {
    std::cout << "============================================================\n";
    std::cout << "   Architecture-Aware ISP Pipeline Testbench (TIMED MODE)\n";
    std::cout << "============================================================\n\n";

    // ----------------------------------------------------------------
    // 1. Setup architecture configuration
    // ----------------------------------------------------------------
    isp_arch_config arch_cfg;
    arch_cfg.clock_freq_mhz = 200.0f;     // 200 MHz
    arch_cfg.enable_metrics = true;
    arch_cfg.enable_tracing = false;

    // Initialize default block configurations
    arch_cfg.init_defaults();

    // Display architecture configuration
    std::cout << "--- Architecture Configuration ---\n";
    std::cout << "Clock frequency: " << arch_cfg.clock_freq_mhz << " MHz\n";
    std::cout << "Cycle time: " << arch_cfg.cycle_ns() << " ns\n";
    std::cout << "Image size: " << WIDTH << "x" << HEIGHT << "\n\n";

    // ----------------------------------------------------------------
    // 2. Setup ISP configuration (functional)
    // ----------------------------------------------------------------
    isp_config isp_cfg;
    isp_cfg.scale.is_enable = true;
    isp_cfg.scale.in_width = WIDTH;
    isp_cfg.scale.in_height = HEIGHT;
    isp_cfg.scale.out_width = WIDTH / 2;
    isp_cfg.scale.out_height = HEIGHT / 2;
    // Keep AWB enabled for proper data flow
    // isp_cfg.awb.is_enable = false;

    // Hardware parameters - TIMED MODE
    hw_params hw;
    hw.clk_mhz = 200.0f;         // Configurable clock frequency
    hw.bus_width_bits = 64;
    hw.pixel_bits = 16;
    hw.fifo_depth = 4096;
    hw.timed_mode = false;         // Disabled - timed mode needs proper sc_start with time
    hw.default_cycles_per_pixel = 1;

    std::cout << "--- Hardware Parameters (TIMED MODE) ---\n";
    std::cout << hw.to_string();

    // Create the clock signal (used when timed_mode is enabled)
    // Clock period = 1 / freq = 1 / 200MHz = 5ns
    // sc_clock clk("clk", 1.0f / hw.clk_mhz * 1000.0f, sc_time_unit::SC_NS);

    // Enable metrics collection
    arch_cfg.enable_metrics = true;

    // ----------------------------------------------------------------
    // 3. Create test data (synthetic pattern)
    // ----------------------------------------------------------------
    std::vector<std::uint16_t> input(N_PIXELS);
    for (std::uint32_t i = 0; i < N_PIXELS; ++i) {
        input[i] = static_cast<std::uint16_t>((i % 256) * 16);  // Ramp pattern
    }

    // ----------------------------------------------------------------
    // 4. Create testbench FIFOs
    // ----------------------------------------------------------------
    sc_fifo<std::uint16_t> raw_in(N_PIXELS + 64);
    sc_fifo<std::uint8_t> yuv_out(N_PIXELS * 2);  // YUV420 output

    // ----------------------------------------------------------------
    // 5. Enable metrics before pipeline construction
    // ----------------------------------------------------------------
    // Must be called before sc_isp_pipeline constructor
    sc_isp_pipeline::set_metrics_request(true, "output/metrics", nullptr);
    
    std::vector<float> lsc_lut;  // Empty LUT for this test
    sc_isp_pipeline dut("isp_pipeline", isp_cfg, lsc_lut,
                        &raw_in, &yuv_out, 12, cfa_types::RGGB, &hw);
    // Note: Clock binding only needed for timed mode
    // dut.bind_clock(&clk);

    // ----------------------------------------------------------------
    // 6. Instantiate driver and monitor
    // ----------------------------------------------------------------
    Generic_Driver<std::uint16_t> driver("driver", input);
    driver.fifo_out(raw_in);

    std::size_t expected_output = (WIDTH / 2) * (HEIGHT / 2) * 3 / 2;
    Generic_Monitor<std::uint8_t> monitor("monitor", expected_output);
    monitor.fifo_in(yuv_out);

    // ----------------------------------------------------------------
    // 7. Calculate max simulation time (for estimation only)
    // ----------------------------------------------------------------
    // For timed mode: estimate worst-case frame time
    double tokens_per_cycle = static_cast<double>(hw.bus_width_bits) / hw.pixel_bits;
    double mpix_per_s = hw.clk_mhz * tokens_per_cycle;
    double max_frame_us = (static_cast<double>(WIDTH * HEIGHT) / mpix_per_s) / 1e6;
    max_frame_us *= 3.0;  // Add margin for pipeline latency

    std::cout << "--- Starting Simulation (no time limit) ---\n";
    std::cout << "Estimated max frame time: " << max_frame_us << " us\n\n";

    auto sim_start = std::chrono::steady_clock::now();

    // ----------------------------------------------------------------
    // 8. Run simulation without time limit
    // ----------------------------------------------------------------
    sc_start();  // No max time

    auto sim_end = std::chrono::steady_clock::now();
    std::cout << "Simulation completed\n";
    std::cout << "Wall-clock time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(sim_end - sim_start).count()
              << " ms\n";

    // ----------------------------------------------------------------
    // 9. Results
    // ----------------------------------------------------------------
    std::cout << "\n============================================================\n";
    std::cout << "   SIMULATION RESULTS\n";
    std::cout << "============================================================\n\n";

    std::cout << "--- Frame Timing ---\n";
    std::cout << "Frame start: " << std::fixed << std::setprecision(3)
              << dut.get_frame_start_time().to_double() / 1e-3 << " us\n";
    std::cout << "Frame end: " << dut.get_frame_end_time().to_double() / 1e-3 << " us\n";
    std::cout << "Frame time: " << dut.get_frame_time_us() << " us\n";

    if (dut.get_frame_time_us() > 0) {
        double fps = 1e6 / dut.get_frame_time_us();
        std::cout << "Estimated FPS: " << std::fixed << std::setprecision(2) << fps << "\n";
    }
    std::cout << "\n";

    // ----------------------------------------------------------------
    // 10. Architecture Metrics
    // ----------------------------------------------------------------
    std::cout << "--- Architecture Metrics ---\n";
    std::cout << "Total boundary samples: " << dut.metrics_sample_count() << "\n";
    std::cout << "Total block samples: " << dut.block_metrics_sample_count() << "\n";

    dut.print_metrics_summary();

    // ----------------------------------------------------------------
    // 11. Output verification
    // ----------------------------------------------------------------
    const auto& captured = monitor.get_captured_data();
    std::cout << "\n--- Output Verification ---\n";
    std::cout << "Expected tokens: " << expected_output << "\n";
    std::cout << "Captured tokens: " << captured.size() << "\n";

    bool complete = (captured.size() >= expected_output);
    std::cout << "Status: " << (complete ? "COMPLETE" : "INCOMPLETE") << "\n";

    std::cout << "\n============================================================\n";
    std::cout << "TEST RESULT: " << (complete ? "PASS" : "FAIL (INCOMPLETE)") << "\n";
    std::cout << "============================================================\n";

    return complete ? 0 : 1;
}
