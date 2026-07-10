#pragma once

#include <cstdint>

namespace cdc::components {

#pragma pack(push, 1)
struct isp_iq_config {
    std::uint32_t magic_word; // 0x49535021 ("ISP!")

    // Global Params
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t stride;
    std::uint32_t format;
    std::uint32_t op_mode;
    std::uint32_t bit_depth;
    std::uint32_t bayer_pattern;

    // BLC
    std::uint8_t  blc_enable;
    std::uint8_t  blc_linear;
    std::uint16_t blc_r_offset;
    std::uint16_t blc_gr_offset;
    std::uint16_t blc_gb_offset;
    std::uint16_t blc_b_offset;
    std::uint16_t blc_r_sat;
    std::uint16_t blc_gr_sat;
    std::uint16_t blc_gb_sat;
    std::uint16_t blc_b_sat;

    // DPC
    std::uint8_t  dpc_enable;
    std::uint16_t dpc_thresh;

    // LSC
    std::uint8_t  lsc_enable;
    std::uint32_t lsc_grid_w;
    std::uint32_t lsc_grid_h;

    // DG
    std::uint8_t  dg_enable;
    std::uint32_t dg_gain;
    std::uint32_t dg_auto;

    // BNR
    std::uint8_t  bnr_enable;
    std::uint32_t bnr_window;
    float         bnr_r_std_dev_s;
    float         bnr_r_std_dev_r;
    float         bnr_g_std_dev_s;
    float         bnr_g_std_dev_r;
    float         bnr_b_std_dev_s;
    float         bnr_b_std_dev_r;

    // Demosaic
    std::uint8_t  demosaic_enable;

    // AWB
    std::uint8_t  awb_enable;
    std::uint32_t awb_algorithm;
    float         awb_r_gain;
    float         awb_b_gain;
    float         awb_under_pct;
    float         awb_over_pct;
    float         awb_percent;

    // WB
    std::uint8_t  wb_enable;
    float         wb_r_gain;
    float         wb_b_gain;

    // CCM
    std::uint8_t  ccm_enable;
    float         ccm_matrix00;
    float         ccm_matrix01;
    float         ccm_matrix02;
    float         ccm_matrix10;
    float         ccm_matrix11;
    float         ccm_matrix12;
    float         ccm_matrix20;
    float         ccm_matrix21;
    float         ccm_matrix22;

    // GC
    std::uint8_t  gc_enable;
    std::uint32_t gc_gamma;

    // AEC
    std::uint8_t  aec_enable;
    std::uint32_t aec_feedback;
    std::uint32_t aec_center_illum;
    float         aec_skewness;

    // CSC
    std::uint8_t  csc_enable;
    std::uint32_t csc_standard;

    // CSE
    std::uint8_t  cse_enable;
    float         cse_sat_gain;

    // Sharpen
    std::uint8_t  sharpen_enable;
    std::uint32_t sharpen_sigma;
    std::uint32_t sharpen_strength;

    // 2DNR
    std::uint8_t  twodnr_enable;
    std::uint32_t twodnr_window;
    std::uint32_t twodnr_patch;
    std::uint32_t twodnr_wts;

    // Scale
    std::uint8_t  scale_enable;
    std::uint32_t scale_out_w;
    std::uint32_t scale_out_h;

    // YUV420
    std::uint8_t  yuv420_enable;
};
#pragma pack(pop)

} // namespace cdc::components
