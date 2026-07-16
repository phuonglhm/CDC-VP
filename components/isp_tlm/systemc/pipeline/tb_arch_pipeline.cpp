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
 * can be extended for parameter sweeps (Phase 7 of plan.md).
 */

#include <systemc>
using namespace sc_core;

#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstring>

#include "sc_isp_pipeline.h"
#include "../hw/isp_arch_config.h"
#include "../hw/metrics.h"

// Test parameters
constexpr std::uint32_t WIDTH = 32;
constexpr std::uint32_t HEIGHT = 32;
constexpr std::uint32_t N_PIXELS = WIDTH * HEIGHT;

// ============================================================================
// Driver Module - feeds input pixels to pipeline
// ============================================================================
class driver_module : public sc_module {
public:
    sc_port<sc_fifo_out_if<std::uint16_t>> raw_out;
    std::vector<std::uint16_t>* input_data;

    SC_HAS_PROCESS(driver_module);

    driver_module(sc_module_name name, std::vector<std::uint16_t>* data)
        : sc_module(name), input_data(data) {
        SC_THREAD(run);
    }

    void run() {
        std::cout << "[Driver] Starting...\n";

        // Wait a bit for pipeline to initialize
        wait(100, SC_NS);

        // Write input pixels
        for (std::uint32_t i = 0; i < input_data->size(); ++i) {
            raw_out->write((*input_data)[i]);
            if (i % 1000 == 0 && i > 0) {
                std::cout << "[Driver] Wrote " << i << "/" << input_data->size() << " pixels\n";
            }
        }

        std::cout << "[Driver] Completed writing " << input_data->size() << " pixels\n";
    }
};

// ============================================================================
// Monitor Module - collects output from pipeline
// ============================================================================
class monitor_module : public sc_module {
public:
    sc_port<sc_fifo_in_if<std::uint8_t>> yuv_in;
    std::size_t expected_tokens;
    std::size_t received = 0;

    SC_HAS_PROCESS(monitor_module);

    monitor_module(sc_module_name name, std::size_t expected)
        : sc_module(name), expected_tokens(expected), received(0) {
        SC_THREAD(run);
    }

    void run() {
        std::cout << "[Monitor] Starting...\n";

        // YUV420 output
        const std::size_t max_wait = 100000;  // Prevent infinite loop
        int wait_count = 0;

        while (received < expected_tokens) {
            if (yuv_in->num_available() > 0) {
                std::uint8_t val = yuv_in->read();
                ++received;
                if (received % 1000 == 0 && received > 0) {
                    std::cout << "[Monitor] Read " << received << " tokens\n";
                }
                wait_count = 0;
            } else {
                wait(10, SC_NS);
                if (++wait_count > static_cast<int>(max_wait)) {
                    std::cout << "[Monitor] Timeout after " << max_wait << " waits\n";
                    break;
                }
            }
        }

        std::cout << "[Monitor] Completed reading " << received << " tokens\n";
    }
};

// ============================================================================
// Main Testbench
// ============================================================================
int sc_main(int argc, char* argv[]) {
    std::cout << "============================================================\n";
    std::cout << "   Architecture-Aware ISP Pipeline Testbench\n";
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
    // 5. Instantiate pipeline
    // ----------------------------------------------------------------
    std::vector<float> lsc_lut;  // Empty LUT for this test
    sc_isp_pipeline dut("isp_pipeline", isp_cfg, lsc_lut,
                        &raw_in, &yuv_out, 12, cfa_types::RGGB);

    // Enable metrics collection
    dut.enable_metrics = true;
    dut.metrics_output_dir = "output/arch_metrics";

    // Enable architecture metrics (Phase 5)
    dut.enable_arch_metrics("output/arch_metrics");

    // ----------------------------------------------------------------
    // 6. Instantiate driver and monitor (as proper SystemC modules)
    // ----------------------------------------------------------------
    driver_module driver("driver", &input);
    driver.raw_out(raw_in);

    std::size_t expected_output = (WIDTH / 2) * (HEIGHT / 2) * 3 / 2;
    monitor_module monitor("monitor", expected_output);
    monitor.yuv_in(yuv_out);

    // ----------------------------------------------------------------
    // 7. Run simulation
    // ----------------------------------------------------------------
    std::cout << "--- Starting Simulation ---\n";
    sc_start();

    // ----------------------------------------------------------------
    // 8. Results
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
    // 9. Architecture Metrics
    // ----------------------------------------------------------------
    std::cout << "--- Architecture Metrics ---\n";

    // Collect block metrics into arch_metrics
    dut.collect_block_metrics();

    // Update frame timing
    dut.update_arch_frame_timing(0);

    // Dump architecture metrics report
    dut.dump_arch_metrics();

    // Dump summary files (CSVs)
    dut.dump_arch_summary();

    // Also dump block and pipeline metrics
    dut.dump_all_block_metrics();
    dut.dump_pipeline_metrics();

    // ----------------------------------------------------------------
    // 10. Output Verification
    // ----------------------------------------------------------------
    std::cout << "\n--- Output Verification ---\n";
    std::cout << "Output tokens received: " << monitor.received << "\n";
    std::cout << "Expected: " << expected_output << "\n";

    if (monitor.received >= expected_output) {
        std::cout << "TEST RESULT: PASS\n";
    } else {
        std::cout << "TEST RESULT: INCOMPLETE (expected " << expected_output
                  << ", got " << monitor.received << ")\n";
    }

    std::cout << "\n============================================================\n";
    std::cout << "Metrics files written to: output/arch_metrics/\n";
    std::cout << "============================================================\n";

    return 0;
}
