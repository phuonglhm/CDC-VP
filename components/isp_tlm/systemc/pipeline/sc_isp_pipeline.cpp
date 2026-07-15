/**
 * @file sc_isp_pipeline.cpp
 * @brief Implementation of sc_isp_pipeline SystemC module
 */
#include "sc_isp_pipeline.h"
#include <iostream>

sc_isp_pipeline::sc_isp_pipeline(sc_core::sc_module_name name,
                                const isp_config& cfg,
                                const std::vector<float>& lsc_lut,
                                sc_core::sc_fifo<std::uint16_t>* raw_in_fifo,
                                sc_core::sc_fifo<std::uint8_t>* yuv_out_fifo,
                                std::uint8_t input_bit_depth,
                                cfa_types bayer_pattern)
    : sc_module(name)
    , m_cfg(cfg)
    , m_lsc_lut(lsc_lut)
    , m_width(cfg.scale.in_width)
    , m_height(cfg.scale.in_height)
    , m_bit_depth(12)
    , m_input_bit_depth(input_bit_depth)
    , m_bayer_pattern(bayer_pattern)
    , raw_in(raw_in_fifo)
    , yuv_out(yuv_out_fifo) {

    std::cout << "[sc_isp_pipeline] Initializing ISP pipeline..." << std::endl;
    std::cout << "[sc_isp_pipeline] Image size: " << m_width << "x" << m_height << std::endl;

    init_modules();
    bind_channels();

    std::cout << "[sc_isp_pipeline] Pipeline initialization complete" << std::endl;
}

sc_isp_pipeline::~sc_isp_pipeline() {
    // Cleanup dynamically allocated FIFOs and modules
}

void sc_isp_pipeline::init_modules() {
    // Allocate FIFOs
    fifo_in_norm = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_blc_dpc = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_dpc_lsc = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_lsc_dg = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_dg_bnr = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_bnr_demosaic = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_demosaic_awb = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_demosaic_wb = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_wb_ccm = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_ccm_gc = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_gc_aec = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_aec_csc = new sc_core::sc_fifo<std::uint16_t>(FIFO_DEPTH);
    fifo_csc_cse = new sc_core::sc_fifo<std::uint8_t>(FIFO_DEPTH);
    fifo_cse_sharpen = new sc_core::sc_fifo<std::uint8_t>(FIFO_DEPTH);
    fifo_sharpen_2dnr = new sc_core::sc_fifo<std::uint8_t>(FIFO_DEPTH);
    fifo_2dnr_scale = new sc_core::sc_fifo<std::uint8_t>(FIFO_DEPTH);
    fifo_scale_yuv420 = new sc_core::sc_fifo<std::uint8_t>(FIFO_DEPTH);
    fifo_2dnr_yuv420 = new sc_core::sc_fifo<std::uint8_t>(FIFO_DEPTH);

    // RAW domain processing
    m_input_norm = new sc_input_normalizer("input_norm", m_input_bit_depth, m_bit_depth);
    m_blc = new sc_blc("blc", m_cfg.blc, m_bayer_pattern, m_bit_depth);
    m_dpc = new sc_dpc("dpc", m_cfg.dpc, m_width, m_height);
    m_lsc = new sc_lsc("lsc", m_cfg.lsc, m_lsc_lut, m_bayer_pattern, m_bit_depth, m_width, m_height);
    m_dg = new sc_dg("dg", m_cfg.dg, m_bit_depth);
    m_bnr = new sc_bnr("bnr", m_cfg.bnr, m_bayer_pattern, m_bit_depth, m_width, m_height);

    // RGB domain processing
    m_demosaic = new sc_demosaic("demosaic", m_cfg.demosaic, m_bayer_pattern, m_bit_depth, m_width, m_height);
    m_awb = new sc_awb("awb", m_cfg.awb, m_width, m_height, m_bit_depth);
    m_wb = new sc_wb("wb", m_cfg.wb);
    m_ccm = new sc_ccm("ccm", m_cfg.ccm);
    m_gc = new sc_gc("gc", m_cfg.gc);
    m_aec = new sc_aec("aec", m_cfg.aec, m_width, m_height, m_bit_depth);
    m_csc = new sc_csc("csc", m_cfg.csc);

    // YUV domain processing
    m_cse = new sc_cse("cse", m_cfg.cse);
    m_sharpen = new sc_sharpen("sharpen", m_cfg.sharpen, m_width, m_height);
    m_2dnr = new sc_2dnr("2dnr", m_cfg.twodnr, m_width, m_height);
    if (m_cfg.scale.is_enable) {
        m_scale = new sc_scale("scale", m_cfg.scale, m_width, m_height);
        m_yuv420 = new sc_yuv420("yuv420", m_cfg.yuv420,
                                 m_cfg.scale.out_width, m_cfg.scale.out_height);
    } else {
        m_scale = nullptr;
        // When scale is disabled, the YUV420 block operates on full-resolution
        // (cfg.scale.in_width/in_height) input.
        m_yuv420 = new sc_yuv420("yuv420", m_cfg.yuv420, m_width, m_height);
    }
}

