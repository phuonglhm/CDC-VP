/**
 * @file tb_d65_pipeline.cpp
 * @brief Full ISP pipeline testbench using a real D65 RAW image
 *
 * Loads `D65_raw_2688x1520_5376.raw` from `<repo>/components/isp_tlm/input/`,
 * streams it through the full SystemC `sc_isp_pipeline` (17 blocks via
 * sc_fifo), captures the YUV420 output, and:
 *   1. Saves the output to `output/d65_pipeline.yuv` (planar Y/U/V)
 *   2. Compares it to the golden reference produced by the original
 *      C++ `isp_pipeline::run` reference model.
 *
 * Usage:
 *   ./tb_d65_pipeline
 *   ./tb_d65_pipeline --metrics            # Enable metrics collection
 *   ./tb_d65_pipeline --metrics-dir <dir>  # Custom metrics output directory
 *
 * Exits with 0 on success (MSE == 0), non-zero on mismatch.
 */

#include <systemc>
using namespace sc_core;

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "sc_isp_pipeline.h"
#include "../tb_utils/tb_utils.h"
#include "../input_utils/raw_loader.h"
#include "../input_utils/tuning_loader.h"

#include "../../pipeline/include/isp_config.h"
#include "../../pipeline/include/isp_pipeline.h"
#include "../../pipeline/include/isp_regmap.h"

using namespace cdc::components;

namespace {

// Resolve a path relative to the systemc directory
// tb_d65_pipeline.cpp is at:
//   <repo>/components/isp_tlm/systemc/pipeline/d65/tb_d65_pipeline.cpp
// So systemc = __FILE__ -> d65 -> pipeline -> systemc
std::string resolve_output_path(const std::string& rel_path) {
    std::filesystem::path p(__FILE__);
    p = p.parent_path();  // d65/
    p = p.parent_path();  // pipeline/
    p = p.parent_path();  // systemc/
    return (p / rel_path).string();
}

// ---------------------------------------------------------------------------
// Resolve input/output paths relative to the repository root.
// The binary may be run from any CWD; we anchor everything at the parent
// of `components/isp_tlm/systemc/`.
// ---------------------------------------------------------------------------
// tb_d65_pipeline.cpp lives at:
//   <repo>/components/isp_tlm/systemc/pipeline/d65/tb_d65_pipeline.cpp
// parent_path:  d65/ -> pipeline/ -> systemc/ -> isp_tlm/ -> components/ -> CDC-VP/
// That's 5 hops.
const std::filesystem::path kRepoRoot =
    std::filesystem::path(__FILE__).parent_path()  // d65/
                              .parent_path()         // pipeline/
                              .parent_path()         // systemc/
                              .parent_path()         // isp_tlm/
                              .parent_path()         // components/
                              .parent_path();        // CDC-VP/

const std::filesystem::path kInputPath =
    kRepoRoot / "components" / "isp_tlm" / "input" / "D65_raw_2688x1520_5376.raw";

const std::filesystem::path kOutputDir =
    std::filesystem::path(__FILE__).parent_path().parent_path() / "output";

const std::filesystem::path kYuvOutputPath = kOutputDir / "d65_pipeline.yuv";
const std::filesystem::path kGoldenYuvPath = kOutputDir / "d65_pipeline_golden.yuv";

// -----------------------------------------------------------------------------
// NOTE: kOutW/kOutH are derived dynamically from `dut_cfg.scale.*` after
// the golden pipeline has applied its register writes (see below).
// -----------------------------------------------------------------------------

// Deep copies the captured YUV bytes from the SystemC monitor into a
// std::vector. We do this here (rather than use the monitor's golden
// comparison) because we want a side-by-side MSE vs. the *real* golden
// generated from the original C++ pipeline.
std::vector<std::uint8_t> drain_monitor(Generic_Monitor<std::uint8_t>& mon) {
    return mon.get_captured_data();
}

double compute_mse(const std::vector<std::uint8_t>& a,
                   const std::vector<std::uint8_t>& b) {
    const std::size_t n = std::min(a.size(), b.size());
    if (n == 0) return 0.0;
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double d = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        sum += d * d;
    }
    return sum / static_cast<double>(n);
}

void print_stats(const std::string& label, const std::vector<std::uint8_t>& v) {
    if (v.empty()) {
        std::cout << "[" << label << "] empty\n";
        return;
    }
    std::uint64_t sum = 0;
    std::uint8_t mn = 255, mx = 0;
    for (std::uint8_t x : v) {
        sum += x;
        if (x < mn) mn = x;
        if (x > mx) mx = x;
    }
    std::cout << "[" << label << "] n=" << v.size()
              << "  min=" << static_cast<int>(mn)
              << "  max=" << static_cast<int>(mx)
              << "  mean=" << (static_cast<double>(sum) / v.size()) << "\n";
}

} // anonymous namespace

