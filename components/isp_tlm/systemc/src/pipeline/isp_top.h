/*
 * ISP Top Module - Complete Pipeline Implementation
 * Matches RTL isp_top module structure
 * All 17 blocks integrated in correct order
 */

#ifndef ISP_TOP_H
#define ISP_TOP_H

#include <systemc>
#include <tlm>
#include <memory>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "common/isp_params.h"
#include "pipeline/isp_config.h"

// Include all block headers
#include "blocks/crop/isp_crop.h"
#include "blocks/dpc/isp_dpc.h"
#include "blocks/blc/isp_blc.h"
#include "blocks/oecf/isp_oecf.h"
#include "blocks/dgain/isp_dgain.h"
#include "blocks/lsc/isp_lsc.h"
#include "blocks/bnr/isp_bnr.h"
#include "blocks/wb/isp_wb.h"
#include "blocks/demosaic/isp_demosaic.h"
#include "blocks/ccm/isp_ccm.h"
#include "blocks/gamma/isp_gamma.h"
#include "blocks/csc/isp_csc.h"
#include "blocks/ldci/isp_ldci.h"
#include "blocks/sharpen/isp_sharpen.h"
#include "blocks/2dnr/isp_2dnr.h"
#include "blocks/awb/isp_awb.h"
#include "blocks/ae/isp_ae.h"

// Include metrics
#include "blocks/crop/crop_metrics.h"
#include "blocks/blc/blc_metrics.h"
#include "blocks/wb/wb_metrics.h"
#include "blocks/ccm/ccm_metrics.h"
#include "blocks/csc/csc_metrics.h"
#include "blocks/demosaic/demosaic_metrics.h"
#include "blocks/gamma/gamma_metrics.h"
#include "blocks/sharpen/sharpen_metrics.h"
#include "blocks/dpc/dpc_metrics.h"
#include "blocks/bnr/bnr_metrics.h"
#include "blocks/2dnr/nr2d_metrics.h"
#include "blocks/awb/awb_metrics.h"
#include "blocks/ae/ae_metrics.h"

//=============================================================================
// ISP Pipeline Metrics Container
//=============================================================================
struct IspPipelineMetrics {
    uint64_t total_cycles = 0;
    uint64_t total_pixels_processed = 0;
    uint64_t total_frames_processed = 0;
    double total_processing_time_us = 0.0;

    CropMetricsCollector* crop_metrics = nullptr;
    BlcMetricsCollector* blc_metrics = nullptr;
    DpcMetricsCollector* dpc_metrics = nullptr;
    OecfMetricsCollector* oecf_metrics = nullptr;
    WbMetricsCollector* wb_metrics = nullptr;
    BnrMetricsCollector* bnr_metrics = nullptr;
    DemosaicMetricsCollector* demosaic_metrics = nullptr;
    CcmMetricsCollector* ccm_metrics = nullptr;
    GammaMetricsCollector* gamma_metrics = nullptr;
    CscMetricsCollector* csc_metrics = nullptr;
    SharpenMetricsCollector* sharpen_metrics = nullptr;
    Nr2dMetricsCollector* nr2d_metrics = nullptr;
    AwbMetricsCollector* awb_metrics = nullptr;
    AeMetricsCollector* ae_metrics = nullptr;
};

//=============================================================================
// ISP Top Module
//=============================================================================
template<unsigned int BITS = 10, BayerPattern BAYER = BayerPattern::RGGB,
         unsigned int WIDTH = 64, unsigned int HEIGHT = 32>
class isp_top : public sc_module {
public:
    // Clock and Reset
    sc_in<bool> pclk{"pclk"};
    sc_in<bool> rst_n{"rst_n"};

    // Input Interface (RAW from sensor)
    sc_in<bool> in_href{"in_href"};
    sc_in<bool> in_vsync{"in_vsync"};
    sc_in<uint16_t> in_raw{"in_raw"};

    // Output Interface (YUV)
    sc_out<bool> out_href{"out_href"};
    sc_out<bool> out_vsync{"out_vsync"};
    sc_out<uint8_t> out_y{"out_y"};
    sc_out<uint8_t> out_u{"out_u"};
    sc_out<uint8_t> out_v{"out_v"};

