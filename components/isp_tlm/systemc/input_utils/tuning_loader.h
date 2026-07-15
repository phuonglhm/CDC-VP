#pragma once

// =============================================================================
// tuning_loader.h
//
// Helper for loading an ISP IQ configuration from the on-disk binary format
// produced by `components/isp_tlm/tests/generate_iq.py` (magic word 0x49535021,
// "ISP!"). The on-disk layout is the packed C struct `isp_iq_config` defined
// in `components/isp_tlm/pipeline/include/isp_config.h`.
//
// The same tuning file is consumed by `components/isp_tlm/tests/isp_run.cpp`
// (the external `isp_run` runner) so loading it here keeps the SystemC test
// in lockstep with the production flow.
//
// Two entry points:
//   1. `load_iq_config(path)` -> returns the raw `isp_iq_config` struct
//      populated from the file. Useful if you need to inspect fields
//      before applying them.
//
//   2. `apply_iq_config(pipeline, iq)` -> pushes every field of `iq` into
//      the given `isp_pipeline` via its `write_reg` interface. This is the
//      recommended way to make the SystemC test behave identically to
//      `isp_run` with the same `-c tuning.bin` flag.
//
// Both functions throw `std::runtime_error` on I/O failure or magic-word
// mismatch.
// =============================================================================

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdio>
#include <cstring>

#include "../../pipeline/include/isp_config.h"
#include "../../pipeline/include/isp_pipeline.h"
#include "../../pipeline/include/isp_regmap.h"

namespace tuning_loader {

constexpr std::uint32_t kIspIqMagic = 0x49535021u;  // "ISP!" little-endian
using namespace cdc::components;  // pull isp_iq_config / isp_pipeline / REG_* into scope

// Read the 232-byte `isp_iq_config` blob from `path` and return the struct.
// Throws on I/O failure or invalid magic word.
inline isp_iq_config load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        throw std::runtime_error("tuning_loader: cannot open '" + path + "'");
    }

    isp_iq_config iq{};
    f.read(reinterpret_cast<char*>(&iq), sizeof(iq));
    if (f.gcount() != static_cast<std::streamsize>(sizeof(iq))) {
        throw std::runtime_error(
            "tuning_loader: short read on '" + path + "' "
            "(got " + std::to_string(f.gcount()) +
            " bytes, expected " + std::to_string(sizeof(iq)) + ")");
    }

    if (iq.magic_word != kIspIqMagic) {
        throw std::runtime_error(
            "tuning_loader: invalid magic word in '" + path + "' (got 0x" +
            [&]() {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%08x", iq.magic_word);
                return std::string(buf);
            }() + ", expected 0x49535021)");
    }

    return iq;
}

// Convenience: convert a 32-bit register-encoded float (IEEE-754 bits) into a
// `uint32_t` we can hand to `write_reg`.
inline std::uint32_t float_to_reg(float value) {
    std::uint32_t raw = 0;
    std::memcpy(&raw, &value, sizeof(raw));
    return raw;
}

