/**
 * @file tb_d65_blc.cpp
 * @brief Testbench for sc_blc driven by the real D65 RAW image
 *
 * Loads the 2688x1520 D65 RAW, pushes every pixel through the SystemC
 * `sc_blc` module, captures the BLC-corrected frame, and:
 *   1. Saves the output to `output/d65_blc_out.pgm` (12-bit PGM)
 *   2. Compares bit-exact to the golden computed by the original
 *      `blc_block::process`.
 *
 * Usage:
 *   ./tb_d65_blc
 *
 * Exits 0 on bit-exact match.
 */

#include <systemc>
using namespace sc_core;

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <vector>

#include "sc_blc.h"
#include "../../tb_utils/tb_utils.h"
#include "../../input_utils/raw_loader.h"

#include "../../blc/include/blc.h"

namespace {

// tb_d65_blc.cpp lives at:
//   <repo>/components/isp_tlm/systemc/blocks/blc/d65/tb_d65_blc.cpp
// parent_path:  d65/ -> blc/ -> blocks/ -> systemc/ -> isp_tlm/ -> components/ -> CDC-VP/
// That's 6 hops.
const std::filesystem::path kRepoRoot =
    std::filesystem::path(__FILE__).parent_path()  // d65/
                              .parent_path()         // blc/
                              .parent_path()         // blocks/
                              .parent_path()         // systemc/
                              .parent_path()         // isp_tlm/
                              .parent_path()         // components/
                              .parent_path();        // CDC-VP/

const std::filesystem::path kInputPath =
    kRepoRoot / "components" / "isp_tlm" / "input" / "D65_raw_2688x1520_5376.raw";

const std::filesystem::path kOutputDir =
    std::filesystem::path(__FILE__).parent_path().parent_path() / "output";

const std::filesystem::path kPgmOutPath = kOutputDir / "d65_blc_out.pgm";

} // namespace

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================\n";
    std::cout << "BLC TEST ON D65 RAW 2688x1520\n";
    std::cout << "==================================================\n";

    constexpr std::uint32_t W = 2688;
    constexpr std::uint32_t H = 1520;
    constexpr std::size_t   FIFO_DEPTH = 8192;

    // 1. Load real RAW
    std::vector<std::uint16_t> test_input;
    raw_loader::RawFormat fmt = raw_loader::RawFormat::UNKNOWN;
    try {
        raw_loader::load(kInputPath.string(), W, H, test_input, &fmt);
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << "\n";
        return 2;
    }
    std::cout << "[TB] Loaded " << kInputPath.filename() << " ("
              << raw_loader::format_name(fmt) << ")\n";
    std::cout << "[TB] First 5 input pixels: ";
    for (std::size_t i = 0; i < 5; ++i) {
        std::cout << test_input[i] << " ";
    }
    std::cout << "\n";

    // 2. Compute golden reference
    blc_config cfg;
    cfg.is_enable = true;
    cfg.is_linear = true;
    cfg.r_offset  = 256;
    cfg.gr_offset = 256;
    cfg.gb_offset = 256;
    cfg.b_offset  = 256;
    cfg.r_sat     = 4095;
    cfg.gr_sat    = 4095;
    cfg.gb_sat    = 4095;
    cfg.b_sat     = 4095;

    std::vector<std::uint16_t> golden_output(test_input.size());
    blc_block golden_block;
    auto t0 = std::chrono::steady_clock::now();
    golden_block.process(test_input.data(), golden_output.data(),
                         W, H, cfg, cfa_types::RGGB, 12);
    auto t1 = std::chrono::steady_clock::now();
    std::cout << "[TB] Golden reference computed in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
              << " ms\n";

    // 3. SystemC streaming
    sc_core::sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint16_t> output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint16_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    sc_blc dut("blc_dut", cfg, cfa_types::RGGB, 12);
    dut.fifo_in(input_fifo);
    dut.fifo_out(output_fifo);

    Generic_Monitor<std::uint16_t> monitor("monitor", test_input.size());
    monitor.fifo_in(output_fifo);

    std::cout << "[TB] Starting SystemC simulation...\n";
    auto s0 = std::chrono::steady_clock::now();
    sc_start();
    auto s1 = std::chrono::steady_clock::now();
    std::cout << "[TB] SystemC simulation finished in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(s1 - s0).count()
              << " ms\n";

    // 4. Save output PGM (12-bit)
    std::error_code ec;
    std::filesystem::create_directories(kOutputDir, ec);
    if (!ec) {
        raw_loader::save_pgm(kPgmOutPath.string(),
                             monitor.get_captured_data(), W, H, 4095);
        std::cout << "[TB] Saved BLC output -> " << kPgmOutPath << "\n";
    }

    // 5. Bit-exact comparison
    const auto& captured = monitor.get_captured_data();
    if (captured.size() != golden_output.size()) {
        std::cerr << "FAIL: size mismatch sc=" << captured.size()
                  << " golden=" << golden_output.size() << "\n";
        return 1;
    }

    std::size_t diff_count = 0;
    std::uint16_t max_err = 0;
    std::size_t max_idx = 0;
    for (std::size_t i = 0; i < captured.size(); ++i) {
        const int d = std::abs(static_cast<int>(captured[i]) -
                               static_cast<int>(golden_output[i]));
        if (d != 0) ++diff_count;
        if (d > max_err) { max_err = static_cast<std::uint16_t>(d); max_idx = i; }
    }

    double mse = 0.0;
    for (std::size_t i = 0; i < captured.size(); ++i) {
        const double d = static_cast<double>(captured[i]) -
                         static_cast<double>(golden_output[i]);
        mse += d * d;
    }
    mse /= static_cast<double>(captured.size());

    std::cout << "\n==================================================\n";
    std::cout << "D65 BLC VERIFICATION\n";
    std::cout << "==================================================\n";
    std::cout << "  Pixels            : " << captured.size() << "\n";
    std::cout << "  MSE               : " << std::scientific << mse << "\n";
    std::cout << "  Diff pixels       : " << diff_count << "\n";
    std::cout << "  Max error         : " << max_err << " (at index " << max_idx << ")\n";
    const bool pass = (mse == 0.0);
    std::cout << "  Status            : " << (pass ? "PASS (bit-exact)" : "FAIL") << "\n";
    std::cout << "==================================================\n";

    return pass ? 0 : 1;
}