void sc_isp_pipeline::bind_channels() {
    // RAW domain connections
    m_input_norm->fifo_in(*raw_in);
    m_input_norm->fifo_out(*fifo_in_norm);

    m_blc->fifo_in(*fifo_in_norm);
    m_blc->fifo_out(*fifo_blc_dpc);

    m_dpc->fifo_in(*fifo_blc_dpc);
    m_dpc->fifo_out(*fifo_dpc_lsc);

    m_lsc->fifo_in(*fifo_dpc_lsc);
    m_lsc->fifo_out(*fifo_lsc_dg);

    m_dg->fifo_in(*fifo_lsc_dg);
    m_dg->fifo_out(*fifo_dg_bnr);

    m_bnr->fifo_in(*fifo_dg_bnr);
    m_bnr->fifo_out(*fifo_bnr_demosaic);

    // RGB domain connections
    // Demosaic outputs to a single FIFO; AWB and WB both consume it.
    m_demosaic->fifo_in(*fifo_bnr_demosaic);
    m_demosaic->fifo_out(*fifo_demosaic_awb);  // Single output shared downstream

    // AWB reads from demosaic, passes through unchanged, then WB also reads.
    // To avoid two writers on the same FIFO, we use a tee pattern:
    //   Demosaic -> fifo_demosaic_awb -> AWB (pass-through) -> fifo_demosaic_wb
    //   WB reads the AWB output (which is identical to demosaic output).
    m_awb->fifo_in(*fifo_demosaic_awb);
    m_awb->fifo_out(*fifo_demosaic_wb);  // AWB is pass-through

    // WB reads from AWB's pass-through output, applies R/B gains, writes to CCM.
    m_wb->fifo_in(*fifo_demosaic_wb);
    m_wb->fifo_out(*fifo_wb_ccm);

    m_wb->bind_awb(m_awb);

    m_ccm->fifo_in(*fifo_wb_ccm);
    m_ccm->fifo_out(*fifo_ccm_gc);

    m_gc->fifo_in(*fifo_ccm_gc);
    m_gc->fifo_out(*fifo_gc_aec);

    m_aec->fifo_in(*fifo_gc_aec);
    m_aec->fifo_out(*fifo_aec_csc);

    m_csc->fifo_in(*fifo_aec_csc);
    m_csc->fifo_out(*fifo_csc_cse);

    // YUV444 domain connections
    m_cse->fifo_in(*fifo_csc_cse);
    m_cse->fifo_out(*fifo_cse_sharpen);

    m_sharpen->fifo_in(*fifo_cse_sharpen);
    m_sharpen->fifo_out(*fifo_sharpen_2dnr);

    m_2dnr->fifo_in(*fifo_sharpen_2dnr);

    // Scale path
    if (m_cfg.scale.is_enable) {
        m_2dnr->fifo_out(*fifo_2dnr_scale);
        m_scale->fifo_in(*fifo_2dnr_scale);
        m_scale->fifo_out(*fifo_scale_yuv420);
        m_yuv420->fifo_in(*fifo_scale_yuv420);
    } else {
        m_2dnr->fifo_out(*fifo_2dnr_yuv420);
        m_yuv420->fifo_in(*fifo_2dnr_yuv420);
    }

    // Final output
    m_yuv420->fifo_out(*yuv_out);
}

float sc_isp_pipeline::get_awb_r_gain() const {
    return m_awb ? m_awb->get_r_gain() : 1.0f;
}

float sc_isp_pipeline::get_awb_b_gain() const {
    return m_awb ? m_awb->get_b_gain() : 1.0f;
}

std::int32_t sc_isp_pipeline::get_aec_feedback() const {
    return m_aec ? m_aec->get_ae_feedback() : 0;
}

void sc_isp_pipeline::precompute_awb_gains(const std::uint16_t* rgb12) {
    if (m_awb == nullptr) return;
    m_awb->precompute_gains(rgb12, awb_input_pixels());
}

void sc_isp_pipeline::precompute_awb_gains_from_bayer(const std::uint16_t* bayer16) {
    if (m_awb == nullptr) return;
    m_awb->set_bayer_pattern(m_bayer_pattern);
    m_awb->set_input_bit_depth(m_input_bit_depth);
    m_awb->precompute_gains_from_bayer(bayer16, awb_input_pixels());
}

void sc_isp_pipeline::prime_awb_gains(float r_gain, float b_gain) {
    if (m_awb == nullptr) return;
    m_awb->prime_gains(r_gain, b_gain);
}
