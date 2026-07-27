/*
 * ISP Pipeline Integration Testbench
 * Tests the complete ISP pipeline with comprehensive metrics collection
 *
 * Metrics collected:
 * - Timing: Total cycles, active cycles, throughput (FPS)
 * - Operations: Pixels processed, arithmetic operations
 * - Resource: LUT/FF estimates per block
 * - Power: Dynamic and static power estimates
 */

#include <systemc>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cmath>
#include <vector>

using namespace sc_core;
using namespace sc_dt;

//=============================================================================
// Types and Constants
//=============================================================================
typedef uint16_t isp_data_t;
typedef uint8_t isp_yuv_t;

const unsigned BITS = 10;
const unsigned DEFAULT_WIDTH = 64;
const unsigned DEFAULT_HEIGHT = 32;

//=============================================================================
// Include ISP Pipeline Components
//=============================================================================
#include "common/isp_types.h"
#include "blocks/blc/isp_blc.h"
#include "blocks/crop/isp_crop.h"
#include "blocks/wb/isp_wb.h"
#include "blocks/ccm/isp_ccm.h"
#include "blocks/demosaic/isp_demosaic.h"
#include "blocks/gamma/isp_gamma.h"
#include "blocks/sharpen/isp_sharpen.h"
#include "blocks/dpc/isp_dpc.h"
#include "blocks/bnr/isp_bnr.h"
#include "blocks/2dnr/isp_2dnr.h"
#include "blocks/awb/isp_awb.h"
#include "blocks/ae/isp_ae.h"
#include "blocks/oecf/isp_oecf.h"
#include "blocks/dgain/isp_dgain.h"
#include "blocks/lsc/isp_lsc.h"
#include "blocks/ldci/isp_ldci.h"
#include "blocks/csc/isp_csc.h"
#include "metrics/isp_metrics_base.h"

//=============================================================================
// Performance Metrics Structure
//=============================================================================
struct IspPerformanceMetrics {
    // Timing
    uint64_t total_cycles;
    uint64_t active_cycles;
    double throughput_fps;
    double pixels_per_cycle;

    // Operations
    uint64_t total_pixels;
    uint64_t frames_processed;

    // Per-block metrics
    std::map<std::string, BlockMetrics> block_metrics;

    void reset() {
        total_cycles = 0;
        active_cycles = 0;
        throughput_fps = 0.0;
        pixels_per_cycle = 0.0;
        total_pixels = 0;
        frames_processed = 0;
        block_metrics.clear();
    }

    void calculate(double clock_period_ns) {
        if (total_cycles > 0) {
            double total_time_ns = total_cycles * clock_period_ns;
            double total_time_s = total_time_ns / 1e9;
            throughput_fps = frames_processed / total_time_s;
            pixels_per_cycle = (double)total_pixels / total_cycles;
        }
    }
};

// BlockMetrics is defined in isp_metrics_base.h - no need to redefine

//=============================================================================
// ISP Pipeline Testbench
//=============================================================================
class IspPipelineTestbench : public sc_module {
public:
    // Configuration signals
    sc_signal<bool> crop_en{"crop_en"}, dpc_en{"dpc_en"}, blc_en{"blc_en"};
    sc_signal<bool> oecf_en{"oecf_en"}, dgain_en{"dgain_en"}, lsc_en{"lsc_en"};
    sc_signal<bool> bnr_en{"bnr_en"}, wb_en{"wb_en"}, demosaic_en{"demosaic_en"};
    sc_signal<bool> ccm_en{"ccm_en"}, gamma_en{"gamma_en"}, csc_en{"csc_en"};
    sc_signal<bool> ldci_en{"ldci_en"}, sharpen_en{"sharpen_en"}, nr2d_en{"nr2d_en"};
    sc_signal<bool> awb_en{"awb_en"}, ae_en{"ae_en"};

    // BLC parameters
    sc_signal<uint16_t> blc_r{"blc_r"}, blc_gr{"blc_gr"}, blc_gb{"blc_gb"}, blc_b{"blc_b"};
    sc_signal<bool> linear_en{"linear_en"};
    sc_signal<uint16_t> linear_r{"linear_r"}, linear_gr{"linear_gr"};
    sc_signal<uint16_t> linear_gb{"linear_gb"}, linear_b{"linear_b"};

    // DPC parameters
    sc_signal<uint16_t> dpc_threshold{"dpc_threshold"};

    // WB parameters
    sc_signal<uint16_t> wb_rgain{"wb_rgain"}, wb_bgain{"wb_bgain"};

    // CCM parameters
    sc_signal<int16_t> ccm_rr{"ccm_rr"}, ccm_rg{"ccm_rg"}, ccm_rb{"ccm_rb"};
    sc_signal<int16_t> ccm_gr{"ccm_gr"}, ccm_gg{"ccm_gg"}, ccm_gb{"ccm_gb"};
    sc_signal<int16_t> ccm_br{"ccm_br"}, ccm_bg{"ccm_bg"}, ccm_bb{"ccm_bb"};

    // CSC parameters
    sc_signal<uint8_t> csc_conv_std{"csc_conv_std"};

    // Sharpen parameters
    sc_signal<uint16_t> sharpen_strength{"sharpen_strength"};