// Push every field of `iq` into the given `isp_pipeline` via `write_reg`.
// The flow follows `tests/isp_run.cpp` so both tools produce the same
// internal state (and therefore the same output) when given the same
// tuning file and same RAW frame.
//
// IMPORTANT: this also sets REG_WIDTH / REG_HEIGHT / REG_BIT_DEPTH /
// REG_BAYER_PATTERN, so it is the caller's responsibility to apply IQ
// BEFORE allocating any output buffers that depend on dimensions.
inline void apply(isp_pipeline& pipeline, const isp_iq_config& iq) {
    // Global params
    pipeline.write_reg(REG_WIDTH,         iq.width);
    pipeline.write_reg(REG_HEIGHT,        iq.height);
    pipeline.write_reg(REG_STRIDE,        iq.stride);
    pipeline.write_reg(REG_FORMAT,        iq.format);
    pipeline.write_reg(REG_OP_MODE,       iq.op_mode);
    pipeline.write_reg(REG_BIT_DEPTH,     iq.bit_depth);
    pipeline.write_reg(REG_BAYER_PATTERN, iq.bayer_pattern);

    // BLC
    pipeline.write_reg(REG_BLC_ENABLE,    iq.blc_enable);
    pipeline.write_reg(REG_BLC_LINEAR,    iq.blc_linear);
    pipeline.write_reg(REG_BLC_R_OFFSET,  iq.blc_r_offset);
    pipeline.write_reg(REG_BLC_GR_OFFSET, iq.blc_gr_offset);
    pipeline.write_reg(REG_BLC_GB_OFFSET, iq.blc_gb_offset);
    pipeline.write_reg(REG_BLC_B_OFFSET,  iq.blc_b_offset);
    pipeline.write_reg(REG_BLC_R_SAT,     iq.blc_r_sat);
    pipeline.write_reg(REG_BLC_GR_SAT,    iq.blc_gr_sat);
    pipeline.write_reg(REG_BLC_GB_SAT,    iq.blc_gb_sat);
    pipeline.write_reg(REG_BLC_B_SAT,     iq.blc_b_sat);

    // DPC
    pipeline.write_reg(REG_DPC_ENABLE,    iq.dpc_enable);
    pipeline.write_reg(REG_DPC_THRESH,    iq.dpc_thresh);

    // LSC (grid dims only; LUT itself is left at the pipeline's default
    // unless an external source reloads it via REG_LSC_LUT_ADDR/DATA).
    pipeline.write_reg(REG_LSC_ENABLE,    iq.lsc_enable);
    pipeline.write_reg(REG_LSC_GRID_W,    iq.lsc_grid_w);
    pipeline.write_reg(REG_LSC_GRID_H,    iq.lsc_grid_h);

    // DG
    pipeline.write_reg(REG_DG_ENABLE,     iq.dg_enable);
    pipeline.write_reg(REG_DG_GAIN,       iq.dg_gain);
    pipeline.write_reg(REG_DG_AUTO,       iq.dg_auto);

    // BNR
    pipeline.write_reg(REG_BNR_ENABLE,       iq.bnr_enable);
    pipeline.write_reg(REG_BNR_WINDOW,       iq.bnr_window);
    pipeline.write_reg(REG_BNR_R_STD_DEV_S,  float_to_reg(iq.bnr_r_std_dev_s));
    pipeline.write_reg(REG_BNR_R_STD_DEV_R,  float_to_reg(iq.bnr_r_std_dev_r));
    pipeline.write_reg(REG_BNR_G_STD_DEV_S,  float_to_reg(iq.bnr_g_std_dev_s));
    pipeline.write_reg(REG_BNR_G_STD_DEV_R,  float_to_reg(iq.bnr_g_std_dev_r));
    pipeline.write_reg(REG_BNR_B_STD_DEV_S,  float_to_reg(iq.bnr_b_std_dev_s));
    pipeline.write_reg(REG_BNR_B_STD_DEV_R,  float_to_reg(iq.bnr_b_std_dev_r));

    // Demosaic
    pipeline.write_reg(REG_DEMOSAIC_ENABLE, iq.demosaic_enable);

    // AWB
    pipeline.write_reg(REG_AWB_ENABLE,      iq.awb_enable);
    pipeline.write_reg(REG_AWB_ALGORITHM,   iq.awb_algorithm);
    pipeline.write_reg(REG_AWB_R_GAIN,      float_to_reg(iq.awb_r_gain));
    pipeline.write_reg(REG_AWB_B_GAIN,      float_to_reg(iq.awb_b_gain));
    pipeline.write_reg(REG_AWB_UNDER_PCT,   float_to_reg(iq.awb_under_pct));
    pipeline.write_reg(REG_AWB_OVER_PCT,    float_to_reg(iq.awb_over_pct));
    pipeline.write_reg(REG_AWB_PERCENT,     float_to_reg(iq.awb_percent));

    // WB
    pipeline.write_reg(REG_WB_ENABLE,       iq.wb_enable);
    pipeline.write_reg(REG_WB_R_GAIN,       float_to_reg(iq.wb_r_gain));
    pipeline.write_reg(REG_WB_B_GAIN,       float_to_reg(iq.wb_b_gain));

    // CCM
    pipeline.write_reg(REG_CCM_ENABLE,      iq.ccm_enable);
    pipeline.write_reg(REG_CCM_MATRIX00,    float_to_reg(iq.ccm_matrix00));
    pipeline.write_reg(REG_CCM_MATRIX01,    float_to_reg(iq.ccm_matrix01));
    pipeline.write_reg(REG_CCM_MATRIX02,    float_to_reg(iq.ccm_matrix02));
    pipeline.write_reg(REG_CCM_MATRIX10,    float_to_reg(iq.ccm_matrix10));
    pipeline.write_reg(REG_CCM_MATRIX11,    float_to_reg(iq.ccm_matrix11));
    pipeline.write_reg(REG_CCM_MATRIX12,    float_to_reg(iq.ccm_matrix12));
    pipeline.write_reg(REG_CCM_MATRIX20,    float_to_reg(iq.ccm_matrix20));
    pipeline.write_reg(REG_CCM_MATRIX21,    float_to_reg(iq.ccm_matrix21));
    pipeline.write_reg(REG_CCM_MATRIX22,    float_to_reg(iq.ccm_matrix22));

    // GC
    pipeline.write_reg(REG_GC_ENABLE,       iq.gc_enable);
    pipeline.write_reg(REG_GC_GAMMA,        iq.gc_gamma);

    // AEC
    pipeline.write_reg(REG_AEC_ENABLE,      iq.aec_enable);
    pipeline.write_reg(REG_AEC_FEEDBACK,    iq.aec_feedback);
    pipeline.write_reg(REG_AEC_CENTER_ILLUM,iq.aec_center_illum);
    pipeline.write_reg(REG_AEC_SKEWNESS,    float_to_reg(iq.aec_skewness));

    // CSC
    pipeline.write_reg(REG_CSC_ENABLE,      iq.csc_enable);
    pipeline.write_reg(REG_CSC_STANDARD,    iq.csc_standard);

    // CSE
    pipeline.write_reg(REG_CSE_ENABLE,      iq.cse_enable);
    pipeline.write_reg(REG_CSE_SAT_GAIN,    float_to_reg(iq.cse_sat_gain));

    // Sharpen
    pipeline.write_reg(REG_SHARPEN_ENABLE,    iq.sharpen_enable);
    pipeline.write_reg(REG_SHARPEN_SIGMA,     iq.sharpen_sigma);
    pipeline.write_reg(REG_SHARPEN_STRENGTH,  iq.sharpen_strength);

    // 2DNR
    pipeline.write_reg(REG_2DNR_ENABLE,  iq.twodnr_enable);
    pipeline.write_reg(REG_2DNR_WINDOW,  iq.twodnr_window);
    pipeline.write_reg(REG_2DNR_PATCH,   iq.twodnr_patch);
    pipeline.write_reg(REG_2DNR_WTS,     iq.twodnr_wts);

    // Scale
    pipeline.write_reg(REG_SCALE_ENABLE, iq.scale_enable);
    pipeline.write_reg(REG_SCALE_OUT_W, iq.scale_out_w);
    pipeline.write_reg(REG_SCALE_OUT_H, iq.scale_out_h);

    // YUV420
    pipeline.write_reg(REG_YUV420_ENABLE, iq.yuv420_enable);
}

