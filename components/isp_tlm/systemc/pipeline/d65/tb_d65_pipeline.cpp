/**
 * @file tb_d65_pipeline.cpp
 * @brief Full ISP Pipeline Testbench with Real D65 Image
 *
 * This testbench:
 * 1. Loads real D65 RAW image (2688x1520)
 * 2. Runs the functional line pipeline
 * 3. Compares against the C++ reference
 *
 * Usage:
 *   ./tb_d65_pipeline
 *   ./tb_d65_pipeline --metrics <file>
 *   ./tb_d65_pipeline --output-dir <dir>
 *   ./tb_d65_pipeline --input <file.raw>
 */

#include <systemc>
using namespace sc_core;

#include <iostream>
#include <vector>
#include <cstdlib>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <filesystem>

#include "sc_isp_pipeline.h"
#include "../tb_utils/tb_utils.h"
#include "../input_utils/raw_loader.h"

#include "../../pipeline/include/isp_config.h"
#include "../../pipeline/include/isp_pipeline.h"
namespace {

constexpr std::uint32_t WIDTH = 2688;
constexpr std::uint32_t HEIGHT = 1520;
constexpr std::uint32_t FIFO_DEPTH = WIDTH * HEIGHT / 4;  // Quarter frame for buffering


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
    std::cout << "D65 REAL IMAGE PIPELINE TESTBENCH" << std::endl;
    std::cout << "==================================================" << std::endl;

    std::string raw_path = "../input/D65_raw_2688x1520_5376.raw";
    std::string metrics_path;
    std::string output_dir;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--input" && index + 1 < argc) {
            raw_path = argv[++index];
        } else if (argument == "--metrics" && index + 1 < argc) {
            metrics_path = argv[++index];
        } else if (argument == "--output-dir" && index + 1 < argc) {
            output_dir = argv[++index];
        } else if (argument == "--help" || argument == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "  --metrics <file>    Write the unified pipeline metrics report\n"
                      << "  --input <file>      Read the D65 RAW image from this path\n"
                      << "  --output-dir <dir>  Save D65 golden and captured YUV artifacts there\n";
            return 0;
        } else {
            std::cerr << "[ERROR] Unknown or incomplete option: "
                      << argument << '\n';
            return 2;
        }
    }

    // 1. Load the caller-selected real D65 RAW image.
    std::cout << "\n--- Loading D65 RAW Image ---\n";
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

    // 4. Create the functional SystemC pipeline.
    std::cout << "\n--- Creating SystemC Pipeline ---\n";
    std::vector<float> lsc_lut(8192, 1.0f);

    sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_fifo<std::uint8_t> output_fifo(FIFO_DEPTH * 2);

    sc_isp_pipeline dut("isp_pipeline", cfg, lsc_lut,
                        &input_fifo, &output_fifo,
                        /*input_bit_depth=*/16,
                        /*bayer_pattern=*/cfa_types::RGGB);

    // 5. Create driver and monitor
    Generic_Driver<std::uint16_t> driver("driver", raw_input);
    driver.fifo_out(input_fifo);

    std::size_t out_w = cfg.scale.is_enable ? cfg.scale.out_width : WIDTH;
    std::size_t out_h = cfg.scale.is_enable ? cfg.scale.out_height : HEIGHT;
    std::size_t expected_size = out_w * out_h +
                                2 * ((out_w + 1) / 2) * ((out_h + 1) / 2);

    Generic_Monitor<std::uint8_t> monitor("monitor", expected_size);
    monitor.fifo_in(output_fifo);
    monitor.set_golden_reference(golden_output.data(), golden_output.size());

    // 6. Run simulation.
    std::cout << "\n[TB] Starting simulation...\n";
    const auto sim_start = std::chrono::steady_clock::now();
    sc_start();
    const auto sim_end = std::chrono::steady_clock::now();

    std::cout << "[TB] Simulation completed" << std::endl;
    std::cout << "[TB] Simulation time: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                     sim_end - sim_start).count()
              << " ms" << std::endl;

    if (!metrics_path.empty()) {
        if (!dut.write_metrics(metrics_path)) {
            std::cerr << "[TB] ERROR: failed to write metrics: "
                      << metrics_path << '\n';
            return 1;
        }
        const auto snapshot = dut.metrics();
        const auto& frame = snapshot.frame;
        std::cout << "\n--- Frame Metrics ---\n"
                  << "  Frame cycles       : " << frame.frame_cycles
                  << " (" << (frame.frame_cycles_available ? "available" : "unavailable")
                  << ")\n"
                  << "  Input bandwidth    : " << frame.input_bandwidth_mbps
                  << " Mbps\n"
                  << "  Output bandwidth   : " << frame.output_bandwidth_mbps
                  << " Mbps\n";
    }


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
    std::cout << "PIPELINE VERIFICATION RESULTS (LINE MODEL)" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "  Expected output size: " << golden_output.size() << std::endl;
    std::cout << "  Captured output size: " << captured.size() << std::endl;
    std::cout << "  Differences: " << diff_count << " / " << compare_size << std::endl;
    std::cout << "  Max difference: " << max_diff << std::endl;

    pass = captured.size() == golden_output.size() && diff_count == 0;

    std::cout << "==================================================" << std::endl;
    std::cout << "TEST RESULT: " << (pass ? "PASS" : "FAIL") << std::endl;
    std::cout << "==================================================" << std::endl;

    if (!output_dir.empty()) {
        std::error_code error;
        std::filesystem::create_directories(output_dir, error);
        if (error) {
            std::cerr << "[TB] ERROR: cannot create output directory "
                      << output_dir << ": " << error.message() << '\n';
            pass = false;
        } else {
            const std::filesystem::path directory(output_dir);
            const std::filesystem::path golden_path =
                directory / "d65_golden.yuv";
            const std::filesystem::path captured_path =
                directory / "d65_captured.yuv";

            std::ofstream gout(golden_path, std::ios::binary);
            std::ofstream coutf(captured_path, std::ios::binary);
            if (!gout || !coutf) {
                std::cerr << "[TB] ERROR: cannot save D65 YUV artifacts in "
                          << output_dir << '\n';
                pass = false;
            } else {
                gout.write(reinterpret_cast<const char*>(golden_output.data()),
                           static_cast<std::streamsize>(golden_output.size()));
                coutf.write(reinterpret_cast<const char*>(captured.data()),
                            static_cast<std::streamsize>(captured.size()));
                std::cout << "\n[Saved] Golden: " << golden_path << std::endl;
                std::cout << "[Saved] Captured: " << captured_path << std::endl;
            }
        }
    }

    return pass ? 0 : 1;
}