    // AWB/AE parameters
    sc_signal<uint16_t> awb_underexp{"awb_underexp"}, awb_overexp{"awb_overexp"};
    sc_signal<uint8_t> awb_frames{"awb_frames"};
    sc_signal<uint16_t> awb_r_gain{"awb_r_gain"}, awb_b_gain{"awb_b_gain"};
    sc_signal<uint8_t> ae_center_illum{"ae_center_illum"};
    sc_signal<uint16_t> ae_crop_left{"ae_crop_left"}, ae_crop_right{"ae_crop_right"};
    sc_signal<uint16_t> ae_crop_top{"ae_crop_top"}, ae_crop_bottom{"ae_crop_bottom"};
    sc_signal<uint16_t> ae_target_skewness{"ae_target_skewness"};

    // DGain parameters
    sc_signal<bool> dgain_manual{"dgain_manual"};
    sc_signal<uint8_t> dgain_manual_index{"dgain_manual_index"};
    sc_signal<uint8_t> dgain_ae_index{"dgain_ae_index"};
    sc_signal<uint8_t> dgain_array[100];

    // CROP parameters (used in bind_pipeline)
    sc_signal<uint16_t> s_crop_w{"s_crop_w"}, s_crop_h{"s_crop_h"};
    sc_signal<uint16_t> s_crop_x{"s_crop_x"}, s_crop_y{"s_crop_y"};

    // AE output monitoring
    sc_signal<bool> s_ae_done{"s_ae_done"};
    sc_signal<uint16_t> s_ae_result_skewness{"s_ae_result_skewness"};
    sc_signal<int8_t> s_ae_response{"s_ae_response"};

    // AWB output monitoring
    sc_signal<uint16_t> s_awb_r_gain{"s_awb_r_gain"}, s_awb_b_gain{"s_awb_b_gain"};

    // DGain output
    sc_signal<uint8_t> s_dgain_applied_index{"s_dgain_applied_index"};

    // Output tready signal (from testbench side)
    sc_signal<bool> s_o_tready{"s_o_tready"};

    // Input data signal (drive from testbench)
    sc_signal<isp_data_t> s_tdata{"s_tdata"};
    sc_signal<bool> s_tvalid{"s_tvalid"};
    sc_signal<bool> s_tready{"s_tready"};
    sc_signal<bool> s_done{"s_done"};
    sc_signal<bool> s_start{"s_start"};

    // Internal signals
    sc_signal<bool> s_pclk{"s_pclk"};
    sc_signal<bool> s_rstn{"s_rstn"};
    sc_signal<bool> s_href{"s_href"}, s_vsync{"s_vsync"};
    sc_signal<isp_data_t> s_raw{"s_raw"};
    sc_signal<bool> mid_href{"mid_href"}, mid_vsync{"mid_vsync"};
    sc_signal<isp_data_t> mid_raw{"mid_raw"};

    // Output signals (from 2DNR)
    sc_signal<bool> s_out_href{"s_out_href"}, s_out_vsync{"s_out_vsync"};
    sc_signal<uint8_t> s_out_y{"s_out_y"}, s_out_u{"s_out_u"}, s_out_v{"s_out_v"};

    // Pipeline inter-block signals
    sc_signal<bool> s_crop_href{"s_crop_href"}, s_crop_vsync{"s_crop_vsync"};
    sc_signal<isp_data_t> s_crop_data{"s_crop_data"};
    sc_signal<bool> s_dpc_href{"s_dpc_href"}, s_dpc_vsync{"s_dpc_vsync"};
    sc_signal<isp_data_t> s_dpc_data{"s_dpc_data"};
    sc_signal<bool> s_blc_href{"s_blc_href"}, s_blc_vsync{"s_blc_vsync"};
    sc_signal<isp_data_t> s_blc_data{"s_blc_data"};
    sc_signal<bool> s_oecf_href{"s_oecf_href"}, s_oecf_vsync{"s_oecf_vsync"};
    sc_signal<isp_data_t> s_oecf_data{"s_oecf_data"};
    sc_signal<bool> s_dgain_href{"s_dgain_href"}, s_dgain_vsync{"s_dgain_vsync"};
    sc_signal<isp_data_t> s_dgain_data{"s_dgain_data"};
    sc_signal<bool> s_lsc_href{"s_lsc_href"}, s_lsc_vsync{"s_lsc_vsync"};
    sc_signal<isp_data_t> s_lsc_data{"s_lsc_data"};
    sc_signal<bool> s_bnr_href{"s_bnr_href"}, s_bnr_vsync{"s_bnr_vsync"};
    sc_signal<isp_data_t> s_bnr_data{"s_bnr_data"};
    sc_signal<bool> s_wb_href{"s_wb_href"}, s_wb_vsync{"s_wb_vsync"};
    sc_signal<isp_data_t> s_wb_data{"s_wb_data"};

    // DEMOSAIC RGB output
    sc_signal<bool> s_dem_href{"s_dem_href"}, s_dem_vsync{"s_dem_vsync"};
    sc_signal<isp_data_t> s_dem_r{"s_dem_r"}, s_dem_g{"s_dem_g"}, s_dem_b{"s_dem_b"};

    // CCM output
    sc_signal<bool> s_ccm_href{"s_ccm_href"}, s_ccm_vsync{"s_ccm_vsync"};
    sc_signal<isp_data_t> s_ccm_r{"s_ccm_r"}, s_ccm_g{"s_ccm_g"}, s_ccm_b{"s_ccm_b"};

    // GAMMA output
    sc_signal<bool> s_gamma_href{"s_gamma_href"}, s_gamma_vsync{"s_gamma_vsync"};
    sc_signal<isp_data_t> s_gamma_r{"s_gamma_r"}, s_gamma_g{"s_gamma_g"}, s_gamma_b{"s_gamma_b"};

