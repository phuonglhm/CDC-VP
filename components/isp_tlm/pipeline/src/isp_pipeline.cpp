#include "isp_pipeline.h"

#include <cstring>
#include <iostream>

isp_pipeline::isp_pipeline()
    : width_(0)
    , height_(0)
    , input_bit_depth_(12)
    , input_bayer_pattern_(cfa_types::RGGB)
    , working_bit_depth_(12)
    , lsc_mem_ptr_(nullptr)
    , awb_r_gain_(1.0f)
    , awb_b_gain_(1.0f)
{
}

isp_pipeline::~isp_pipeline()
{
}

void isp_pipeline::set_dimensions(std::uint32_t width, std::uint32_t height)
{
    width_ = width;
    height_ = height;

    const std::size_t raw_pixels = static_cast<std::size_t>(width_) * height_;
    const std::size_t rgb_pixels = raw_pixels * 3u;
    const std::size_t yuv_pixels = raw_pixels * 3u;

    raw_buf_.resize(raw_pixels);
    blc_out_.resize(raw_pixels);
    dpc_out_.resize(raw_pixels);
    lsc_out_.resize(raw_pixels);
    dg_out_.resize(raw_pixels);
    bnr_out_.resize(raw_pixels);
    demosaic_out_.resize(rgb_pixels);
    wb_out_.resize(rgb_pixels);
    ccm_out_.resize(rgb_pixels);
    gc_out_.resize(rgb_pixels);
    csc_out_.resize(yuv_pixels);
    cse_out_.resize(yuv_pixels);
    sharpen_out_.resize(yuv_pixels);
    twodnr_out_.resize(yuv_pixels);
    scale_out_.resize(yuv_pixels);
    final_out_.resize(yuv_pixels);
}

void isp_pipeline::set_lsc_mem(const float* lsc_mem)
{
    lsc_mem_ptr_ = lsc_mem;
}

void isp_pipeline::set_input_format(std::uint8_t bit_depth, cfa_types bayer_pattern)
{
    input_bit_depth_ = bit_depth;
    input_bayer_pattern_ = bayer_pattern;
    // All processing blocks operate at 12-bit working precision
    working_bit_depth_ = 12;
}

void isp_pipeline::run(const std::uint16_t* raw_in,
                       std::vector<std::uint8_t>& yuv_out,
                       const isp_config& cfg)
{
    if (width_ == 0 || height_ == 0 || raw_in == nullptr) {
        yuv_out.clear();
        return;
    }

    const std::size_t raw_pixels = static_cast<std::size_t>(width_) * height_;
    const std::size_t rgb_pixels = raw_pixels * 3u;
    const std::size_t yuv_pixels = raw_pixels * 3u;

    // Normalize input to 12-bit working range (0..4095).
    // For 12-bit input: no change. For 16-bit input: shift right by 4.
    const std::uint32_t work_max = (1u << working_bit_depth_) - 1u;
    const std::uint32_t src_max  = (1u << input_bit_depth_)  - 1u;
    if (src_max == 0) return;
    // Pre-compute scale = work_max / src_max as Q16 fixed point for speed
    // but keep it readable using 64-bit math
    const std::uint64_t scale_num = work_max;
    const std::uint64_t scale_den = src_max;

    std::memcpy(raw_buf_.data(), raw_in, raw_pixels * sizeof(std::uint16_t));
    for (std::size_t p = 0; p < raw_pixels; ++p) {
        const std::uint64_t v = static_cast<std::uint64_t>(raw_buf_[p]);
        const std::uint64_t scaled = (v * scale_num) / scale_den;
        raw_buf_[p] = static_cast<std::uint16_t>(scaled > work_max ? work_max : scaled);
    }

    const std::uint8_t bd = working_bit_depth_;
    const cfa_types cfa = input_bayer_pattern_;

    blc_.process(raw_buf_.data(), blc_out_.data(), width_, height_,
                 cfg.blc, cfa, bd);
    dpc_.process(blc_out_.data(), dpc_out_.data(), width_, height_, cfg.dpc);
    lsc_.process(dpc_out_.data(), lsc_out_.data(), width_, height_,
                 cfg.lsc, lsc_mem_ptr_, cfa, bd);
    dg_.process(lsc_out_.data(), dg_out_.data(), width_, height_, cfg.dg, bd);
    bnr_.process(dg_out_.data(), bnr_out_.data(), width_, height_,
                 cfg.bnr, cfa, bd);

    awb_config awb_cfg = cfg.awb;
    if (cfg.awb.is_enable) {
        awb_.process(bnr_out_.data(), width_, height_, awb_cfg, bd);
        awb_r_gain_ = awb_cfg.r_gain_out;
        awb_b_gain_ = awb_cfg.b_gain_out;
    } else {
        awb_r_gain_ = 1.0f;
        awb_b_gain_ = 1.0f;
    }

    demosaic_.process(bnr_out_.data(), demosaic_out_.data(), width_, height_,
                      cfg.demosaic, cfa, bd);

    wb_config wb_cfg = cfg.wb;
    wb_cfg.r_gain = cfg.wb.is_enable ? cfg.wb.r_gain : 1.0f;
    wb_cfg.b_gain = cfg.wb.is_enable ? cfg.wb.b_gain : 1.0f;
    if (cfg.awb.is_enable) {
        wb_cfg.r_gain *= awb_r_gain_;
        wb_cfg.b_gain *= awb_b_gain_;
    }
    wb_.process(demosaic_out_.data(), wb_out_.data(), width_, height_, wb_cfg);

    ccm_.process(wb_out_.data(), ccm_out_.data(), width_, height_, cfg.ccm);
    gc_.process(ccm_out_.data(), gc_out_.data(), width_, height_, cfg.gc);
    csc_.process(gc_out_.data(), csc_out_.data(), width_, height_, cfg.csc);
    cse_.process(csc_out_.data(), cse_out_.data(), width_, height_, cfg.cse);
    sharpen_.process(cse_out_.data(), sharpen_out_.data(), width_, height_, cfg.sharpen);
    twodnr_.process(sharpen_out_.data(), twodnr_out_.data(), width_, height_, cfg.twodnr);

    if (cfg.scale.is_enable) {
        scale_.process(twodnr_out_.data(), scale_out_.data(),
                       width_, height_,
                       cfg.scale.out_width, cfg.scale.out_height,
                       cfg.scale);
        if (cfg.yuv420.is_enable) {
            yuv420_.process(scale_out_.data(), final_out_.data(),
                            cfg.scale.out_width, cfg.scale.out_height,
                            cfg.yuv420);
        } else {
            final_out_ = scale_out_;
        }
    } else if (cfg.yuv420.is_enable) {
        yuv420_.process(twodnr_out_.data(), final_out_.data(),
                        width_, height_, cfg.yuv420);
    } else {
        final_out_ = twodnr_out_;
    }

    yuv_out = final_out_;
}
