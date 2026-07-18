/**
 * @file tb_dma_integration.cpp
 * @brief Testbench demonstrating DMA and memory model integration
 *
 * Shows how to use:
 *   - frame_dma for input/output bandwidth simulation
 *   - local_memory for line buffer modeling
 *   - timed_input_stream/timed_output_stream for bandwidth throttling
 *   - Bandwidth limiting capabilities
 */

#include <systemc>
using namespace sc_core;

#include <iostream>
#include <iomanip>

#include "sc_isp_pipeline.h"
#include "../hw/frame_dma.h"
#include "../hw/local_memory.h"
#include "../hw/timed_stream.h"
#include "../hw/isp_arch_config.h"

// Test parameters
constexpr std::uint32_t WIDTH = 64;
constexpr std::uint32_t HEIGHT = 64;
constexpr std::uint32_t N_PIXELS = WIDTH * HEIGHT;

// DMA configuration for testing
frame_dma::dma_config input_dma_cfg;
frame_dma::dma_config output_dma_cfg;

// Stream configurations
stream_config input_stream_cfg;
stream_config output_stream_cfg;

// ============================================================================
// Driver Module - feeds input pixels with DMA-like behavior
// ============================================================================
class dma_driver_module : public sc_module {
public:
    sc_port<sc_fifo_out_if<std::uint16_t>> raw_out;
    std::vector<std::uint16_t>* input_data;

    SC_HAS_PROCESS(dma_driver_module);

    dma_driver_module(sc_module_name name, std::vector<std::uint16_t>* data)
        : sc_module(name), input_data(data) {
        SC_THREAD(run);
    }

    void run() {
        std::cout << "[Driver] Starting...\n";

        // Wait for pipeline to initialize
        wait(100, SC_NS);

        // Simulate DMA read with latency
        std::uint32_t pixels_per_burst = input_dma_cfg.burst_length * input_dma_cfg.bus_width_bits / 16;
        std::uint32_t burst_count = 0;

        for (std::uint32_t i = 0; i < input_data->size(); ++i) {
            // Simulate DMA burst read
            if (i % pixels_per_burst == 0) {
                // Wait for DMA read latency
                wait(input_dma_cfg.read_latency, SC_NS);
                burst_count++;
            }

            raw_out->write((*input_data)[i]);

            // Simulate bandwidth limiting
            if (input_stream_cfg.enable_throttling && input_stream_cfg.bandwidth_limit_mbps > 0) {
                float bytes_per_pixel = 2.0f;  // 16-bit
                float ns_per_byte = 1000.0f / (input_stream_cfg.bandwidth_limit_mbps / 8.0f);
                wait(static_cast<std::uint32_t>(bytes_per_pixel * ns_per_byte), SC_NS);
            }
        }

        std::cout << "[Driver] Completed writing " << input_data->size() << " pixels in "
                  << burst_count << " bursts\n";
    }
};

// ============================================================================
// Monitor Module - reads output with bandwidth throttling
// ============================================================================
class dma_monitor_module : public sc_module {
public:
    sc_port<sc_fifo_in_if<std::uint8_t>> yuv_in;
    std::size_t expected_tokens;
    std::size_t received = 0;

    SC_HAS_PROCESS(dma_monitor_module);

    dma_monitor_module(sc_module_name name, std::size_t expected)
        : sc_module(name), expected_tokens(expected), received(0) {
        SC_THREAD(run);
    }