    // CSC output
    sc_signal<bool> s_csc_href{"s_csc_href"}, s_csc_vsync{"s_csc_vsync"};
    sc_signal<uint8_t> s_csc_y{"s_csc_y"}, s_csc_u{"s_csc_u"}, s_csc_v{"s_csc_v"};

    // LDCI output
    sc_signal<bool> s_ldci_href{"s_ldci_href"}, s_ldci_vsync{"s_ldci_vsync"};
    sc_signal<uint8_t> s_ldci_y{"s_ldci_y"}, s_ldci_u{"s_ldci_u"}, s_ldci_v{"s_ldci_v"};

    // SHARPEN output
    sc_signal<bool> s_sharp_href{"s_sharp_href"}, s_sharp_vsync{"s_sharp_vsync"};
    sc_signal<uint8_t> s_sharp_y{"s_sharp_y"}, s_sharp_u{"s_sharp_u"}, s_sharp_v{"s_sharp_v"};

    // DUT instantiation
    isp_crop<BITS>* crop;  // dùng WIDTH=2048, HEIGHT=1536 mặc định
    isp_dpc<BITS, BayerPattern::RGGB>* dpc;
    isp_blc<BITS>* blc;
    isp_oecf<BITS, BayerPattern::RGGB>* oecf;
    isp_dgain<BITS>* dgain;
    isp_lsc<BITS>* lsc;
    isp_bnr<BITS, BayerPattern::RGGB>* bnr;
    isp_wb<BITS, BayerPattern::RGGB>* wb;
    isp_demosaic<BITS, BayerPattern::RGGB>* demosaic;
    isp_ccm<BITS>* ccm;
    isp_gamma<BITS>* gamma;
    isp_csc<BITS>* csc;
    isp_ldci* ldci;
    isp_sharpen* sharpen;
    isp_2dnr* nr2d;
    isp_awb<BITS, BayerPattern::RGGB>* awb;
    isp_ae<BITS>* ae;

    // Metrics
    IspPerformanceMetrics metrics;
    sc_time sim_start_time;
    sc_time sim_end_time;

    unsigned test_width;
    unsigned test_height;
    unsigned test_frames;

    // Real image buffer (loaded from .raw file)
    std::vector<isp_data_t> raw_image_buffer;  // size = test_width * test_height
    bool use_real_image = false;

    // Pixel counters for blocks without MetricsCollector (DGAIN, LSC, LDCI)
    uint64_t dgain_pixel_count = 0;
    uint64_t lsc_pixel_count = 0;
    uint64_t ldci_pixel_count = 0;

    IspPipelineTestbench(const sc_module_name& name)
        : sc_module(name)
        , test_width(DEFAULT_WIDTH)
        , test_height(DEFAULT_HEIGHT)
        , test_frames(2)
    {
        // Instantiate all blocks
        crop = new isp_crop<BITS>("crop");  // dùng WIDTH=2048, HEIGHT=1536 mặc định
        dpc = new isp_dpc<BITS, BayerPattern::RGGB>("dpc");
        blc = new isp_blc<BITS>("blc");
        oecf = new isp_oecf<BITS, BayerPattern::RGGB>("oecf");
        dgain = new isp_dgain<BITS>("dgain");
        lsc = new isp_lsc<BITS>("lsc");
        bnr = new isp_bnr<BITS, BayerPattern::RGGB>("bnr");
        wb = new isp_wb<BITS, BayerPattern::RGGB>("wb");
        demosaic = new isp_demosaic<BITS, BayerPattern::RGGB>("demosaic");
        ccm = new isp_ccm<BITS>("ccm");
        gamma = new isp_gamma<BITS>("gamma");
        csc = new isp_csc<BITS>("csc");
        ldci = new isp_ldci("ldci");
        sharpen = new isp_sharpen("sharpen");
        nr2d = new isp_2dnr("nr2d");
        awb = new isp_awb<BITS, BayerPattern::RGGB>("awb");
        ae = new isp_ae<BITS>("ae");

        // Connect clock and reset to all blocks
        bind_clock_reset();

        // Connect enable signals
        bind_enables();

        // Connect parameters
        bind_parameters();

        // Connect pipeline
        bind_pipeline();

        // Register processes
        SC_THREAD(clk_gen);
        SC_THREAD(test_process);
        SC_THREAD(dgain_counter_thread);
        SC_THREAD(lsc_counter_thread);
        SC_THREAD(ldci_counter_thread);
    }

    void set_test_config(unsigned width, unsigned height, unsigned frames) {
        test_width = width;
        test_height = height;
        test_frames = frames;
    }

    // Public API for raw image loading (called from sc_main before sc_start)
    bool load_raw_image_public(const std::string& filename) {
        return load_raw_image(filename);
    }

