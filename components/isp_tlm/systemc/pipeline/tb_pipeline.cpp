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
 *   ./tb_pipeline
 *   ./tb_pipeline --metrics <file>
 *   ./tb_pipeline --output-dir <dir>
 *   ./tb_pipeline --golden <file.yuv>
 */
#include <systemc>
using namespace sc_core;


#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <fstream>
#include <filesystem>

#include "sc_isp_pipeline.h"
#include "../tb_utils/tb_utils.h"


#include "../../pipeline/include/isp_pipeline.h"
#include "../../pipeline/include/isp_regmap.h"
#include <cstring>

#define WIDTH  32
#define HEIGHT 32
constexpr std::size_t FRAME_COUNT = 4;
#define FIFO_DEPTH 1024

namespace {


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

    // Parse command-line arguments. Output is opt-in and always caller-owned.
    bool wb_enable = true;
    std::string external_golden_path;
    std::string metrics_path;
    std::string output_dir;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--wb-off") {
            wb_enable = false;
        } else if ((arg == "--golden" || arg == "-g") && i + 1 < argc) {
            external_golden_path = argv[++i];
        } else if (arg == "--metrics" && i + 1 < argc) {
            metrics_path = argv[++i];
        } else if (arg == "--output-dir" && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --wb-off            Disable WB while AWB remains enabled\n";
            std::cout << "  --golden, -g <file> Compare with external golden YUV file\n";
            std::cout << "  --metrics <file>    Write the unified pipeline metrics report\n";
            std::cout << "  --output-dir <dir>  Save captured and golden YUV artifacts there\n";
            std::cout << "  --help, -h          Show this help message\n";
            return 0;
        } else {
            std::cerr << "[ERROR] Unknown or incomplete option: " << arg << '\n';
            return 2;
        }
    }

    // 2. Generate test input
    const std::size_t raw_pixels = WIDTH * HEIGHT;
    const std::vector<std::uint16_t> base_input =
        tb_utils::generate_test_pattern<std::uint16_t>(WIDTH, HEIGHT, 1, 2048);
    std::vector<std::uint16_t> test_input(raw_pixels * FRAME_COUNT);
    std::copy(base_input.begin(), base_input.end(), test_input.begin());
    for (std::size_t frame = 1; frame < FRAME_COUNT; ++frame) {
        for (std::size_t row = 0; row < HEIGHT; ++row) {
            for (std::size_t col = 0; col < WIDTH; ++col) {
                const bool even_row = (row & 1u) == 0u;
                const bool even_col = (col & 1u) == 0u;
                std::uint16_t value = 1700;
                if (even_row && even_col) {
                    value = 3000;  // RGGB red
                } else if (!even_row && !even_col) {
                    value = 600;   // RGGB blue
                }
                test_input[frame * raw_pixels + row * WIDTH + col] = value;
            }
        }
    }

    // Configure the oracle through the same register interface as the DUT.
    // isp_pipeline exposes config() read-only, so mutating a copied config
    // would leave the oracle at its defaults and produce a vacuous golden.
    isp_pipeline golden_pipeline;
    auto write_to_reg = [&](std::uint32_t offset, std::uint32_t value) {
        golden_pipeline.write_reg(offset, value);
    };
    auto float_to_reg = [](float value) {
        std::uint32_t raw = 0;
        std::memcpy(&raw, &value, sizeof(raw));
        return raw;
    };

    using namespace cdc::components;
    write_to_reg(REG_CTRL, 0);
    write_to_reg(REG_WIDTH, WIDTH);
    write_to_reg(REG_HEIGHT, HEIGHT);
    write_to_reg(REG_BIT_DEPTH, 12);
    write_to_reg(REG_BAYER_PATTERN, 0);  // RGGB

    write_to_reg(REG_BLC_ENABLE, 1);
    write_to_reg(REG_BLC_R_OFFSET, 256);
    write_to_reg(REG_BLC_GR_OFFSET, 256);
    write_to_reg(REG_BLC_GB_OFFSET, 256);
    write_to_reg(REG_BLC_B_OFFSET, 256);
    write_to_reg(REG_BLC_R_SAT, 4095);
    write_to_reg(REG_BLC_GR_SAT, 4095);
    write_to_reg(REG_BLC_GB_SAT, 4095);
    write_to_reg(REG_BLC_B_SAT, 4095);
    write_to_reg(REG_DPC_ENABLE, 1);
    write_to_reg(REG_DPC_THRESH, 30);
    write_to_reg(REG_LSC_ENABLE, 0);
    write_to_reg(REG_DG_ENABLE, 1);
    write_to_reg(REG_DG_AUTO, 1);
    write_to_reg(REG_BNR_ENABLE, 0);
    write_to_reg(REG_DEMOSAIC_ENABLE, 1);
    write_to_reg(REG_AWB_ENABLE, 1);
    write_to_reg(REG_AWB_UNDER_PCT, float_to_reg(0.01f));
    write_to_reg(REG_AWB_OVER_PCT, float_to_reg(0.01f));
    write_to_reg(REG_WB_ENABLE, wb_enable ? 1u : 0u);
    write_to_reg(REG_WB_R_GAIN, float_to_reg(1.25f));
    write_to_reg(REG_WB_B_GAIN, float_to_reg(0.75f));
    write_to_reg(REG_CCM_ENABLE, 1);
    write_to_reg(REG_CCM_MATRIX00, float_to_reg(1.0f));
    write_to_reg(REG_CCM_MATRIX01, float_to_reg(0.0f));
    write_to_reg(REG_CCM_MATRIX02, float_to_reg(0.0f));
    write_to_reg(REG_CCM_MATRIX10, float_to_reg(0.0f));
    write_to_reg(REG_CCM_MATRIX11, float_to_reg(1.0f));
    write_to_reg(REG_CCM_MATRIX12, float_to_reg(0.0f));
    write_to_reg(REG_CCM_MATRIX20, float_to_reg(0.0f));
    write_to_reg(REG_CCM_MATRIX21, float_to_reg(0.0f));
    write_to_reg(REG_CCM_MATRIX22, float_to_reg(1.0f));
    write_to_reg(REG_GC_ENABLE, 1);
    write_to_reg(REG_AEC_ENABLE, 1);
    write_to_reg(REG_CSC_STANDARD, 0);
    write_to_reg(REG_CSE_ENABLE, 1);
    write_to_reg(REG_CSE_SAT_GAIN, float_to_reg(1.0f));
    write_to_reg(REG_SHARPEN_ENABLE, 1);
    write_to_reg(REG_SHARPEN_SIGMA, 1);
    write_to_reg(REG_SHARPEN_STRENGTH, 1);
    write_to_reg(REG_2DNR_ENABLE, 0);
    write_to_reg(REG_SCALE_ENABLE, 1);
    write_to_reg(REG_SCALE_OUT_W, WIDTH / 2);
    write_to_reg(REG_SCALE_OUT_H, HEIGHT / 2);
    write_to_reg(REG_YUV420_ENABLE, 1);

    // Read back only after programming so the DUT and oracle share one config.
    isp_config cfg = golden_pipeline.config();
    std::cout << "[TB] Oracle config: scale=" << cfg.scale.is_enable
              << " " << cfg.scale.out_width << "x" << cfg.scale.out_height
              << ", yuv420=" << cfg.yuv420.is_enable << std::endl;
    const std::size_t golden_expected_size = (WIDTH / 2) * (HEIGHT / 2) +
                                             2 * (((WIDTH / 2) + 1) / 2) *
                                                 (((HEIGHT / 2) + 1) / 2);
    std::vector<std::uint8_t> golden_output;
    golden_output.reserve(golden_expected_size * FRAME_COUNT);
    for (std::size_t frame = 0; frame < FRAME_COUNT; ++frame) {
        std::vector<std::uint8_t> frame_output;
        golden_pipeline.run(test_input.data() + frame * raw_pixels, frame_output);
        if (frame_output.size() < golden_expected_size) {
            std::cerr << "[TB] ERROR: oracle frame " << frame << " produced "
                      << frame_output.size() << " bytes; expected at least "
                      << golden_expected_size << " bytes for YUV420 output"
                      << std::endl;
            return 1;
        }
        frame_output.resize(golden_expected_size);
        golden_output.insert(golden_output.end(), frame_output.begin(),
                             frame_output.end());
    }
    std::cout << "[TB] Golden output size: " << golden_output.size()
              << " bytes (" << golden_expected_size << " bytes/frame, "
              << FRAME_COUNT << " frames)" << std::endl;
    if (golden_output.empty()) {
        std::cerr << "[TB] ERROR: oracle produced an empty YUV420 frame"
                  << std::endl;
        return 1;
    }

    // 4. Create SystemC pipeline
    std::vector<float> lsc_lut(8192, 1.0f);

    sc_core::sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint8_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint16_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    sc_isp_pipeline dut("isp_pipeline", cfg, lsc_lut,
                        &input_fifo, &output_fifo,
                        /*input_bit_depth=*/12,
                        /*bayer_pattern=*/cfa_types::RGGB);

    std::size_t out_w = cfg.scale.is_enable ? cfg.scale.out_width : WIDTH;
    std::size_t out_h = cfg.scale.is_enable ? cfg.scale.out_height : HEIGHT;
    std::size_t expected_size = out_w * out_h +
                                2 * ((out_w + 1) / 2) * ((out_h + 1) / 2);

    Generic_Monitor<std::uint8_t> monitor("monitor", expected_size * FRAME_COUNT);
    monitor.fifo_in(output_fifo);
    monitor.set_golden_reference(golden_output.data(), golden_output.size());

    // 5. Run simulation
    std::cout << "[TB] Starting simulation..." << std::endl;
    sc_start();
    std::cout << "[TB] Simulation completed" << std::endl;

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

    // Verify output
    const auto& captured = monitor.get_captured_data();
    bool pass = captured.size() == golden_output.size();
    std::size_t diff_count = 0;
    double max_diff = 0.0;

    const std::size_t compare_size = std::min(captured.size(), golden_output.size());
    for (std::size_t i = 0; i < compare_size; ++i) {
        const double diff = std::abs(static_cast<double>(captured[i]) - golden_output[i]);
        if (diff > 0.0) {
            ++diff_count;
            max_diff = std::max(max_diff, diff);
        }
    }
    if (captured.size() != golden_output.size()) {
        std::cerr << "[TB] ERROR: captured output size " << captured.size()
                  << " differs from oracle size " << golden_output.size() << std::endl;
    }

    std::cout << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "PIPELINE VERIFICATION RESULTS" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "  Expected output size: " << golden_output.size() << std::endl;
    std::cout << "  Captured output size: " << captured.size() << std::endl;
    std::cout << "  Differences: " << diff_count << " / " << golden_output.size() << std::endl;
    std::cout << "  Max difference: " << max_diff << std::endl;

    pass = pass && diff_count == 0;

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
            const std::string golden_path =
                (directory / ("golden_" + std::to_string(WIDTH) + "x" +
                              std::to_string(HEIGHT) + ".yuv")).string();
            const std::string captured_path =
                (directory / ("captured_" + std::to_string(out_w) + "x" +
                              std::to_string(out_h) + ".yuv")).string();
            pass = save_yuv_file(golden_path, golden_output.data(),
                                 golden_output.size()) && pass;
            pass = save_yuv_file(captured_path, captured.data(),
                                 captured.size()) && pass;
        }
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
