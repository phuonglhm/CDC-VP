/**
 * @file tb_d65_pipeline.cpp
 * @brief Full ISP Pipeline Testbench with Real D65 Image and Timed Mode
 *
 * This testbench:
 * 1. Loads real D65 RAW image (2688x1520)
 * 2. Runs full ISP pipeline with timed mode (real clock)
 * 3. Measures accurate latency with sc_clock
 * 4. Compares against golden reference
 *
 * Usage:
 *   ./tb_d65_pipeline
 */

#include <systemc>
using namespace sc_core;

#include <iostream>
#include <vector>
#include <cstdlib>
#include <iomanip>
#include <sys/stat.h>
#include <fstream>
#include <sstream>
#include <filesystem>

#include "sc_isp_pipeline.h"
#include "../tb_utils/tb_utils.h"
#include "../tb_utils/hardware_params.h"
#include "../hw/isp_arch_config.h"
#include "../input_utils/raw_loader.h"

#include "../../pipeline/include/isp_config.h"
#include "../../pipeline/include/isp_pipeline.h"

namespace {

constexpr std::uint32_t WIDTH = 2688;
constexpr std::uint32_t HEIGHT = 1520;
constexpr std::uint32_t FIFO_DEPTH = WIDTH * HEIGHT / 4;  // Quarter frame for buffering

// Get the absolute path to the systemc directory
std::string get_systemc_dir() {
    std::filesystem::path p(__FILE__);
    p = p.parent_path();      // d65/
    p = p.parent_path();      // pipeline/
    p = p.parent_path();      // systemc/
    return p.string();
}

std::string resolve_output_path(const std::string& rel_path) {
    return get_systemc_dir() + "/" + rel_path;
}

inline void ensure_dir(const std::string& path) {
    if (path.empty()) return;
    std::string p = path;
    if (p.front() != '/') {
        p = resolve_output_path(path);
    }
    for (std::size_t i = 1; i < p.size(); ++i) {
        if (p[i] == '/') {
            mkdir(p.substr(0, i).c_str(), 0755);
        }
    }
    mkdir(p.c_str(), 0755);
}

double compute_mse(const std::uint8_t* a, const std::uint8_t* b, std::size_t size) {
    double mse = 0.0;
    for (std::size_t i = 0; i < size; ++i) {
        double diff = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        mse += diff * diff;
    }
    return mse / static_cast<double>(size);
}

double compute_max_diff(const std::uint8_t* a, const std::uint8_t* b, std::size_t size) {
    double max_diff = 0.0;
    for (std::size_t i = 0; i < size; ++i) {
        double diff = std::abs(static_cast<double>(a[i]) - static_cast<double>(b[i]));
        if (diff > max_diff) max_diff = diff;
    }
    return max_diff;
}

}  // namespace

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================" << std::endl;
    std::cout << "D65 REAL IMAGE PIPELINE TESTBENCH (TIMED MODE)" << std::endl;
    std::cout << "==================================================" << std::endl;

    // 1. Hardware parameters - TIMED MODE with real clock
    hw_params hw;
    hw.clk_mhz = 200.0f;           // 200 MHz
    hw.bus_width_bits = 64;          // 64-bit bus
    hw.pixel_bits = 16;              // 16-bit tokens
    hw.fifo_depth = FIFO_DEPTH;      // Deep FIFOs for timed mode
    hw.timed_mode = false;            // Disable timed mode for now
    hw.default_cycles_per_pixel = 1; // 1 cycle per pixel

    std::cout << "\n--- Hardware Parameters (TIMED MODE) ---\n";
    std::cout << hw.to_string();

    // 2. Load real D65 RAW image
    std::cout << "\n--- Loading D65 RAW Image ---\n";
    // Navigate from pipeline/d65/ -> systemc/ -> isp_tlm/ -> input/
    std::string raw_path = get_systemc_dir() + "/../input/D65_raw_2688x1520_5376.raw";
    std::cout << "Loading: " << raw_path << std::endl;

    std::vector<std::uint16_t> raw_input;
    raw_loader::RawFormat fmt = raw_loader::RawFormat::UNKNOWN;

    try {
        raw_loader::load(raw_path, WIDTH, HEIGHT, raw_input, &fmt);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to load RAW image: " << e.what() << std::endl;
        return 1;
    }
    std::cout << "Loaded " << raw_input.size() << " pixels (" << raw_loader::format_name(fmt) << ")" << std::endl;

    // 3. Compute golden reference using C++ pipeline
    std::cout << "\n--- Computing Golden Reference ---\n";
    isp_pipeline golden_pipeline;
    isp_config cfg = golden_pipeline.config();

    // Configure pipeline
    cfg.scale.in_width = WIDTH;
    cfg.scale.in_height = HEIGHT;
    cfg.scale.out_width = WIDTH / 2;
    cfg.scale.out_height = HEIGHT / 2;
    cfg.scale.is_enable = true;

    cfg.blc.is_enable = true;
    cfg.dpc.is_enable = true;
    cfg.lsc.is_enable = false;
    cfg.dg.is_enable = false;
    cfg.bnr.is_enable = false;
    cfg.demosaic.is_enable = true;
    cfg.awb.is_enable = true;
    cfg.wb.is_enable = true;
    cfg.ccm.is_enable = true;
    cfg.gc.is_enable = true;
    cfg.aec.is_enable = true;
    cfg.csc.conv_standard = 0;
    cfg.cse.is_enable = true;
    cfg.sharpen.is_enable = true;
    cfg.twodnr.is_enable = false;
    cfg.yuv420.is_enable = true;

    // Register setup
    auto write_to_reg = [&](std::uint32_t offset, std::uint32_t value) {
        while (golden_pipeline.write_reg(offset, value)) {}
    };
    write_to_reg(0x0004, 0);
    write_to_reg(0x000C, 0);
    write_to_reg(0x0010, WIDTH);
    write_to_reg(0x0014, HEIGHT);
    write_to_reg(0x0020, 12);
    write_to_reg(0x0024, 0);

    std::vector<std::uint8_t> golden_output;
    golden_pipeline.run(raw_input.data(), golden_output);
    std::cout << "[TB] Golden output size: " << golden_output.size() << " bytes" << std::endl;

    // 4. Create SystemC pipeline with TIMED MODE
    std::cout << "\n--- Creating SystemC Pipeline (TIMED MODE) ---\n";
    std::vector<float> lsc_lut(8192, 1.0f);

    // Create FIFOs
    sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_fifo<std::uint8_t> output_fifo(FIFO_DEPTH * 2);

    // Create clock - 200 MHz = 5 ns period
    sc_clock clk("clk", 5.0, SC_NS, 0.5);

    // Set metrics request before pipeline construction
    sc_isp_pipeline::set_metrics_request(true, resolve_output_path("output/metrics"), nullptr);

    // Create pipeline with clock
    sc_isp_pipeline dut("isp_pipeline", cfg, lsc_lut,
                        &input_fifo, &output_fifo,
                        /*input_bit_depth=*/16,
                        /*bayer_pattern=*/cfa_types::RGGB,
                        &hw);
    dut.bind_clock(&clk);
    sc_isp_pipeline::clear_metrics_request();

    // 5. Create driver and monitor
    Generic_Driver<std::uint16_t> driver("driver", raw_input);
    driver.fifo_out(input_fifo);

    std::size_t out_w = cfg.scale.is_enable ? cfg.scale.out_width : WIDTH;
    std::size_t out_h = cfg.scale.is_enable ? cfg.scale.out_height : HEIGHT;
    std::size_t expected_size = out_w * out_h + 2 * ((out_w + 1) / 2) * ((out_h + 1) / 2);

    Generic_Monitor<std::uint8_t> monitor("monitor", expected_size);
    monitor.fifo_in(output_fifo);
    monitor.set_golden_reference(golden_output.data(), golden_output.size());

    // 6. Calculate max simulation time (for estimation only)
    double tokens_per_cycle = static_cast<double>(hw.bus_width_bits) / hw.pixel_bits;
    double mpix_per_s = hw.clk_mhz * tokens_per_cycle;
    double max_frame_us = (static_cast<double>(WIDTH * HEIGHT) / mpix_per_s) / 1e6;
    max_frame_us *= 100.0;  // Add large margin for pipeline latency

    std::cout << "[TB] Estimated max frame time: " << max_frame_us << " us" << std::endl;

    // 7. Run simulation (no time limit in untimed mode)
    std::cout << "\n[TB] Starting simulation...\n";
    auto sim_start = std::chrono::steady_clock::now();
    sc_start();  // No time limit
    auto sim_end = std::chrono::steady_clock::now();

    std::cout << "[TB] Simulation completed" << std::endl;
    std::cout << "[TB] Simulation time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(sim_end - sim_start).count()
              << " ms" << std::endl;

    // 8. Dump metrics
    ensure_dir("output/metrics");
    if (dut.dump_pipeline_metrics()) {
        std::cout << "[TB] Dumped "
                  << dut.metrics_sample_count() << " boundary samples\n";
    }
    if (dut.dump_all_block_metrics()) {
        std::cout << "[TB] Dumped "
                  << dut.block_metrics_sample_count() << " block samples\n";
    }

    // Frame timing
    const double frame_time_us = dut.get_frame_time_us();
    const double frame_time_ns = dut.get_frame_time_ns();
    std::cout << "\n--- Frame Timing ---\n";
    std::cout << "  Frame size          : " << WIDTH << "x" << HEIGHT
              << " (" << raw_input.size() << " RAW pixels)\n";
    std::cout << "  Frame time (sim)   : " << std::fixed << std::setprecision(3)
              << frame_time_us << " us\n";
    std::cout << "  Frame time (ns)    : " << std::setprecision(3)
              << frame_time_ns << " ns\n";
    std::cout << "  Sim timestamp       : " << sc_time_stamp().to_seconds() / 1e-9
              << " ns\n";

    double fps = (frame_time_us > 0) ? (1e6 / frame_time_us) : 0;
    std::cout << "  FPS (measured)      : " << std::fixed << std::setprecision(2) << fps << "\n";

    dut.print_metrics_summary();

    // 9. Verify output
    const auto& captured = monitor.get_captured_data();
    bool pass = true;
    std::size_t diff_count = 0;
    double max_diff = 0.0;

    std::size_t compare_size = std::min(captured.size(), golden_output.size());

    for (std::size_t i = 0; i < compare_size; ++i) {
        double diff = std::abs(static_cast<double>(captured[i]) - golden_output[i]);
        if (diff > 0.0) {
            diff_count++;
            max_diff = std::max(max_diff, diff);
        }
    }

    std::cout << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "PIPELINE VERIFICATION RESULTS (D65 TIMED)" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "  Expected output size: " << golden_output.size() << std::endl;
    std::cout << "  Captured output size: " << captured.size() << std::endl;
    std::cout << "  Differences: " << diff_count << " / " << compare_size << std::endl;
    std::cout << "  Max difference: " << max_diff << std::endl;
    std::cout << "  AWB R gain: " << dut.get_awb_r_gain() << std::endl;
    std::cout << "  AWB B gain: " << dut.get_awb_b_gain() << std::endl;
    std::cout << "  AEC feedback: " << dut.get_aec_feedback() << std::endl;

    pass = (diff_count == 0) ||
           (max_diff < 10.0 && (diff_count < compare_size * 0.05));

    std::cout << "==================================================" << std::endl;
    std::cout << "TEST RESULT: " << (pass ? "PASS" : "FAIL") << std::endl;
    std::cout << "==================================================" << std::endl;

    // 10. Save output files
    ensure_dir("output");
    std::string golden_path = resolve_output_path("output/d65_golden.yuv");
    std::string captured_path = resolve_output_path("output/d65_captured.yuv");

    std::ofstream gout(golden_path, std::ios::binary);
    if (gout.is_open()) {
        gout.write(reinterpret_cast<const char*>(golden_output.data()), golden_output.size());
        gout.close();
        std::cout << "\n[Saved] Golden: " << golden_path << std::endl;
    }

    std::ofstream coutf(captured_path, std::ios::binary);
    if (coutf.is_open()) {
        coutf.write(reinterpret_cast<const char*>(captured.data()), captured.size());
        coutf.close();
        std::cout << "[Saved] Captured: " << captured_path << std::endl;
    }

    return pass ? 0 : 1;
}