    // Propagate image size to all block metrics collectors for correct reporting
    void propagate_image_size() {
        crop->set_image_size(test_width, test_height);
        dpc->set_image_size(test_width, test_height);
        blc->set_image_size(test_width, test_height);
        oecf->set_image_size(test_width, test_height);
        bnr->set_image_size(test_width, test_height);
        wb->set_image_size(test_width, test_height);
        demosaic->set_image_size(test_width, test_height);
        ccm->set_image_size(test_width, test_height);
        gamma->set_image_size(test_width, test_height);
        csc->set_image_size(test_width, test_height);
        sharpen->set_image_size(test_width, test_height);
        nr2d->set_image_size(test_width, test_height);
        awb->set_image_size(test_width, test_height);
        ae->set_image_size(test_width, test_height);
        // Note: DGAIN, LSC, LDCI do not have MetricsCollector (signal-monitored only)
    }

private:
    void bind_clock_reset() {
        // Bind clock and reset to all blocks
        // (For testing simplicity, all blocks share same signals)
        crop->pclk(s_pclk);
        crop->rst_n(s_rstn);
        dpc->pclk(s_pclk);
        dpc->rst_n(s_rstn);
        blc->pclk(s_pclk);
        blc->rst_n(s_rstn);
        oecf->pclk(s_pclk);
        oecf->rst_n(s_rstn);
        dgain->pclk(s_pclk);
        dgain->rst_n(s_rstn);
        lsc->pclk(s_pclk);
        lsc->rst_n(s_rstn);
        bnr->pclk(s_pclk);
        bnr->rst_n(s_rstn);
        wb->pclk(s_pclk);
        wb->rst_n(s_rstn);
        demosaic->pclk(s_pclk);
        demosaic->rst_n(s_rstn);
        ccm->pclk(s_pclk);
        ccm->rst_n(s_rstn);
        gamma->pclk(s_pclk);
        gamma->rst_n(s_rstn);
        csc->pclk(s_pclk);
        csc->rst_n(s_rstn);
        ldci->pclk(s_pclk);
        ldci->rst_n(s_rstn);
        sharpen->pclk(s_pclk);
        sharpen->rst_n(s_rstn);
        nr2d->pclk(s_pclk);
        nr2d->rst_n(s_rstn);
        awb->pclk(s_pclk);
        awb->rst_n(s_rstn);
        ae->pclk(s_pclk);
        ae->rst_n(s_rstn);
    }

    void bind_enables() {
        crop->enable(crop_en);
        dpc->enable(dpc_en);
        blc->enable(blc_en);
        oecf->enable(oecf_en);
        dgain->enable(dgain_en);
        lsc->enable(lsc_en);
        bnr->enable(bnr_en);
        wb->enable(wb_en);
        demosaic->enable(demosaic_en);
        ccm->enable(ccm_en);
        gamma->enable(gamma_en);
        csc->enable(csc_en);
        ldci->enable(ldci_en);
        sharpen->enable(sharpen_en);
        nr2d->enable(nr2d_en);
        awb->enable(awb_en);
        ae->enable(ae_en);
    }

    void bind_parameters() {
        // BLC
        blc->i_blc_r(blc_r);
        blc->i_blc_gr(blc_gr);
        blc->i_blc_gb(blc_gb);
        blc->i_blc_b(blc_b);
        blc->i_linear_en(linear_en);
        blc->i_linear_r(linear_r);
        blc->i_linear_gr(linear_gr);
        blc->i_linear_gb(linear_gb);
        blc->i_linear_b(linear_b);

        // DPC
        dpc->i_threshold(dpc_threshold);

        // WB
        wb->i_gain_r(wb_rgain);
        wb->i_gain_b(wb_bgain);

        // CCM
        ccm->i_m_rr(ccm_rr);
        ccm->i_m_rg(ccm_rg);
        ccm->i_m_rb(ccm_rb);
        ccm->i_m_gr(ccm_gr);
        ccm->i_m_gg(ccm_gg);
        ccm->i_m_gb(ccm_gb);
        ccm->i_m_br(ccm_br);
        ccm->i_m_bg(ccm_bg);
        ccm->i_m_bb(ccm_bb);

        // CSC
        csc->i_conv_standard(csc_conv_std);

        // Sharpen
        sharpen->i_sharpen_strength(sharpen_strength);

        // AWB
        awb->i_underexposed_limit(awb_underexp);
        awb->i_overexposed_limit(awb_overexp);
        awb->i_frames(awb_frames);

        // AE
        ae->i_center_illuminance(ae_center_illum);
        ae->i_ae_crop_left(ae_crop_left);
        ae->i_ae_crop_right(ae_crop_right);
        ae->i_ae_crop_top(ae_crop_top);
        ae->i_ae_crop_bottom(ae_crop_bottom);
        ae->i_skewness(ae_target_skewness);
        ae->o_ae_done(s_ae_done);
        ae->o_ae_result_skewness(s_ae_result_skewness);
        ae->o_ae_response(s_ae_response);

        // AWB outputs (monitored)
        awb->o_r_gain(s_awb_r_gain);
        awb->o_b_gain(s_awb_b_gain);

        // DGain
        dgain->i_is_manual(dgain_manual);
        dgain->i_manual_index(dgain_manual_index);
        dgain->i_ae_feedback_index(dgain_ae_index);
        for (unsigned i = 0; i < 100; i++) {
            dgain->i_dgain_array[i](dgain_array[i]);
        }
        dgain->o_applied_index(s_dgain_applied_index);
    }