    // Module Enables
    sc_in<bool> crop_en{"crop_en"};
    sc_in<bool> dpc_en{"dpc_en"};
    sc_in<bool> blc_en{"blc_en"};
    sc_in<bool> oecf_en{"oecf_en"};
    sc_in<bool> dgain_en{"dgain_en"};
    sc_in<bool> lsc_en{"lsc_en"};
    sc_in<bool> bnr_en{"bnr_en"};
    sc_in<bool> wb_en{"wb_en"};
    sc_in<bool> demosaic_en{"demosaic_en"};
    sc_in<bool> ccm_en{"ccm_en"};
    sc_in<bool> gamma_en{"gamma_en"};
    sc_in<bool> csc_en{"csc_en"};
    sc_in<bool> ldci_en{"ldci_en"};
    sc_in<bool> sharpen_en{"sharpen_en"};
    sc_in<bool> nr2d_en{"nr2d_en"};

    // CROP Parameters
    sc_in<uint16_t> crop_w{"crop_w"}, crop_h{"crop_h"};
    sc_in<uint16_t> crop_x{"crop_x"}, crop_y{"crop_y"};

    // DGain Parameters
    sc_in<bool> dgain_manual{"dgain_manual"};
    sc_in<uint8_t> dgain_manual_index{"dgain_manual_index"};
    sc_in<uint8_t> dgain_ae_feedback_index{"dgain_ae_feedback_index"};
    static constexpr unsigned DGainArraySize = 100;
    sc_in<uint8_t> dgain_array[DGainArraySize];

    // BLC Parameters
    sc_in<uint16_t> blc_r{"blc_r"}, blc_gr{"blc_gr"}, blc_gb{"blc_gb"}, blc_b{"blc_b"};
    sc_in<bool> linear_en{"linear_en"};
    sc_in<uint16_t> linear_r{"linear_r"}, linear_gr{"linear_gr"}, linear_gb{"linear_gb"}, linear_b{"linear_b"};

    // DPC Parameters
    sc_in<uint16_t> dpc_threshold{"dpc_threshold"};

    // WB Parameters
    sc_in<uint16_t> wb_rgain{"wb_rgain"}, wb_bgain{"wb_bgain"};

    // CCM Parameters
    sc_in<int16_t> ccm_rr{"ccm_rr"}, ccm_rg{"ccm_rg"}, ccm_rb{"ccm_rb"};
    sc_in<int16_t> ccm_gr{"ccm_gr"}, ccm_gg{"ccm_gg"}, ccm_gb{"ccm_gb"};
    sc_in<int16_t> ccm_br{"ccm_br"}, ccm_bg{"ccm_bg"}, ccm_bb{"ccm_bb"};

    // CSC Parameters
    sc_in<uint8_t> csc_conv_standard{"csc_conv_standard"};

    // Sharpen Parameters
    sc_in<uint16_t> sharpen_strength{"sharpen_strength"};

    // AWB/AE Configuration
    sc_in<uint16_t> awb_underexp{"awb_underexp"}, awb_overexp{"awb_overexp"};
    sc_in<uint8_t> awb_frames{"awb_frames"};
    sc_in<uint8_t> ae_center_illum{"ae_center_illum"};
    sc_in<bool> awb_en{"awb_en"};
    sc_in<bool> ae_en{"ae_en"};

    // AE Crop
    sc_in<uint16_t> ae_crop_left{"ae_crop_left"};
    sc_in<uint16_t> ae_crop_right{"ae_crop_right"};
    sc_in<uint16_t> ae_crop_top{"ae_crop_top"};
    sc_in<uint16_t> ae_crop_bottom{"ae_crop_bottom"};
    sc_in<uint16_t> ae_target_skewness{"ae_target_skewness"};

    // AWB/AE Outputs
    sc_out<uint16_t> awb_r_gain{"awb_r_gain"}, awb_b_gain{"awb_b_gain"};
    sc_out<int8_t> ae_response{"ae_response"};
    sc_out<uint16_t> ae_result_skewness{"ae_result_skewness"};
    sc_out<bool> ae_done{"ae_done"};

    // DGain Outputs
    sc_out<uint8_t> dgain_applied_index{"dgain_applied_index"};