    void run() {
        std::cout << "[Monitor] Starting...\n";

        const std::size_t max_wait = 500000;
        std::size_t wait_count = 0;

        while (received < expected_tokens) {
            if (yuv_in->num_available() > 0) {
                std::uint8_t val = yuv_in->read();
                ++received;

                // Simulate bandwidth limiting on output
                if (output_stream_cfg.enable_throttling && output_stream_cfg.bandwidth_limit_mbps > 0) {
                    float bytes_per_token = 1.0f;
                    float ns_per_byte = 1000.0f / (output_stream_cfg.bandwidth_limit_mbps / 8.0f);
                    wait(static_cast<std::uint32_t>(bytes_per_token * ns_per_byte), SC_NS);
                }
                wait_count = 0;
            } else {
                wait(10, SC_NS);
                if (++wait_count > max_wait) {
                    std::cout << "[Monitor] Timeout after " << wait_count << " waits\n";
                    break;
                }
            }
        }

        std::cout << "[Monitor] Completed reading " << received << " tokens\n";
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "============================================================\n";
    std::cout << "   DMA Integration Testbench v2\n";
    std::cout << "   Demonstrating DMA + Timed Stream Integration\n";
    std::cout << "============================================================\n\n";

    // ----------------------------------------------------------------
    // 1. DMA Configuration
    // ----------------------------------------------------------------
    input_dma_cfg.name = "input_dma";
    input_dma_cfg.bus_width_bits = 64;       // 64-bit bus
    input_dma_cfg.burst_length = 16;          // 16 beats per burst
    input_dma_cfg.read_latency = 4;           // 4 cycles latency
    input_dma_cfg.max_outstanding = 4;        // 4 outstanding transactions
    input_dma_cfg.max_bandwidth_mbps = 800.0f; // Limit to 800 Mbps

    output_dma_cfg.name = "output_dma";
    output_dma_cfg.bus_width_bits = 64;
    output_dma_cfg.burst_length = 16;
    output_dma_cfg.write_latency = 2;
    output_dma_cfg.max_outstanding = 4;
    output_dma_cfg.max_bandwidth_mbps = 400.0f; // Limit to 400 Mbps

    std::cout << "--- DMA Configuration ---\n";
    std::cout << "Input DMA:\n";
    std::cout << "  Bus width: " << input_dma_cfg.bus_width_bits << " bits\n";
    std::cout << "  Burst length: " << input_dma_cfg.burst_length << "\n";
    std::cout << "  Read latency: " << input_dma_cfg.read_latency << " cycles\n";
    std::cout << "  Bandwidth limit: " << input_dma_cfg.max_bandwidth_mbps << " Mbps\n";

    std::cout << "\nOutput DMA:\n";
    std::cout << "  Bus width: " << output_dma_cfg.bus_width_bits << " bits\n";
    std::cout << "  Burst length: " << output_dma_cfg.burst_length << "\n";
    std::cout << "  Write latency: " << output_dma_cfg.write_latency << " cycles\n";
    std::cout << "  Bandwidth limit: " << output_dma_cfg.max_bandwidth_mbps << " Mbps\n\n";

    // ----------------------------------------------------------------
    // 2. Stream Configuration (for timed streams)
    // ----------------------------------------------------------------
    input_stream_cfg.name = "input_stream";
    input_stream_cfg.bus_width_bits = 64;
    input_stream_cfg.max_burst = 16;
    input_stream_cfg.bandwidth_limit_mbps = 800.0f;
    input_stream_cfg.read_latency_cycles = 4;
    input_stream_cfg.enable_throttling = true;

    output_stream_cfg.name = "output_stream";
    output_stream_cfg.bus_width_bits = 64;
    output_stream_cfg.max_burst = 16;
    output_stream_cfg.bandwidth_limit_mbps = 400.0f;
    output_stream_cfg.write_latency_cycles = 2;
    output_stream_cfg.enable_throttling = true;

    std::cout << "--- Stream Configuration ---\n";
    std::cout << "Input Stream:\n";
    std::cout << "  Bandwidth limit: " << input_stream_cfg.bandwidth_limit_mbps << " Mbps\n";
    std::cout << "  Throttling: " << (input_stream_cfg.enable_throttling ? "enabled" : "disabled") << "\n";
    std::cout << "  Read latency: " << input_stream_cfg.read_latency_cycles << " cycles\n\n";

    std::cout << "Output Stream:\n";
    std::cout << "  Bandwidth limit: " << output_stream_cfg.bandwidth_limit_mbps << " Mbps\n";
    std::cout << "  Throttling: " << (output_stream_cfg.enable_throttling ? "enabled" : "disabled") << "\n";
    std::cout << "  Write latency: " << output_stream_cfg.write_latency_cycles << " cycles\n\n";

    // ----------------------------------------------------------------
    // 3. Local Memory Configuration (for spatial blocks)
    // ----------------------------------------------------------------
    local_mem_config lsc_mem_cfg;
    lsc_mem_cfg.name = "lsc_scratchpad";
    lsc_mem_cfg.depth = 4096;           // 4K entries
    lsc_mem_cfg.width_bits = 16;         // 16-bit pixels
    lsc_mem_cfg.num_banks = 2;          // Dual-bank for ping-pong
    lsc_mem_cfg.read_ports = 1;
    lsc_mem_cfg.write_ports = 1;
    lsc_mem_cfg.latency_cycles = 1;
    lsc_mem_cfg.collision_modeling = true;

    local_mem_config spatial_mem_cfg;
    spatial_mem_cfg.name = "spatial_buffer";
    spatial_mem_cfg.depth = 8192;        // 8K entries
    spatial_mem_cfg.width_bits = 48;     // RGB 16-bit each
    spatial_mem_cfg.num_banks = 4;       // 4-bank for parallel access
    spatial_mem_cfg.read_ports = 2;
    spatial_mem_cfg.write_ports = 2;
    spatial_mem_cfg.latency_cycles = 2;
    spatial_mem_cfg.collision_modeling = true;

    std::cout << "--- Local Memory Configuration ---\n";
    std::cout << "LSC Scratchpad:\n";
    std::cout << "  Depth: " << lsc_mem_cfg.depth << " entries\n";
    std::cout << "  Width: " << lsc_mem_cfg.width_bits << " bits\n";
    std::cout << "  Banks: " << lsc_mem_cfg.num_banks << "\n";
    std::cout << "  Latency: " << lsc_mem_cfg.latency_cycles << " cycle(s)\n";

    std::cout << "\nSpatial Buffer:\n";
    std::cout << "  Depth: " << spatial_mem_cfg.depth << " entries\n";
    std::cout << "  Width: " << spatial_mem_cfg.width_bits << " bits\n";
    std::cout << "  Banks: " << spatial_mem_cfg.num_banks << "\n";
    std::cout << "  Latency: " << spatial_mem_cfg.latency_cycles << " cycle(s)\n\n";

    // ----------------------------------------------------------------
    // 4. Calculate expected bandwidth
    // ----------------------------------------------------------------
    std::uint64_t input_frame_bytes = N_PIXELS * 2;  // 16-bit pixels
    std::uint64_t output_frame_bytes =
        static_cast<std::uint64_t>(WIDTH) * HEIGHT +
        2u * ((WIDTH + 1u) / 2u) * ((HEIGHT + 1u) / 2u);  // YUV420

    float bus_freq_mhz = 200.0f;
    float input_cycles = (float)(input_frame_bytes * 8) / input_dma_cfg.bus_width_bits;
    float ideal_input_time_us = input_cycles / bus_freq_mhz;

    float output_cycles = (float)(output_frame_bytes * 8) / output_dma_cfg.bus_width_bits;
    float ideal_output_time_us = output_cycles / bus_freq_mhz;

    // Calculate time with bandwidth limiting
    float limited_input_time_us = (float)input_frame_bytes / (input_dma_cfg.max_bandwidth_mbps / 8.0f) * 1e6;
    float limited_output_time_us = (float)output_frame_bytes / (output_dma_cfg.max_bandwidth_mbps / 8.0f) * 1e6;

    std::cout << "--- Bandwidth Analysis ---\n";
    std::cout << "Input frame size: " << input_frame_bytes << " bytes\n";
    std::cout << "Output frame size: " << output_frame_bytes << " bytes\n\n";

    std::cout << "Input DMA:\n";
    std::cout << "  Ideal time (@unlimited): " << std::fixed << std::setprecision(2)
              << ideal_input_time_us << " us\n";
    std::cout << "  Limited time (@" << input_dma_cfg.max_bandwidth_mbps << " Mbps): "
              << std::fixed << std::setprecision(2) << limited_input_time_us << " us\n";
    std::cout << "  Overhead: +" << std::fixed << std::setprecision(1)
              << ((limited_input_time_us / ideal_input_time_us - 1.0f) * 100.0f) << "%\n";

    std::cout << "\nOutput DMA:\n";
    std::cout << "  Ideal time (@unlimited): " << std::fixed << std::setprecision(2)
              << ideal_output_time_us << " us\n";
    std::cout << "  Limited time (@" << output_dma_cfg.max_bandwidth_mbps << " Mbps): "
              << std::fixed << std::setprecision(2) << limited_output_time_us << " us\n";
    std::cout << "  Overhead: +" << std::fixed << std::setprecision(1)
              << ((limited_output_time_us / ideal_output_time_us - 1.0f) * 100.0f) << "%\n\n";

    // ----------------------------------------------------------------
    // 5. Create test data
    // ----------------------------------------------------------------
    std::vector<std::uint16_t> input(N_PIXELS);
    for (std::uint32_t i = 0; i < N_PIXELS; ++i) {
        std::uint16_t val = static_cast<std::uint16_t>((i % 256) * 16);
        if ((i / WIDTH) % 4 == 0) val = 0xFFF;  // Bright rows
        else if ((i / WIDTH) % 4 == 2) val = 0x100;  // Dark rows
        input[i] = val;
    }

    std::cout << "--- Test Data ---\n";
    std::cout << "Generated " << N_PIXELS << " pixels (";
    std::cout << WIDTH << "x" << HEIGHT << ")\n";
    std::cout << "Pattern: gradient with bright/dark rows for AWB/AEC\n\n";

    // ----------------------------------------------------------------
    // 6. Create testbench FIFOs
    // ----------------------------------------------------------------
    sc_fifo<std::uint16_t> raw_in(N_PIXELS + 64);
    sc_fifo<std::uint8_t> yuv_out(N_PIXELS * 2);

    // ----------------------------------------------------------------
    // 7. Instantiate pipeline
    // ----------------------------------------------------------------
    isp_config isp_cfg;
    isp_cfg.scale.is_enable = false;
    isp_cfg.bnr.is_enable = false;
    isp_cfg.sharpen.is_enable = false;
    isp_cfg.cse.is_enable = false;
    isp_cfg.scale.in_width = WIDTH;
    isp_cfg.scale.in_height = HEIGHT;
    isp_cfg.yuv420.is_enable = true;

    std::vector<float> lsc_lut;
    sc_isp_pipeline dut("isp_pipeline", isp_cfg, lsc_lut,
                        &raw_in, &yuv_out, 12, cfa_types::RGGB);

    dut.enable_metrics = true;
    dut.metrics_output_dir = "output/dma_test";

    // Architecture configuration
    isp_arch_config arch_cfg;
    arch_cfg.clock_freq_mhz = 200.0f;
    arch_cfg.enable_metrics = true;
    arch_cfg.init_defaults();

    dut.enable_arch_metrics("output/dma_test");

    std::cout << "--- Pipeline Configuration ---\n";
    std::cout << "Clock: " << arch_cfg.clock_freq_mhz << " MHz\n";
    std::cout << "Scale: " << (isp_cfg.scale.is_enable ? "enabled" : "disabled") << "\n";
    std::cout << "BNR: " << (isp_cfg.bnr.is_enable ? "enabled" : "disabled") << "\n";
    std::cout << "Sharpen: " << (isp_cfg.sharpen.is_enable ? "enabled" : "disabled") << "\n\n";

    // ----------------------------------------------------------------
    // 8. Instantiate driver and monitor modules
    // ----------------------------------------------------------------
    dma_driver_module driver("dma_driver", &input);
    driver.raw_out(raw_in);

    std::size_t expected_output = static_cast<std::size_t>(output_frame_bytes);
    dma_monitor_module monitor("dma_monitor", expected_output);
    monitor.yuv_in(yuv_out);

    // ----------------------------------------------------------------
    // 9. Run simulation
    // ----------------------------------------------------------------
    auto start_time = sc_time_stamp();
    std::cout << "--- Starting Simulation ---\n";
    sc_start();
    auto end_time = sc_time_stamp();

    // ----------------------------------------------------------------
    // 10. Results
    // ----------------------------------------------------------------
    auto sim_time = end_time - start_time;

    std::cout << "\n============================================================\n";
    std::cout << "   SIMULATION RESULTS\n";
    std::cout << "============================================================\n\n";

    std::cout << "--- Timing ---\n";
    std::cout << "Simulation time: " << sim_time.to_seconds() / 1e-6 << " us\n";
    std::cout << "Sim cycles (@200MHz): " << sim_time.to_seconds() / 5e-9 << "\n\n";

    std::cout << "--- Frame Timing ---\n";
    std::cout << "Frame time: " << dut.get_frame_time_us() << " us\n";
    if (dut.get_frame_time_us() > 0) {
        double fps = 1e6 / dut.get_frame_time_us();
        std::cout << "Estimated FPS: " << std::fixed << std::setprecision(2) << fps << "\n";
    }
    std::cout << "\n";

    // ----------------------------------------------------------------
    // 11. Architecture Metrics
    // ----------------------------------------------------------------
    std::cout << "--- Architecture Metrics ---\n";
    dut.collect_block_metrics();
    dut.update_arch_frame_timing(0);
    dut.dump_arch_metrics();
    dut.dump_arch_summary();

    // ----------------------------------------------------------------
    // 12. Output Verification
    // ----------------------------------------------------------------
    std::cout << "\n--- Output Verification ---\n";
    std::cout << "Output tokens received: " << monitor.received << "\n";
    std::cout << "Expected size: " << expected_output << "\n";

    bool test_passed = (monitor.received >= expected_output);

    if (test_passed) {
        std::cout << "TEST RESULT: PASS\n";
    } else {
        std::cout << "TEST RESULT: INCOMPLETE\n";
        std::cout << "  Expected: " << expected_output << " tokens\n";
        std::cout << "  Got: " << monitor.received << " tokens\n";
        std::cout << "  Missing: " << (expected_output - monitor.received) << " tokens\n";
    }

    // ----------------------------------------------------------------
    // 13. Bandwidth Analysis Summary
    // ----------------------------------------------------------------
    std::cout << "\n--- Bandwidth Analysis Summary ---\n";
    if (dut.get_frame_time_us() > 0) {
        float actual_fps = 1e6 / dut.get_frame_time_us();
        float input_bw = (float)input_frame_bytes * actual_fps / 1e6;  // MB/s
        float output_bw = (float)output_frame_bytes * actual_fps / 1e6;  // MB/s

        std::cout << "Actual throughput:\n";
        std::cout << "  FPS: " << std::fixed << std::setprecision(2) << actual_fps << "\n";
        std::cout << "  Input BW: " << std::fixed << std::setprecision(1) << input_bw << " MB/s\n";
        std::cout << "  Output BW: " << std::fixed << std::setprecision(1) << output_bw << " MB/s\n";

        if (input_bw > input_dma_cfg.max_bandwidth_mbps * 0.9f) {
            std::cout << "  WARNING: Approaching input bandwidth limit!\n";
        }
        if (output_bw > output_dma_cfg.max_bandwidth_mbps * 0.9f) {
            std::cout << "  WARNING: Approaching output bandwidth limit!\n";
        }
    }

    std::cout << "\n============================================================\n";
    std::cout << "DMA integration test completed.\n";
    std::cout << "Metrics files written to: output/dma_test/\n";
    std::cout << "============================================================\n";

    return test_passed ? 0 : 1;
}