    void bind_pipeline() {
        // Set CROP parameters (use test config dims so ảnh thật được pass through)
        s_crop_w.write(test_width);
        s_crop_h.write(test_height);
        s_crop_x.write(0);
        s_crop_y.write(0);

        // Pipeline: CROP -> DPC -> BLC -> OECF -> DGain -> LSC -> BNR -> WB
        // -> DEMOSAIC -> CCM -> GAMMA -> AE -> CSC -> LDCI -> SHARPEN -> 2DNR

        // CROP -> DPC
        crop->i_href(mid_href);
        crop->i_vsync(mid_vsync);
        crop->i_data(mid_raw);
        crop->i_crop_w(s_crop_w);
        crop->i_crop_h(s_crop_h);
        crop->i_crop_x(s_crop_x);
        crop->i_crop_y(s_crop_y);
        crop->o_href(s_crop_href);
        crop->o_vsync(s_crop_vsync);
        crop->o_data(s_crop_data);

        dpc->i_href(s_crop_href);
        dpc->i_vsync(s_crop_vsync);
        dpc->i_raw(s_crop_data);
        dpc->o_href(s_dpc_href);
        dpc->o_vsync(s_dpc_vsync);
        dpc->o_raw(s_dpc_data);

        // BLC
        blc->i_href(s_dpc_href);
        blc->i_vsync(s_dpc_vsync);
        blc->i_data(s_dpc_data);
        blc->o_href(s_blc_href);
        blc->o_vsync(s_blc_vsync);
        blc->o_data(s_blc_data);

        // OECF
        oecf->i_href(s_blc_href);
        oecf->i_vsync(s_blc_vsync);
        oecf->i_raw(s_blc_data);
        oecf->o_href(s_oecf_href);
        oecf->o_vsync(s_oecf_vsync);
        oecf->o_raw(s_oecf_data);

        // DGain
        dgain->i_href(s_oecf_href);
        dgain->i_vsync(s_oecf_vsync);
        dgain->i_raw(s_oecf_data);
        dgain->o_href(s_dgain_href);
        dgain->o_vsync(s_dgain_vsync);
        dgain->o_raw(s_dgain_data);

        // LSC
        lsc->i_href(s_dgain_href);
        lsc->i_vsync(s_dgain_vsync);
        lsc->i_raw(s_dgain_data);
        lsc->o_href(s_lsc_href);
        lsc->o_vsync(s_lsc_vsync);
        lsc->o_raw(s_lsc_data);

        // BNR
        bnr->i_href(s_lsc_href);
        bnr->i_vsync(s_lsc_vsync);
        bnr->i_raw(s_lsc_data);
        bnr->o_href(s_bnr_href);
        bnr->o_vsync(s_bnr_vsync);
        bnr->o_raw(s_bnr_data);

        // WB
        wb->i_href(s_bnr_href);
        wb->i_vsync(s_bnr_vsync);
        wb->i_data(s_bnr_data);
        wb->o_href(s_wb_href);
        wb->o_vsync(s_wb_vsync);
        wb->o_data(s_wb_data);

        // AWB (takes WB output)
        awb->i_href(s_wb_href);
        awb->i_vsync(s_wb_vsync);
        awb->i_raw(s_wb_data);

        // DEMOSAIC
        demosaic->i_href(s_wb_href);
        demosaic->i_vsync(s_wb_vsync);
        demosaic->i_raw(s_wb_data);
        demosaic->o_href(s_dem_href);
        demosaic->o_vsync(s_dem_vsync);
        demosaic->o_r(s_dem_r);
        demosaic->o_g(s_dem_g);
        demosaic->o_b(s_dem_b);

        // CCM
        ccm->i_href(s_dem_href);
        ccm->i_vsync(s_dem_vsync);
        ccm->i_r(s_dem_r);
        ccm->i_g(s_dem_g);
        ccm->i_b(s_dem_b);
        ccm->o_href(s_ccm_href);
        ccm->o_vsync(s_ccm_vsync);
        ccm->o_r(s_ccm_r);
        ccm->o_g(s_ccm_g);
        ccm->o_b(s_ccm_b);

        // GAMMA
        gamma->i_href(s_ccm_href);
        gamma->i_vsync(s_ccm_vsync);
        gamma->i_data_r(s_ccm_r);
        gamma->i_data_g(s_ccm_g);
        gamma->i_data_b(s_ccm_b);
        gamma->o_href(s_gamma_href);
        gamma->o_vsync(s_gamma_vsync);
        gamma->o_data_r(s_gamma_r);
        gamma->o_data_g(s_gamma_g);
        gamma->o_data_b(s_gamma_b);

        // AE (takes GAMMA G output)
        ae->i_href(s_gamma_href);
        ae->i_vsync(s_gamma_vsync);
        ae->i_data(s_gamma_g);

        // CSC
        csc->i_href(s_gamma_href);
        csc->i_vsync(s_gamma_vsync);
        csc->i_r(s_gamma_r);
        csc->i_g(s_gamma_g);
        csc->i_b(s_gamma_b);
        csc->o_href(s_csc_href);
        csc->o_vsync(s_csc_vsync);
        csc->o_y(s_csc_y);
        csc->o_u(s_csc_u);
        csc->o_v(s_csc_v);

        // LDCI
        ldci->i_href(s_csc_href);
        ldci->i_vsync(s_csc_vsync);
        ldci->i_data_y(s_csc_y);
        ldci->i_data_u(s_csc_u);
        ldci->i_data_v(s_csc_v);
        ldci->o_href(s_ldci_href);
        ldci->o_vsync(s_ldci_vsync);
        ldci->o_data_y(s_ldci_y);
        ldci->o_data_u(s_ldci_u);
        ldci->o_data_v(s_ldci_v);

        // SHARPEN
        sharpen->i_href(s_ldci_href);
        sharpen->i_vsync(s_ldci_vsync);
        sharpen->i_data_y(s_ldci_y);
        sharpen->i_data_u(s_ldci_u);
        sharpen->i_data_v(s_ldci_v);
        sharpen->o_href(s_sharp_href);
        sharpen->o_vsync(s_sharp_vsync);
        sharpen->o_data_y(s_sharp_y);
        sharpen->o_data_u(s_sharp_u);
        sharpen->o_data_v(s_sharp_v);

        // 2DNR
        nr2d->i_href(s_sharp_href);
        nr2d->i_vsync(s_sharp_vsync);
        nr2d->i_data_y(s_sharp_y);
        nr2d->i_data_u(s_sharp_u);
        nr2d->i_data_v(s_sharp_v);
        nr2d->o_href(s_out_href);
        nr2d->o_vsync(s_out_vsync);
        nr2d->o_data_y(s_out_y);
        nr2d->o_data_u(s_out_u);
        nr2d->o_data_v(s_out_v);

    }

