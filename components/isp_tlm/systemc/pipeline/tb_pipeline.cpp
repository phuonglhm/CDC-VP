/**
 *
 * @file tb_pipeline.cpp
 *
 * @brief Full ISP Pipeline Testbench
 *
 *
 * End-to-end testbench that validates the entire 17-block ISP pipeline
 *
 * by comparing SystemC streaming output against the original C++ reference model.
 *
 * Also collects comprehensive metrics: block processing latency, boundary
 * throughput, and frame processing time.
 *
 * Usage:
 *   ./tb_pipeline                          # Run with golden reference comparison
 *   ./tb_pipeline --save-output           # Save both golden and captured output
 *   ./tb_pipeline --golden <file.yuv>     # Compare with external golden file
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

#include "../../pipeline/include/isp_config.h"
#include "../../pipeline/include/isp_pipeline.h"

#define WIDTH  32
#define HEIGHT 32
#define FIFO_DEPTH 1024

namespace {

// Get the absolute path to the systemc directory
// Based on __FILE__ location: .../CDC-VP/components/isp_tlm/systemc/pipeline/tb_pipeline.cpp
std::string get_systemc_dir() {
    std::filesystem::path p(__FILE__);
    // Navigate: tb_pipeline.cpp -> pipeline -> systemc -> isp_tlm -> components -> repo root
    p = p.parent_path();      // pipeline/
    p = p.parent_path();      // systemc/
    return p.string();
}

// Resolve output path relative to systemc directory
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

bool save_yuv_file(const std::string& path, const std::uint8_t* data, std::size_t size) {
    std::ofstream fout(path, std::ios::binary);
    if (!fout.is_open()) {
        std::cerr << "[ERROR] Cannot save YUV to: " << path << std::endl;
        return false;
    }
    fout.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    fout.close();
    std::cout << "[INFO] Saved YUV: " << path << " (" << size << " bytes)" << std::endl;
    return true;
}

bool load_yuv_file(const std::string& path, std::vector<std::uint8_t>& data) {
    std::ifstream fin(path, std::ios::binary | std::ios::ate);
    if (!fin.is_open()) {
        std::cerr << "[ERROR] Cannot load YUV from: " << path << std::endl;
        return false;
    }
    std::streamsize size = fin.tellg();
    fin.seekg(0, std::ios::beg);
    data.resize(static_cast<std::size_t>(size));
    if (!fin.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cerr << "[ERROR] Failed to read YUV file: " << path << std::endl;
        return false;
    }
    std::cout << "[INFO] Loaded YUV: " << path << " (" << size << " bytes)" << std::endl;
    return true;
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
    std::cout << "FULL ISP PIPELINE TESTBENCH" << std::endl;
    std::cout << "==================================================" << std::endl;

    // Parse command-line arguments
    bool save_output = false;
    std::string external_golden_path;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--save-output" || arg == "-s") {
            save_output = true;
        } else if ((arg == "--golden" || arg == "-g") && i + 1 < argc) {
            external_golden_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --save-output, -s   Save captured and golden output to output/ directory\n";
            std::cout << "  --golden, -g <file> Compare with external golden YUV file\n";
            std::cout << "  --help, -h          Show this help message\n";
            return 0;
        }
    }

    // 1. Hardware parameters
    hw_params hw;
    hw.clk_mhz = 200.0f;
    hw.bus_width_bits = 64;
    hw.pixel_bits = 16;
    hw.fifo_depth = FIFO_DEPTH;
    hw.timed_mode = false;  // Untimed mode - no clock needed

    std::cout << "\n--- Hardware Parameters ---\n";
    std::cout << hw.to_string();

    // 2. Generate test input
    const std::size_t raw_pixels = WIDTH * HEIGHT;
    std::vector<std::uint16_t> test_input = tb_utils::generate_test_pattern<std::uint16_t>(
        WIDTH, HEIGHT, 1, 2048);

    std::cout << "[TB] Test input: " << raw_pixels << " pixels" << std::endl;

    // 3. Compute golden reference using original C++ pipeline
    isp_pipeline golden_pipeline;
    isp_config cfg = golden_pipeline.config();

    cfg.scale.in_width = WIDTH;
    cfg.scale.in_height = HEIGHT;
    cfg.scale.out_width = WIDTH / 2;
    cfg.scale.out_height = HEIGHT / 2;

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
    cfg.scale.is_enable = true;
    cfg.yuv420.is_enable = true;

    cfg.blc.r_offset = 256; cfg.blc.gr_offset = 256;
    cfg.blc.gb_offset = 256; cfg.blc.b_offset = 256;
    cfg.blc.r_sat = 4095; cfg.blc.gr_sat = 4095;
    cfg.blc.gb_sat = 4095; cfg.blc.b_sat = 4095;

    cfg.dpc.dp_threshold = 30;

    cfg.wb.r_gain = 1.0f;
    cfg.wb.b_gain = 1.0f;

    cfg.ccm.bit_depth = 12;
    cfg.ccm.corrected_red[0] = 1.0f; cfg.ccm.corrected_red[1] = 0.0f; cfg.ccm.corrected_red[2] = 0.0f;
    cfg.ccm.corrected_green[0] = 0.0f; cfg.ccm.corrected_green[1] = 1.0f; cfg.ccm.corrected_green[2] = 0.0f;
    cfg.ccm.corrected_blue[0] = 0.0f; cfg.ccm.corrected_blue[1] = 0.0f; cfg.ccm.corrected_blue[2] = 1.0f;

    cfg.gc.bit_depth = 12;
    cfg.cse.saturation_gain = 1.0f;
    cfg.sharpen.sharpen_sigma = 1;
    cfg.sharpen.sharpen_strength = 1;

    cfg.awb.underexposed_percentage = 0.01f;
    cfg.awb.overexposed_percentage = 0.01f;

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
    golden_pipeline.run(test_input.data(), golden_output);

    std::cout << "[TB] Golden output size: " << golden_output.size() << " bytes" << std::endl;

    // 4. Create SystemC pipeline
    std::vector<float> lsc_lut(8192, 1.0f);

    sc_core::sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint8_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint16_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    // Enable metrics - use absolute path relative to executable
    sc_isp_pipeline::set_metrics_request(true, resolve_output_path("output/metrics"), /*skip=*/nullptr);

    sc_isp_pipeline dut("isp_pipeline", cfg, lsc_lut,
                        &input_fifo, &output_fifo,
                        /*input_bit_depth=*/12,
                        /*bayer_pattern=*/cfa_types::RGGB,
                        &hw);
    sc_isp_pipeline::clear_metrics_request();

    std::size_t out_w = cfg.scale.is_enable ? cfg.scale.out_width : WIDTH;
    std::size_t out_h = cfg.scale.is_enable ? cfg.scale.out_height : HEIGHT;
    std::size_t expected_size = out_w * out_h +
                                2 * ((out_w + 1) / 2) * ((out_h + 1) / 2);

    Generic_Monitor<std::uint8_t> monitor("monitor", expected_size);
    monitor.fifo_in(output_fifo);
    monitor.set_golden_reference(golden_output.data(), golden_output.size());

    // 5. Run simulation
    std::cout << "[TB] Starting simulation..." << std::endl;
    sc_start();
    std::cout << "[TB] Simulation completed" << std::endl;

    // Metrics dump
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
              << " (" << raw_pixels << " RAW pixels)\n";
    std::cout << "  Frame time (sim)   : " << std::fixed << std::setprecision(3)
              << frame_time_us << " us\n";
    std::cout << "  Frame time (ns)    : " << std::setprecision(3)
              << frame_time_ns << " ns\n";
    std::cout << "  Sim timestamp       : " << sc_time_stamp().to_double() / 1e-9
              << " ns\n";

    dut.print_metrics_summary();

    // Verify output
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
    std::cout << "PIPELINE VERIFICATION RESULTS" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "  Expected output size: " << golden_output.size() << std::endl;
    std::cout << "  Captured output size: " << captured.size() << std::endl;
    std::cout << "  Differences: " << diff_count << " / " << compare_size << std::endl;
    std::cout << "  Max difference: " << max_diff << std::endl;
    std::cout << "  AWB R gain: " << dut.get_awb_r_gain() << std::endl;
    std::cout << "  AWB B gain: " << dut.get_awb_b_gain() << std::endl;
    std::cout << "  AEC feedback: " << dut.get_aec_feedback() << std::endl;

    pass = (diff_count == 0) ||
           (max_diff < 5.0 && (diff_count < compare_size * 0.05));

    std::cout << "==================================================" << std::endl;
    std::cout << "TEST RESULT: " << (pass ? "PASS" : "FAIL") << std::endl;
    std::cout << "==================================================" << std::endl;

    // Save output files if requested
    if (save_output) {
        ensure_dir("output");
        std::string golden_path = "output/golden_" + std::to_string(WIDTH) + "x" + std::to_string(HEIGHT) + ".yuv";
        std::string captured_path = "output/captured_" + std::to_string(out_w) + "x" + std::to_string(out_h) + ".yuv";

        save_yuv_file(golden_path, golden_output.data(), golden_output.size());
        save_yuv_file(captured_path, captured.data(), captured.size());
    }

    // Compare with external golden file if provided
    if (!external_golden_path.empty()) {
        std::vector<std::uint8_t> external_golden;
        if (load_yuv_file(external_golden_path, external_golden)) {
            std::cout << "\n--- External Golden Comparison ---\n";
            std::size_t compare_ext = std::min(captured.size(), external_golden.size());
            double mse = compute_mse(captured.data(), external_golden.data(), compare_ext);
            double max_diff_ext = compute_max_diff(captured.data(), external_golden.data(), compare_ext);

            std::cout << "  Golden file: " << external_golden_path << std::endl;
            std::cout << "  Golden size: " << external_golden.size() << " bytes\n";
            std::cout << "  Captured size: " << captured.size() << " bytes\n";
            std::cout << "  Compare size: " << compare_ext << " bytes\n";
            std::cout << "  MSE: " << std::scientific << std::setprecision(6) << mse << std::endl;
            std::cout << "  Max diff: " << std::fixed << std::setprecision(2) << max_diff_ext << std::endl;

            bool ext_pass = (mse < 1.0);
            std::cout << "  Status: " << (ext_pass ? "PASS" : "FAIL") << std::endl;
        }
    }

    return pass ? 0 : 1;
}