    // Constructor
    isp_top(const sc_module_name& name)
        : sc_module(name)
        , m_frame_count(0)
        , m_prev_vsync(false)
    {
        // Instantiate all blocks
        m_crop = new isp_crop<BITS, WIDTH, HEIGHT>("crop");
        m_dpc = new isp_dpc<BITS, BAYER>("dpc");
        m_blc = new isp_blc<BITS>("blc");
        m_oecf = new isp_oecf<BITS, BAYER>("oecf");
        m_dgain = new isp_dgain<BITS>("dgain");
        m_lsc = new isp_lsc<BITS>("lsc");
        m_bnr = new isp_bnr<BITS, BAYER>("bnr");
        m_wb = new isp_wb<BITS, BAYER>("wb");
        m_demosaic = new isp_demosaic<BITS, BAYER>("demosaic");
        m_ccm = new isp_ccm<BITS>("ccm");
        m_gamma = new isp_gamma<BITS>("gamma");
        m_csc = new isp_csc<BITS>("csc");
        m_ldci = new isp_ldci("ldci");
        m_sharpen = new isp_sharpen("sharpen");
        m_2dnr = new isp_2dnr("2dnr");
        m_awb = new isp_awb<BITS, BAYER>("awb");
        m_ae = new isp_ae<BITS>("ae");

        bind_ports();
    }

    virtual ~isp_top() {
        delete m_crop;
        delete m_dpc;
        delete m_blc;
        delete m_oecf;
        delete m_dgain;
        delete m_lsc;
        delete m_bnr;
        delete m_wb;
        delete m_demosaic;
        delete m_ccm;
        delete m_gamma;
        delete m_csc;
        delete m_ldci;
        delete m_sharpen;
        delete m_2dnr;
        delete m_awb;
        delete m_ae;
    }

    // Metrics accessors
    CropMetricsCollector& get_crop_metrics() { return m_crop->get_metrics(); }
    BlcMetricsCollector& get_blc_metrics() { return m_blc->get_metrics(); }
    DpcMetricsCollector& get_dpc_metrics() { return m_dpc->get_metrics(); }
    WbMetricsCollector& get_wb_metrics() { return m_wb->get_metrics(); }
    CcmMetricsCollector& get_ccm_metrics() { return m_ccm->get_metrics(); }
    CscMetricsCollector& get_csc_metrics() { return m_csc->get_metrics(); }
    DemosaicMetricsCollector& get_demosaic_metrics() { return m_demosaic->get_metrics(); }
    GammaMetricsCollector& get_gamma_metrics() { return m_gamma->get_metrics(); }
    SharpenMetricsCollector& get_sharpen_metrics() { return m_sharpen->get_metrics(); }
    BnrMetricsCollector& get_bnr_metrics() { return m_bnr->get_metrics(); }
    Nr2dMetricsCollector& get_nr2d_metrics() { return m_2dnr->get_metrics(); }
    AwbMetricsCollector& get_awb_metrics() { return m_awb->get_metrics(); }
    AeMetricsCollector& get_ae_metrics() { return m_ae->get_metrics(); }

    void get_all_metrics(IspPipelineMetrics& metrics) {
        metrics.crop_metrics = &m_crop->get_metrics();
        metrics.blc_metrics = &m_blc->get_metrics();
        metrics.dpc_metrics = &m_dpc->get_metrics();
        metrics.wb_metrics = &m_wb->get_metrics();
        metrics.ccm_metrics = &m_ccm->get_metrics();
        metrics.csc_metrics = &m_csc->get_metrics();
        metrics.demosaic_metrics = &m_demosaic->get_metrics();
        metrics.gamma_metrics = &m_gamma->get_metrics();
        metrics.sharpen_metrics = &m_sharpen->get_metrics();
        metrics.bnr_metrics = &m_bnr->get_metrics();
        metrics.nr2d_metrics = &m_2dnr->get_metrics();
        metrics.awb_metrics = &m_awb->get_metrics();
        metrics.ae_metrics = &m_ae->get_metrics();
    }

    unsigned get_frame_count() const { return m_frame_count; }