    void test_process() {
        // Initialize
        s_rstn.write(false);

        // Initialize internal signals
        s_rstn.write(false);

        // Initialize mid signals (drive CROP input)
        mid_href.write(false);
        mid_vsync.write(true);
        mid_raw.write(0);

        // Enable all pipeline blocks (disable = passthrough)
        crop_en.write(true);
        dpc_en.write(true);
        blc_en.write(true);
        oecf_en.write(true);
        dgain_en.write(true);
        lsc_en.write(true);
        bnr_en.write(true);
        wb_en.write(true);
        demosaic_en.write(true);
        ccm_en.write(true);
        gamma_en.write(true);
        csc_en.write(true);
        ldci_en.write(true);
        sharpen_en.write(true);
        nr2d_en.write(true);
        awb_en.write(true);
        ae_en.write(true);
        std::cout << "[INIT] All 17 block enables set to true" << std::endl;

        // AWB parameters - exposure limits for 16-bit RAW data
        // Image data range: 4224 - 65472, mean = 10283.6
        awb_underexp.write(8000);    // Below 8000 = underexposed
        awb_overexp.write(60000);    // Above 60000 = overexposed
        awb_frames.write(1);         // Compute gains every frame

        // AE parameters
        ae_center_illum.write(128);  // Center-weighted illumination
        ae_target_skewness.write(0); // Target skewness
        ae_crop_left.write(0);
        ae_crop_right.write(test_width);
        ae_crop_top.write(0);
        ae_crop_bottom.write(test_height);

        // Wait for clock to start (clk_gen is a separate thread)
        wait(100, SC_NS);

        // Release reset
        s_rstn.write(true);
        wait(50, SC_NS);

        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "ISP PIPELINE INTEGRATION TEST" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Configuration: " << test_width << "x" << test_height
                  << ", " << test_frames << " frames" << std::endl;

        // Update CROP size (was set in constructor with DEFAULT dims)
        s_crop_w.write(test_width);
        s_crop_h.write(test_height);
        std::cout << "CROP window: " << test_width << "x" << test_height << std::endl;

        // Propagate image size to all block metrics collectors
        propagate_image_size();

        sim_start_time = sc_time_stamp();

        // Send test frames
        for (unsigned f = 0; f < test_frames; f++) {
            std::cout << "\nSending frame " << (f + 1) << "/" << test_frames << "..." << std::endl;
            send_frame();
        }

        // Wait for processing to complete
        wait(2000, SC_NS);

        sim_end_time = sc_time_stamp();

        // Calculate metrics
        calculate_metrics();

        // Print results
        print_metrics();

        // Stop simulation
        sc_stop();
    }

    void clk_gen() {
        // Clock generator (separate thread)
        while (true) {
            s_pclk.write(false);
            wait(5, SC_NS);
            s_pclk.write(true);
            wait(5, SC_NS);
        }
    }

    void dgain_counter_thread() {
        // Count pixels flowing out of DGAIN (no MetricsCollector for this block)
        // Use clock edge to sample the href signal reliably
        while (true) {
            wait(s_pclk.posedge_event());
            if (s_dgain_href.read()) {
                dgain_pixel_count++;
            }
        }
    }

    void lsc_counter_thread() {
        // Count pixels flowing out of LSC
        while (true) {
            wait(s_pclk.posedge_event());
            if (s_lsc_href.read()) {
                lsc_pixel_count++;
            }
        }
    }

    void ldci_counter_thread() {
        // Count pixels flowing out of LDCI
        while (true) {
            wait(s_pclk.posedge_event());
            if (s_ldci_href.read()) {
                ldci_pixel_count++;
            }
        }
    }