int sc_main(int argc, char* argv[]) {
    std::cout << "==================================================\n";
    std::cout << "FULL ISP PIPELINE TEST (D65 RAW 2688x1520)\n";
    std::cout << "==================================================\n";

    // Parse command-line arguments
    bool enable_metrics = false;
    std::string metrics_dir = "output/metrics";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--metrics" || arg == "-m") {
            enable_metrics = true;
        } else if ((arg == "--metrics-dir" || arg == "-d") && i + 1 < argc) {
            metrics_dir = argv[++i];
            enable_metrics = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --metrics, -m         Enable metrics collection\n";
            std::cout << "  --metrics-dir, -d <dir>  Metrics output directory (default: output/metrics)\n";
            std::cout << "  --help, -h            Show this help message\n";
            return 0;
        }
    }

    // 1. Load the real RAW frame
    constexpr std::uint32_t W = 2688;
    constexpr std::uint32_t H = 1520;
    constexpr std::size_t   FIFO_DEPTH = 8192;  // >= 2 * W (streaming depth)

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
    std::cout << "[TB] Pixels: " << test_input.size()
              << " (expected " << (W * H) << ")\n";

    // Sample 5 input pixels for sanity log
    std::cout << "[TB] First 5 input pixels: ";
    for (std::size_t i = 0; i < 5; ++i) {
        std::cout << test_input[i] << " ";
    }
    std::cout << "\n";

    // 2. Compute golden reference using the original C++ pipeline.
    // Load the same IQ tuning file that `isp_run` consumes so the SystemC
    // testbench and the external runner produce equivalent configurations.
    isp_pipeline golden_pipeline;
    std::uint32_t dut_bayer_pattern = 0;
    isp_iq_config iq = [&]() -> isp_iq_config {
        const std::string p = tuning_loader::locate();
        if (p.empty()) {
            std::cerr << "FATAL: could not find tuning.bin (checked env "
                         "ISP_TUNING_BIN and components/isp_tlm/tests/tuning.bin).\n";
            throw std::runtime_error("tuning.bin not found");
        }
        std::cout << "[TB] Using tuning file: " << p << "\n";
        isp_iq_config cfg = tuning_loader::load_and_apply(golden_pipeline, p);
        tuning_loader::print_summary(cfg);
        // Capture Bayer pattern for the SystemC pipeline constructor.
        dut_bayer_pattern = cfg.bayer_pattern;
        return cfg;
    }();

    // After applying the tuning file, dimensions / scale config come from
    // the file. If you want a different config, edit tuning.bin (or set
    // ISP_TUNING_BIN).

    std::vector<std::uint8_t> golden_output_raw;
    {
        const isp_config pre = golden_pipeline.config();
        std::cout << "[TB] Pre-run config: scale.is_enable="
                  << (pre.scale.is_enable ? "true" : "false")
                  << "  in=" << pre.scale.in_width << "x" << pre.scale.in_height
                  << "  out=" << pre.scale.out_width << "x" << pre.scale.out_height
                  << "  yuv420.is_enable=" << (pre.yuv420.is_enable ? "true" : "false")
                  << "\n";
    }
    auto t0 = std::chrono::steady_clock::now();
    golden_pipeline.run(test_input.data(), golden_output_raw);
    auto t1 = std::chrono::steady_clock::now();

    // The C++ golden pipeline's `final_out_` is pre-allocated as
    // `W*H*3` bytes (YUV444 size). When yuv420 is enabled, it only
    // fills the first `out_w*out_h + 2*half_w*half_h` bytes, but the
    // returned vector is still the full W*H*3. We trim the trailing
    // garbage here so the comparison matches the SystemC pipeline's
    // exact YUV420 size.
    const std::uint32_t pre_w  = 2688, pre_h = 1520;
    const std::uint32_t pre_out_w = golden_pipeline.config().scale.is_enable
                                         ? golden_pipeline.config().scale.out_width
                                         : pre_w;
    const std::uint32_t pre_out_h = golden_pipeline.config().scale.is_enable
                                         ? golden_pipeline.config().scale.out_height
                                         : pre_h;
    const std::size_t expected_yuv_size =
        static_cast<std::size_t>(pre_out_w) * pre_out_h +
        2u * ((pre_out_w + 1u) / 2u) * ((pre_out_h + 1u) / 2u);
    std::vector<std::uint8_t> golden_output(
        golden_output_raw.begin(),
        golden_output_raw.begin() + std::min(expected_yuv_size, golden_output_raw.size()));

    std::cout << "[TB] Golden reference computed: " << golden_output_raw.size()
              << " raw bytes (trimmed to " << golden_output.size() << " YUV420 bytes) in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
              << " ms\n";
    print_stats("Golden YUV", golden_output);

    // Use the *post-run* config of the golden pipeline as the SystemC DUT
    // configuration. This guarantees that scale / yuv420 / in_width / in_height
    // all reflect what the C++ reference actually applied.
    const isp_config dut_cfg = golden_pipeline.config();

    std::cout << "[TB] DUT config (from golden):\n"
              << "       scale.is_enable=" << (dut_cfg.scale.is_enable ? "true" : "false")
              << "  in=" << dut_cfg.scale.in_width << "x" << dut_cfg.scale.in_height
              << "  out=" << dut_cfg.scale.out_width << "x" << dut_cfg.scale.out_height << "\n"
              << "       yuv420.is_enable=" << (dut_cfg.yuv420.is_enable ? "true" : "false") << "\n"
              << "       cse.is_enable="    << (dut_cfg.cse.is_enable ? "true" : "false")
              << "  sat_gain=" << dut_cfg.cse.saturation_gain << "\n"
              << "       bayer_pattern="    << static_cast<int>(dut_cfg.scale.in_width)
              << "  awb.is_enable=" << (dut_cfg.awb.is_enable ? "true" : "false") << "\n";

    // 3. Save golden YUV to disk (for visual inspection)
    std::error_code ec;
    std::filesystem::create_directories(kOutputDir, ec);
    if (!ec) {
        const std::uint32_t save_w = dut_cfg.scale.is_enable
                                         ? dut_cfg.scale.out_width
                                         : W;
        const std::uint32_t save_h = dut_cfg.scale.is_enable
                                         ? dut_cfg.scale.out_height
                                         : H;
        raw_loader::save_yuv420(kGoldenYuvPath.string(), golden_output, save_w, save_h);
        std::cout << "[TB] Saved golden YUV -> " << kGoldenYuvPath << "\n";
    }

    // 4. Build the SystemC streaming pipeline
    sc_core::sc_fifo<std::uint16_t> input_fifo(FIFO_DEPTH);
    sc_core::sc_fifo<std::uint8_t>  output_fifo(FIFO_DEPTH);

    Generic_Driver<std::uint16_t> driver("driver", test_input);
    driver.fifo_out(input_fifo);

    // Enable metrics if requested
    if (enable_metrics) {
        sc_isp_pipeline::set_metrics_request(true, resolve_output_path(metrics_dir), /*skip=*/nullptr);
    } else {
        // Metrics disabled - set empty path but don't clear to avoid overriding dut's defaults
        sc_isp_pipeline::set_metrics_request(false, resolve_output_path("output/metrics"), nullptr);
    }

    std::vector<float> lsc_lut(8192, 1.0f);  // not used (LSC disabled)
    cfa_types dut_bayer = cfa_types::RGGB;
    switch (dut_bayer_pattern & 0x3u) {
    case 0: dut_bayer = cfa_types::RGGB; break;
    case 1: dut_bayer = cfa_types::GRBG; break;
    case 2: dut_bayer = cfa_types::BGGR; break;
    case 3: dut_bayer = cfa_types::GBRG; break;
    }
    sc_isp_pipeline dut("isp_pipeline", dut_cfg, lsc_lut, &input_fifo, &output_fifo,
                        /* input_bit_depth = */ 16, dut_bayer);

    const std::uint32_t out_w = dut_cfg.scale.is_enable ? dut_cfg.scale.out_width  : W;
    const std::uint32_t out_h = dut_cfg.scale.is_enable ? dut_cfg.scale.out_height : H;
    const std::size_t expected_size =
        static_cast<std::size_t>(out_w) * out_h +
        2u * ((out_w + 1u) / 2u) * ((out_h + 1u) / 2u);
    Generic_Monitor<std::uint8_t> monitor("monitor", expected_size);
    monitor.fifo_in(output_fifo);

    // Pre-prime the AWB with the exact gains the reference pipeline
    // computed. The streaming AWB inside SystemC can only finish its
    // statistics after reading the whole frame, so without this priming
    // the WB would apply (1.0, 1.0) to the first frame and the colour
    // balance would be off by exactly the AWB gain factor.
    const float ref_r_gain = golden_pipeline.awb_r_gain();
    const float ref_b_gain = golden_pipeline.awb_b_gain();
    std::cout << "[TB] Priming SystemC AWB with reference gains: R="
              << ref_r_gain << " B=" << ref_b_gain << "\n";
    dut.prime_awb_gains(ref_r_gain, ref_b_gain);

    // 5. Run SystemC simulation
    std::cout << "[TB] Starting SystemC simulation...\n";
    auto s0 = std::chrono::steady_clock::now();
    sc_start();
    auto s1 = std::chrono::steady_clock::now();
    std::cout << "[TB] SystemC simulation finished in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(s1 - s0).count()
              << " ms\n";

    // Dump metrics if enabled
    if (enable_metrics) {
        std::size_t boundary_samples = dut.dump_pipeline_metrics();
        std::size_t block_samples = dut.dump_all_block_metrics();

        std::cout << "[TB] Metrics collection completed:\n";
        std::cout << "[TB]   Boundary samples: " << boundary_samples << "\n";
        std::cout << "[TB]   Block samples: " << block_samples << "\n";
        std::cout << "[TB]   Output directory: " << metrics_dir << "\n";
    }

    // 7. Capture results, save, compare
    std::vector<std::uint8_t> sc_output = drain_monitor(monitor);
    print_stats("SC YUV   ", sc_output);

    if (!ec) {
        const std::uint32_t save_w = dut_cfg.scale.is_enable
                                         ? dut_cfg.scale.out_width
                                         : W;
        const std::uint32_t save_h = dut_cfg.scale.is_enable
                                         ? dut_cfg.scale.out_height
                                         : H;
        raw_loader::save_yuv420(kYuvOutputPath.string(), sc_output, save_w, save_h);
        std::cout << "[TB] Saved SystemC YUV -> " << kYuvOutputPath << "\n";
    }

    if (sc_output.size() != golden_output.size()) {
        std::cerr << "FAIL: size mismatch sc=" << sc_output.size()
                  << " golden=" << golden_output.size() << "\n";
        return 1;
    }

    const double mse = compute_mse(sc_output, golden_output);
    std::size_t diff_count = 0;
    std::uint8_t max_err = 0;
    std::size_t max_idx = 0;
    std::int64_t sum_err = 0;
    for (std::size_t i = 0; i < sc_output.size(); ++i) {
        const int d = std::abs(static_cast<int>(sc_output[i]) -
                               static_cast<int>(golden_output[i]));
        if (d != 0) ++diff_count;
        sum_err += d;
        if (d > max_err) { max_err = static_cast<std::uint8_t>(d); max_idx = i; }
    }

    // Diagnostic histogram of per-plane sections.
    // Pipeline emits NV12 (Y plane then interleaved UV), so the UV
    // region has 2*uv_size bytes; U values are at even offsets and V
    // values at odd offsets within that region.
    const std::uint32_t _w = static_cast<std::uint32_t>(
        dut_cfg.scale.is_enable ? dut_cfg.scale.out_width : W);
    const std::uint32_t _h = static_cast<std::uint32_t>(
        dut_cfg.scale.is_enable ? dut_cfg.scale.out_height : H);
    const std::size_t y_size   = static_cast<std::size_t>(_w) * _h;
    const std::size_t uv_size  = (static_cast<std::size_t>(_w) + 1) / 2 *
                                 ((static_cast<std::size_t>(_h) + 1) / 2);
    auto section_mse = [&](std::size_t off, std::size_t stride, const char* name) {
        double s = 0;
        std::size_t n = 0;
        for (std::size_t i = 0; i + stride < uv_size * 2; i += stride, ++n) {
            const double d = static_cast<double>(sc_output[y_size + off + i]) -
                             static_cast<double>(golden_output[y_size + off + i]);
            s += d * d;
        }
        std::cout << "  MSE[" << name << "] = " << std::scientific
                  << (s / std::max<std::size_t>(n, 1)) << "\n";
    };
    // Y plane
    {
        double s = 0;
        for (std::size_t i = 0; i < y_size; ++i) {
            const double d = static_cast<double>(sc_output[i]) -
                             static_cast<double>(golden_output[i]);
            s += d * d;
        }
        std::cout << "  MSE[Y ] = " << std::scientific
                  << (s / static_cast<double>(y_size)) << "\n";
    }
    // UV plane: stride 2 starting at offset 0 = U, offset 1 = V
    section_mse(0, 2, "U ");
    section_mse(1, 2, "V ");
    std::cout << "\n==================================================\n";
    std::cout << "D65 PIPELINE VERIFICATION\n";
    std::cout << "==================================================\n";
    std::cout << "  Output size        : " << sc_output.size() << " bytes\n";
    std::cout << "  MSE                : " << std::scientific << mse << "\n";
    std::cout << "  Diff pixels        : " << diff_count
              << " / " << sc_output.size() << "\n";
    std::cout << "  Max error          : " << static_cast<int>(max_err)
              << " (at index " << max_idx << ")\n";

    const bool pass = (mse == 0.0);
    std::cout << "  Status             : " << (pass ? "PASS (bit-exact)" : "FAIL") << "\n";
    std::cout << "==================================================\n";

    return pass ? 0 : 1;
}