// One-shot helper: load `path` and apply it to `pipeline`. Returns the
// parsed struct so the caller can inspect it.
inline isp_iq_config load_and_apply(isp_pipeline& pipeline, const std::string& path) {
    isp_iq_config iq = load(path);
    apply(pipeline, iq);
    return iq;
}

// Locate a tuning file using the same heuristic as `isp_run`: prefer the
// explicit `path` if non-empty, otherwise look in
// `components/isp_tlm/tests/tuning.bin` and a few other common locations
// anchored at the repo root.
//
// Returns an empty string if no file is found.
inline std::string locate(const std::string& explicit_path = {}) {
    if (!explicit_path.empty() && std::ifstream(explicit_path).good()) {
        return explicit_path;
    }

    const char* env = std::getenv("ISP_TUNING_BIN");
    if (env != nullptr && std::ifstream(env).good()) {
        return std::string(env);
    }

    auto here = std::filesystem::path(__FILE__).parent_path();
    auto candidates = std::vector<std::filesystem::path>{
        here / "tests" / "tuning.bin",
        here / ".." / ".." / ".." / "tests" / "tuning.bin",
        here / ".." / "tests" / "tuning.bin",
        std::filesystem::current_path() / "tuning.bin",
        std::filesystem::current_path() / "components" / "isp_tlm" / "tests" / "tuning.bin",
    };

    // Also walk upward from this file to find any `components/isp_tlm/tests/tuning.bin`
    for (std::filesystem::path p = here; !p.empty(); p = p.parent_path()) {
        std::filesystem::path candidate = p / "components" / "isp_tlm" / "tests" / "tuning.bin";
        if (std::ifstream(candidate).good()) return candidate.string();
        candidate = p / "tuning.bin";
        if (std::ifstream(candidate).good()) return candidate.string();
        if (p == p.root_path()) break;
    }

    for (const auto& c : candidates) {
        if (std::ifstream(c).good()) return std::filesystem::absolute(c).string();
    }
    return {};
}

// Pretty-print the most useful fields of an isp_iq_config to stdout.
// Handy for `--describe` style debugging before running a pipeline.
inline void print_summary(const isp_iq_config& iq) {
    static const char* kPatterns[4] = {"RGGB", "GRBG", "BGGR", "GBRG"};
    const char* pat = (iq.bayer_pattern < 4) ? kPatterns[iq.bayer_pattern] : "UNKNOWN";

    std::cout << "[tuning_loader] IQ summary\n"
              << "  Image       : " << iq.width << "x" << iq.height
              << "  stride=" << iq.stride << "\n"
              << "  Bit depth   : " << static_cast<int>(iq.bit_depth) << "\n"
              << "  Bayer       : " << iq.bayer_pattern << " (" << pat << ")\n"
              << "  Scale       : enable=" << static_cast<int>(iq.scale_enable)
              << "  out=" << iq.scale_out_w << "x" << iq.scale_out_h << "\n";

    auto en = [](const char* n, std::uint8_t v) {
        std::cout << "  " << n << ": " << (v ? "on" : "off") << "\n";
    };
    std::cout << "  --- Enabled blocks ---\n";
    en("BLC     ", iq.blc_enable);
    en("DPC     ", iq.dpc_enable);
    en("LSC     ", iq.lsc_enable);
    en("DG      ", iq.dg_enable);
    en("BNR     ", iq.bnr_enable);
    en("Demosaic", iq.demosaic_enable);
    en("AWB     ", iq.awb_enable);
    en("WB      ", iq.wb_enable);
    en("CCM     ", iq.ccm_enable);
    en("GC      ", iq.gc_enable);
    en("AEC     ", iq.aec_enable);
    en("CSC     ", iq.csc_enable);
    en("CSE     ", iq.cse_enable);
    en("Sharpen ", iq.sharpen_enable);
    en("2DNR    ", iq.twodnr_enable);
    en("YUV420  ", iq.yuv420_enable);
}

}  // namespace tuning_loader