    void send_frame() {
#ifdef DEBUG_LOG
        std::cout << "  [t=" << sc_time_stamp() << "] send_frame() ENTER" << std::endl;
        std::cout.flush();
#endif

        // Frame start - vsync fall = bắt đầu frame mới
        mid_vsync.write(false);
#ifdef DEBUG_LOG
        std::cout << "  [t=" << sc_time_stamp() << "] mid_vsync=false (frame start)" << std::endl;
        std::cout.flush();
#endif
        wait(10, SC_NS);  // 1 clock cycle

        for (unsigned y = 0; y < test_height; y++) {
            mid_href.write(false);
            wait(10, SC_NS);

#ifdef DEBUG_LOG
            std::cout << "  [t=" << sc_time_stamp() << "] Line " << y
                      << "/" << test_height << " start" << std::endl;
            if (y < 3 || y == test_height - 1) std::cout.flush();
#endif

            for (unsigned x = 0; x < test_width; x++) {
                // Get pixel from real image or synthetic generator
                isp_data_t pixel = get_pixel(x, y);
                mid_raw.write(pixel);
                mid_href.write(true);
                wait(10, SC_NS);

#ifdef DEBUG_LOG
                // Debug: stall detection
                if (y == 0 && x == 0) {
                    std::cout << "  [t=" << sc_time_stamp() << "] First pixel sent OK" << std::endl;
                    std::cout.flush();
                }
                if (x == 0) {
                    std::cout << "  [t=" << sc_time_stamp() << "] Line " << y
                              << " pixel 0 done" << std::endl;
                    if (y < 3) std::cout.flush();
                }
#endif
            }
            mid_href.write(false);
#ifdef DEBUG_LOG
            std::cout << "  [t=" << sc_time_stamp() << "] Line " << y
                      << " done (" << test_width << " pixels)" << std::endl;
            if (y < 3 || y == test_height - 1) std::cout.flush();
#endif
        }

        // Frame end
        mid_vsync.write(true);
#ifdef DEBUG_LOG
        std::cout << "  [t=" << sc_time_stamp() << "] mid_vsync=true (frame end)" << std::endl;
        std::cout.flush();
#endif
        wait(50, SC_NS);
#ifdef DEBUG_LOG
        std::cout << "  [t=" << sc_time_stamp() << "] send_frame() EXIT" << std::endl;
        std::cout.flush();
#endif
    }

    isp_data_t generate_bayer_pixel(unsigned x, unsigned y) {
        bool odd_y = (y & 1);
        bool odd_x = (x & 1);

        // Generate a test pattern (gradient with noise)
        uint16_t base = 256 + ((x + y) % 256);
        uint16_t noise = (rand() % 32);

        if (odd_y) {
            return odd_x ? (base + 128 + noise) : base;
        } else {
            return odd_x ? base : (base + 128 + noise);
        }
    }

    bool load_raw_image(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[ERROR] Cannot open RAW file: " << filename << std::endl;
            return false;
        }

        // Get file size
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        size_t expected_pixels = (size_t)test_width * test_height;
        size_t expected_bytes = expected_pixels * sizeof(uint16_t);

        if (file_size != expected_bytes) {
            std::cerr << "[ERROR] File size mismatch: " << file_size
                      << " bytes, expected " << expected_bytes
                      << " (" << test_width << "x" << test_height
                      << " * 2 bytes)" << std::endl;
            std::cerr << "[INFO] Try: -w <actual_width> -h <actual_height>" << std::endl;
            file.close();
            return false;
        }

        raw_image_buffer.resize(expected_pixels);
        file.read(reinterpret_cast<char*>(raw_image_buffer.data()), expected_bytes);
        file.close();

        if (file.gcount() != (std::streamsize)expected_bytes) {
            std::cerr << "[ERROR] Read short: got " << file.gcount()
                      << " bytes, expected " << expected_bytes << std::endl;
            return false;
        }

        use_real_image = true;
        std::cout << "[LOAD] RAW image loaded: " << filename << std::endl;
        std::cout << "       Size: " << test_width << "x" << test_height
                  << " (" << expected_pixels << " pixels, "
                  << (file_size / 1024) << " KB)" << std::endl;