    void set_image_size(unsigned w, unsigned h) {
        m_crop->set_image_size(w, h);
        m_dpc->set_image_size(w, h);
        m_bnr->set_image_size(w, h);
        m_demosaic->set_image_size(w, h);
        m_ccm->set_image_size(w, h);
        m_csc->set_image_size(w, h);
        m_gamma->set_image_size(w, h);
        m_sharpen->set_image_size(w, h);
        m_2dnr->set_image_size(w, h);
    }

private:
    // Block pointers
    isp_crop<BITS, WIDTH, HEIGHT>* m_crop;
    isp_dpc<BITS, BAYER>* m_dpc;
    isp_blc<BITS>* m_blc;
    isp_oecf<BITS, BAYER>* m_oecf;
    isp_dgain<BITS>* m_dgain;
    isp_lsc<BITS>* m_lsc;
    isp_bnr<BITS, BAYER>* m_bnr;
    isp_wb<BITS, BAYER>* m_wb;
    isp_demosaic<BITS, BAYER>* m_demosaic;
    isp_ccm<BITS>* m_ccm;
    isp_gamma<BITS>* m_gamma;
    isp_csc<BITS>* m_csc;
    isp_ldci* m_ldci;
    isp_sharpen* m_sharpen;
    isp_2dnr* m_2dnr;
    isp_awb<BITS, BAYER>* m_awb;
    isp_ae<BITS>* m_ae;

    unsigned m_frame_count;
    bool m_prev_vsync;

    // Internal signals
    sc_signal<bool> s_crop_href{"s_crop_href"}, s_crop_vsync{"s_crop_vsync"};
    sc_signal<uint16_t> s_crop_data{"s_crop_data"};

    sc_signal<bool> s_dpc_href{"s_dpc_href"}, s_dpc_vsync{"s_dpc_vsync"};
    sc_signal<uint16_t> s_dpc_data{"s_dpc_data"};

    sc_signal<bool> s_blc_href{"s_blc_href"}, s_blc_vsync{"s_blc_vsync"};
    sc_signal<uint16_t> s_blc_data{"s_blc_data"};

    sc_signal<bool> s_oecf_href{"s_oecf_href"}, s_oecf_vsync{"s_oecf_vsync"};
    sc_signal<uint16_t> s_oecf_data{"s_oecf_data"};

    sc_signal<bool> s_dgain_href{"s_dgain_href"}, s_dgain_vsync{"s_dgain_vsync"};
    sc_signal<uint16_t> s_dgain_data{"s_dgain_data"};

    sc_signal<bool> s_lsc_href{"s_lsc_href"}, s_lsc_vsync{"s_lsc_vsync"};
    sc_signal<uint16_t> s_lsc_data{"s_lsc_data"};

    sc_signal<bool> s_bnr_href{"s_bnr_href"}, s_bnr_vsync{"s_bnr_vsync"};
    sc_signal<uint16_t> s_bnr_data{"s_bnr_data"};

    sc_signal<bool> s_wb_href{"s_wb_href"}, s_wb_vsync{"s_wb_vsync"};
    sc_signal<uint16_t> s_wb_data{"s_wb_data"};

    sc_signal<bool> s_demosaic_href{"s_demosaic_href"}, s_demosaic_vsync{"s_demosaic_vsync"};
    sc_signal<uint16_t> s_demosaic_r{"s_demosaic_r"}, s_demosaic_g{"s_demosaic_g"}, s_demosaic_b{"s_demosaic_b"};

    sc_signal<bool> s_ccm_href{"s_ccm_href"}, s_ccm_vsync{"s_ccm_vsync"};
    sc_signal<uint16_t> s_ccm_r{"s_ccm_r"}, s_ccm_g{"s_ccm_g"}, s_ccm_b{"s_ccm_b"};

    sc_signal<bool> s_gamma_href{"s_gamma_href"}, s_gamma_vsync{"s_gamma_vsync"};
    sc_signal<uint16_t> s_gamma_r{"s_gamma_r"}, s_gamma_g{"s_gamma_g"}, s_gamma_b{"s_gamma_b"};

    sc_signal<bool> s_csc_href{"s_csc_href"}, s_csc_vsync{"s_csc_vsync"};
    sc_signal<uint8_t> s_csc_y{"s_csc_y"}, s_csc_u{"s_csc_u"}, s_csc_v{"s_csc_v"};

    sc_signal<bool> s_ldci_href{"s_ldci_href"}, s_ldci_vsync{"s_ldci_vsync"};
    sc_signal<uint8_t> s_ldci_y{"s_ldci_y"}, s_ldci_u{"s_ldci_u"}, s_ldci_v{"s_ldci_v"};

    sc_signal<bool> s_sharpen_href{"s_sharpen_href"}, s_sharpen_vsync{"s_sharpen_vsync"};
    sc_signal<uint8_t> s_sharpen_y{"s_sharpen_y"}, s_sharpen_u{"s_sharpen_u"}, s_sharpen_v{"s_sharpen_v"};

    sc_signal<bool> s_2dnr_href{"s_2dnr_href"}, s_2dnr_vsync{"s_2dnr_vsync"};
    sc_signal<uint8_t> s_2dnr_y{"s_2dnr_y"}, s_2dnr_u{"s_2dnr_u"}, s_2dnr_v{"s_2dnr_v"};

    // Output signals
    sc_signal<bool> s_out_href{"s_out_href"}, s_out_vsync{"s_out_vsync"};
    sc_signal<uint8_t> s_out_y{"s_out_y"}, s_out_u{"s_out_u"}, s_out_v{"s_out_v"};

    void bind_ports() {
        // Clock and reset for all blocks
        m_crop->pclk(pclk); m_crop->rst_n(rst_n);
        m_dpc->pclk(pclk); m_dpc->rst_n(rst_n);
        m_blc->pclk(pclk); m_blc->rst_n(rst_n);
        m_oecf->pclk(pclk); m_oecf->rst_n(rst_n);
        m_dgain->pclk(pclk); m_dgain->rst_n(rst_n);
        m_lsc->pclk(pclk); m_lsc->rst_n(rst_n);
        m_bnr->pclk(pclk); m_bnr->rst_n(rst_n);
        m_wb->pclk(pclk); m_wb->rst_n(rst_n);
        m_demosaic->pclk(pclk); m_demosaic->rst_n(rst_n);
        m_ccm->pclk(pclk); m_ccm->rst_n(rst_n);
        m_gamma->pclk(pclk); m_gamma->rst_n(rst_n);
        m_csc->pclk(pclk); m_csc->rst_n(rst_n);
        m_ldci->pclk(pclk); m_ldci->rst_n(rst_n);
        m_sharpen->pclk(pclk); m_sharpen->rst_n(rst_n);
        m_2dnr->pclk(pclk); m_2dnr->rst_n(rst_n);
        m_awb->pclk(pclk); m_awb->rst_n(rst_n);
        m_ae->pclk(pclk); m_ae->rst_n(rst_n);

        // Enables
        m_crop->enable(crop_en);
        m_dpc->enable(dpc_en);
        m_blc->enable(blc_en);
        m_oecf->enable(oecf_en);
        m_dgain->enable(dgain_en);
        m_lsc->enable(lsc_en);
        m_bnr->enable(bnr_en);
        m_wb->enable(wb_en);
        m_demosaic->enable(demosaic_en);
        m_ccm->enable(ccm_en);
        m_gamma->enable(gamma_en);
        m_csc->enable(csc_en);
        m_ldci->enable(ldci_en);
        m_sharpen->enable(sharpen_en);
        m_2dnr->enable(nr2d_en);
        m_awb->enable(awb_en);
        m_ae->enable(ae_en);

        // === RAW Processing Pipeline ===

        // CROP
        m_crop->i_href(in_href);
        m_crop->i_vsync(in_vsync);
        m_crop->i_data(in_raw);
        m_crop->i_crop_w(crop_w);
        m_crop->i_crop_h(crop_h);
        m_crop->i_crop_x(crop_x);
        m_crop->i_crop_y(crop_y);
        m_crop->o_href(s_crop_href);
        m_crop->o_vsync(s_crop_vsync);
        m_crop->o_data(s_crop_data);

        // DPC
        m_dpc->i_href(s_crop_href);
        m_dpc->i_vsync(s_crop_vsync);
        m_dpc->i_raw(s_crop_data);
        m_dpc->i_threshold(dpc_threshold);
        m_dpc->o_href(s_dpc_href);
        m_dpc->o_vsync(s_dpc_vsync);
        m_dpc->o_raw(s_dpc_data);

        // BLC
        m_blc->i_href(s_dpc_href);
        m_blc->i_vsync(s_dpc_vsync);
        m_blc->i_data(s_dpc_data);
        m_blc->i_blc_r(blc_r);
        m_blc->i_blc_gr(blc_gr);
        m_blc->i_blc_gb(blc_gb);
        m_blc->i_blc_b(blc_b);
        m_blc->i_linear_en(linear_en);
        m_blc->i_linear_r(linear_r);
        m_blc->i_linear_gr(linear_gr);
        m_blc->i_linear_gb(linear_gb);
        m_blc->i_linear_b(linear_b);
        m_blc->o_href(s_blc_href);
        m_blc->o_vsync(s_blc_vsync);
        m_blc->o_data(s_blc_data);

        // OECF
        m_oecf->i_href(s_blc_href);
        m_oecf->i_vsync(s_blc_vsync);
        m_oecf->i_raw(s_blc_data);
        m_oecf->o_href(s_oecf_href);
        m_oecf->o_vsync(s_oecf_vsync);
        m_oecf->o_raw(s_oecf_data);

        // DGain
        m_dgain->i_href(s_oecf_href);
        m_dgain->i_vsync(s_oecf_vsync);
        m_dgain->i_raw(s_oecf_data);
        m_dgain->i_is_manual(dgain_manual);
        m_dgain->i_manual_index(dgain_manual_index);
        m_dgain->i_ae_feedback_index(dgain_ae_feedback_index);
        for (unsigned i = 0; i < DGainArraySize; i++) {
            m_dgain->i_dgain_array[i](dgain_array[i]);
        }
        m_dgain->o_href(s_dgain_href);
        m_dgain->o_vsync(s_dgain_vsync);
        m_dgain->o_raw(s_dgain_data);
        m_dgain->o_applied_index(dgain_applied_index);

        // LSC (passthrough)
        m_lsc->i_href(s_dgain_href);
        m_lsc->i_vsync(s_dgain_vsync);
        m_lsc->i_raw(s_dgain_data);
        m_lsc->o_href(s_lsc_href);
        m_lsc->o_vsync(s_lsc_vsync);
        m_lsc->o_raw(s_lsc_data);

        // BNR
        m_bnr->i_href(s_lsc_href);
        m_bnr->i_vsync(s_lsc_vsync);
        m_bnr->i_raw(s_lsc_data);
        m_bnr->o_href(s_bnr_href);
        m_bnr->o_vsync(s_bnr_vsync);
        m_bnr->o_raw(s_bnr_data);

        // AWB (statistics only - same input as BNR)
        m_awb->i_href(s_lsc_href);
        m_awb->i_vsync(s_lsc_vsync);
        m_awb->i_raw(s_lsc_data);
        m_awb->i_underexposed_limit(awb_underexp);
        m_awb->i_overexposed_limit(awb_overexp);
        m_awb->i_frames(awb_frames);
        m_awb->o_r_gain(awb_r_gain);
        m_awb->o_b_gain(awb_b_gain);

        // WB
        m_wb->i_href(s_bnr_href);
        m_wb->i_vsync(s_bnr_vsync);
        m_wb->i_data(s_bnr_data);
        m_wb->i_gain_r(wb_rgain);
        m_wb->i_gain_b(wb_bgain);
        m_wb->o_href(s_wb_href);
        m_wb->o_vsync(s_wb_vsync);
        m_wb->o_data(s_wb_data);

        // Demosaic
        m_demosaic->i_href(s_wb_href);
        m_demosaic->i_vsync(s_wb_vsync);
        m_demosaic->i_raw(s_wb_data);
        m_demosaic->o_href(s_demosaic_href);
        m_demosaic->o_vsync(s_demosaic_vsync);
        m_demosaic->o_r(s_demosaic_r);
        m_demosaic->o_g(s_demosaic_g);
        m_demosaic->o_b(s_demosaic_b);

        // CCM
        m_ccm->i_href(s_demosaic_href);
        m_ccm->i_vsync(s_demosaic_vsync);
        m_ccm->i_r(s_demosaic_r);
        m_ccm->i_g(s_demosaic_g);
        m_ccm->i_b(s_demosaic_b);
        m_ccm->i_m_rr(ccm_rr);
        m_ccm->i_m_rg(ccm_rg);
        m_ccm->i_m_rb(ccm_rb);
        m_ccm->i_m_gr(ccm_gr);
        m_ccm->i_m_gg(ccm_gg);
        m_ccm->i_m_gb(ccm_gb);
        m_ccm->i_m_br(ccm_br);
        m_ccm->i_m_bg(ccm_bg);
        m_ccm->i_m_bb(ccm_bb);
        m_ccm->o_href(s_ccm_href);
        m_ccm->o_vsync(s_ccm_vsync);
        m_ccm->o_r(s_ccm_r);
        m_ccm->o_g(s_ccm_g);
        m_ccm->o_b(s_ccm_b);

        // Gamma
        m_gamma->i_href(s_ccm_href);
        m_gamma->i_vsync(s_ccm_vsync);
        m_gamma->i_data_r(s_ccm_r);
        m_gamma->i_data_g(s_ccm_g);
        m_gamma->i_data_b(s_ccm_b);
        m_gamma->o_href(s_gamma_href);
        m_gamma->o_vsync(s_gamma_vsync);
        m_gamma->o_data_r(s_gamma_r);
        m_gamma->o_data_g(s_gamma_g);
        m_gamma->o_data_b(s_gamma_b);

        // AE (statistics only - same input as Gamma)
        m_ae->i_href(s_gamma_href);
        m_ae->i_vsync(s_gamma_vsync);
        m_ae->i_data(s_ccm_g);
        m_ae->i_center_illuminance(ae_center_illum);
        m_ae->i_ae_crop_left(ae_crop_left);
        m_ae->i_ae_crop_right(ae_crop_right);
        m_ae->i_ae_crop_top(ae_crop_top);
        m_ae->i_ae_crop_bottom(ae_crop_bottom);
        m_ae->i_skewness(ae_target_skewness);
        m_ae->o_ae_response(ae_response);
        m_ae->o_ae_result_skewness(ae_result_skewness);
        m_ae->o_ae_done(ae_done);

        // CSC
        m_csc->i_href(s_gamma_href);
        m_csc->i_vsync(s_gamma_vsync);
        m_csc->i_r(s_gamma_r);
        m_csc->i_g(s_gamma_g);
        m_csc->i_b(s_gamma_b);
        m_csc->i_conv_standard(csc_conv_standard);
        m_csc->o_href(s_csc_href);
        m_csc->o_vsync(s_csc_vsync);
        m_csc->o_y(s_csc_y);
        m_csc->o_u(s_csc_u);
        m_csc->o_v(s_csc_v);

        // LDCI (passthrough)
        m_ldci->i_href(s_csc_href);
        m_ldci->i_vsync(s_csc_vsync);
        m_ldci->i_data_y(s_csc_y);
        m_ldci->i_data_u(s_csc_u);
        m_ldci->i_data_v(s_csc_v);
        m_ldci->o_href(s_ldci_href);
        m_ldci->o_vsync(s_ldci_vsync);
        m_ldci->o_data_y(s_ldci_y);
        m_ldci->o_data_u(s_ldci_u);
        m_ldci->o_data_v(s_ldci_v);

        // Sharpen
        m_sharpen->i_href(s_ldci_href);
        m_sharpen->i_vsync(s_ldci_vsync);
        m_sharpen->i_data_y(s_ldci_y);
        m_sharpen->i_data_u(s_ldci_u);
        m_sharpen->i_data_v(s_ldci_v);
        m_sharpen->i_sharpen_strength(sharpen_strength);
        m_sharpen->o_href(s_sharpen_href);
        m_sharpen->o_vsync(s_sharpen_vsync);
        m_sharpen->o_data_y(s_sharpen_y);
        m_sharpen->o_data_u(s_sharpen_u);
        m_sharpen->o_data_v(s_sharpen_v);

        // 2DNR
        m_2dnr->i_href(s_sharpen_href);
        m_2dnr->i_vsync(s_sharpen_vsync);
        m_2dnr->i_data_y(s_sharpen_y);
        m_2dnr->i_data_u(s_sharpen_u);
        m_2dnr->i_data_v(s_sharpen_v);
        // Connect 2DNR output to top-level output signals
        m_2dnr->o_href(s_out_href);
        m_2dnr->o_vsync(s_out_vsync);
        m_2dnr->o_data_y(s_out_y);
        m_2dnr->o_data_u(s_out_u);
        m_2dnr->o_data_v(s_out_v);

        // Frame counting
        SC_METHOD(frame_counter);
        sensitive << pclk.pos();
    }

    void frame_counter() {
        if (!rst_n.read()) {
            m_frame_count = 0;
            m_prev_vsync = false;
            return;
        }

        bool curr_vsync = in_vsync.read();
        if (m_prev_vsync && !curr_vsync) {
            m_frame_count++;
        }
        m_prev_vsync = curr_vsync;
    }
};

using isp_top_10b_rggb = isp_top<10, BayerPattern::RGGB>;

#endif // ISP_TOP_H