        // Print statistics
        uint32_t min_v = 65535, max_v = 0;
        uint64_t sum = 0;
        for (auto px : raw_image_buffer) {
            if (px < min_v) min_v = px;
            if (px > max_v) max_v = px;
            sum += px;
        }
        double mean = (double)sum / expected_pixels;
        std::cout << "       Pixel range: " << min_v << " - " << max_v
                  << ", mean=" << std::fixed << std::setprecision(1) << mean << std::endl;
        return true;
    }

    isp_data_t get_pixel(unsigned x, unsigned y) {
        if (use_real_image && !raw_image_buffer.empty()) {
            return raw_image_buffer[y * test_width + x];
        }
        return generate_bayer_pixel(x, y);
    }

    void calculate_metrics() {
        sc_time elapsed = sim_end_time - sim_start_time;
        double elapsed_ns = elapsed.to_double();  // Already in ns (default resolution)
        double clock_period_ns = 10.0;  // 100 MHz
        double clock_freq_mhz = 1000.0 / clock_period_ns;  // 100 MHz

        metrics.total_cycles = (uint64_t)(elapsed_ns / clock_period_ns);
        metrics.total_pixels = (uint64_t)test_width * test_height * test_frames;
        metrics.frames_processed = test_frames;
        metrics.active_cycles = metrics.total_cycles;

        // FPS = clock_freq / pixels_per_frame (peak throughput if 100% utilization)
        double pixels_per_frame = (double)test_width * test_height;
        double peak_fps = clock_freq_mhz * 1e6 / pixels_per_frame;
        metrics.throughput_fps = peak_fps;

        if (metrics.total_cycles > 0) {
            metrics.pixels_per_cycle = (double)metrics.total_pixels / metrics.total_cycles;
        }
    }

    void print_metrics() {
        double clock_period_ns = 10.0;
        sc_time elapsed = sim_end_time - sim_start_time;
        double elapsed_ns = elapsed.to_double();  // Already in ns (default resolution)

        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "PERFORMANCE METRICS" << std::endl;
        std::cout << std::string(70, '=') << std::endl;

        std::cout << "\n[TIMING]" << std::endl;
        std::cout << "  Total Cycles:        " << std::setw(12) << metrics.total_cycles << std::endl;
        std::cout << "  Active Cycles:       " << std::setw(12) << metrics.active_cycles << std::endl;
        std::cout << "  Elapsed Time:        " << std::fixed << std::setprecision(3)
                  << elapsed_ns / 1e6 << " ms" << std::endl;
        std::cout << "  Utilization:         " << std::fixed << std::setprecision(2)
                  << (100.0 * metrics.active_cycles / metrics.total_cycles) << "%" << std::endl;

        std::cout << "\n[THROUGHPUT]" << std::endl;
        std::cout << "  Throughput:          " << std::fixed << std::setprecision(2)
                  << metrics.throughput_fps << " FPS" << std::endl;
        std::cout << "  Pixels/Cycle:        " << std::fixed << std::setprecision(4)
                  << metrics.pixels_per_cycle << std::endl;
        std::cout << "  Pixels Processed:    " << std::setw(12) << metrics.total_pixels << std::endl;

        std::cout << "\n[PER-BLOCK METRICS]" << std::endl;
        print_block_metrics();

        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "TEST COMPLETE" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
    }

    void print_block_metrics() {
        std::cout << std::endl;

        // ===== 14 blocks with MetricsCollector: use built-in print_summary() =====
        crop->get_metrics().print_summary();
        dpc->get_metrics().print_summary();
        blc->get_metrics().print_summary();
        oecf->get_metrics().print_summary();
        bnr->get_metrics().print_summary();
        wb->get_metrics().print_summary();
        demosaic->get_metrics().print_summary();
        ccm->get_metrics().print_summary();
        gamma->get_metrics().print_summary();
        csc->get_metrics().print_summary();
        sharpen->get_metrics().print_summary();
        nr2d->get_metrics().print_summary();
        awb->get_metrics().print_summary();
        ae->get_metrics().print_summary();

        // ===== 3 blocks without MetricsCollector: signal monitoring =====
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "  METRICS SUMMARY: DGAIN (signal-monitored)\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "  Pixels Processed:  " << dgain_pixel_count << "\n";
        std::cout << "  Applied Index:     " << (unsigned)s_dgain_applied_index.read()
                  << " (final value)\n";

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "  METRICS SUMMARY: LSC (signal-monitored)\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "  Pixels Processed:  " << lsc_pixel_count << "\n";

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "  METRICS SUMMARY: LDCI (signal-monitored)\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "  Pixels Processed:  " << ldci_pixel_count << "\n";
    }

    void save_metrics_to_file(const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Warning: Could not open " << filename << " for writing" << std::endl;
            return;
        }

        file << "{\n";
        file << "  \"test_config\": {\n";
        file << "    \"width\": " << test_width << ",\n";
        file << "    \"height\": " << test_height << ",\n";
        file << "    \"frames\": " << test_frames << "\n";
        file << "  },\n";
        file << "  \"metrics\": {\n";
        file << "    \"total_cycles\": " << metrics.total_cycles << ",\n";
        file << "    \"active_cycles\": " << metrics.active_cycles << ",\n";
        file << "    \"total_pixels\": " << metrics.total_pixels << ",\n";
        file << "    \"frames_processed\": " << metrics.frames_processed << ",\n";
        file << "    \"throughput_fps\": " << std::fixed << std::setprecision(2) << metrics.throughput_fps << ",\n";
        file << "    \"pixels_per_cycle\": " << std::fixed << std::setprecision(4) << metrics.pixels_per_cycle << "\n";
        file << "  }\n";
        file << "}\n";

        file.close();
        std::cout << "\nMetrics saved to: " << filename << std::endl;
    }
};

//=============================================================================
// Main
//=============================================================================
int sc_main(int argc, char* argv[]) {
    std::cout << "\n";
    std::cout << "************************************************************\n";
    std::cout << "* ISP Pipeline TLM Simulation                                *\n";
    std::cout << "* SystemC " << SC_VERSION << "                              *\n";
    std::cout << "************************************************************\n";

    // Parse arguments
    unsigned width = DEFAULT_WIDTH;
    unsigned height = DEFAULT_HEIGHT;
    unsigned frames = 2;
    std::string raw_file = "";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-w" && i + 1 < argc) {
            width = std::atoi(argv[++i]);
        } else if (arg == "-h" && i + 1 < argc) {
            height = std::atoi(argv[++i]);
        } else if (arg == "-f" && i + 1 < argc) {
            frames = std::atoi(argv[++i]);
        } else if (arg == "-i" && i + 1 < argc) {
            raw_file = argv[++i];
        } else if (arg == "--help" || arg == "-help") {
            std::cout << "\nUsage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  -w <width>    Image width (default: " << DEFAULT_WIDTH << ")\n";
            std::cout << "  -h <height>   Image height (default: " << DEFAULT_HEIGHT << ")\n";
            std::cout << "  -f <frames>   Number of frames (default: 2)\n";
            std::cout << "  -i <raw_file> Load 16-bit Bayer RAW image from file\n";
            std::cout << "                (when -i is used, -w/-h should match file dims)\n";
            std::cout << "  --help        Show this help\n\n";
            return 0;
        }
    }

    // Create module hierarchy
    IspPipelineTestbench tb("tb");

    // Configure test
    tb.set_test_config(width, height, frames);

    // Load RAW image if specified
    if (!raw_file.empty()) {
        if (!tb.load_raw_image_public(raw_file)) {
            std::cerr << "[ERROR] Failed to load RAW image, abort.\n";
            return 1;
        }
    }

    // Run simulation
    std::cout << "\nStarting simulation with " << width << "x" << height
              << " resolution, " << frames << " frame(s)...\n\n";

    sc_start();

    std::cout << "\nSimulation finished.\n";

    return 0;
}
