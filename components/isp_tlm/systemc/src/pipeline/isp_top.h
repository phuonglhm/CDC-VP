/*
 * Register-driven Infinite-ISP SystemC integration boundary.
 *
 * The public surface contains only architectural interfaces: sensor/direct-RGB
 * video, final I420 video, clock/reset, interrupt, AXI4-Lite control, fast TLM
 * control, and the ISP-owned physical-memory initiator. All tuning controls are
 * private, frame-latched signals sourced from one shared register bank.
 */

#ifndef ISP_TOP_H
#define ISP_TOP_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <utility>
#include <vector>

#include "axi/isp_axi_lite_adapter.h"
#include "blocks/2dnr/isp_2dnr.h"
#include "blocks/ae/isp_ae.h"
#include "blocks/awb/isp_awb.h"
#include "blocks/blc/isp_blc.h"
#include "blocks/bnr/isp_bnr.h"
#include "blocks/ccm/isp_ccm.h"
#include "blocks/crop/isp_crop.h"
#include "blocks/csc/isp_csc.h"
#include "blocks/demosaic/isp_demosaic.h"
#include "blocks/dgain/isp_dgain.h"
#include "blocks/dpc/isp_dpc.h"
#include "blocks/gamma/isp_gamma.h"
#include "blocks/ldci/isp_ldci.h"
#include "blocks/lsc/isp_lsc.h"
#include "blocks/oecf/isp_oecf.h"
#include "blocks/sharpen/isp_sharpen.h"
#include "blocks/wb/isp_wb.h"
#include "common/isp_types.h"
#include "memory/isp_frame_memory.h"
#include "registers/isp_register_bank.h"
#include "registers/isp_register_map.h"
#include "tlm/isp_control_target.h"
#include "vip/vip.h"

struct IspPerformanceSnapshot {
    std::uint64_t pclk_cycles = 0;
    std::uint64_t frames_completed = 0;
    std::uint64_t last_frame_cycles = 0;
    sc_core::sc_time last_frame_time = sc_core::SC_ZERO_TIME;
    double last_frame_fps = 0.0;
};

template<unsigned int BITS = 12,
         BayerPattern BAYER = BayerPattern::RGGB,
         unsigned int WIDTH = 2592,
         unsigned int HEIGHT = 1536>
class isp_top : public sc_core::sc_module {
    static_assert(BITS >= 8 && BITS <= 12,
                  "the preserved ISP register/LUT map supports 8-12 bits");
    static_assert(WIDTH > 0 && HEIGHT > 0,
                  "isp_top dimensions must be non-zero");
    static_assert((WIDTH & 1U) == 0U && (HEIGHT & 1U) == 0U,
                  "fixed I420 output requires even width and height");
    static_assert(WIDTH <= 4095 && HEIGHT <= 4095,
                  "RTL-compatible dimension registers are 12 bits");

public:
    // ISP pixel clock/reset. rst_n asserts asynchronously.
    sc_core::sc_in<bool> pclk{"pclk"};
    sc_core::sc_in<bool> rst_n{"rst_n"};

    // RAW sensor stream.
    sc_core::sc_in<bool> in_href{"in_href"};
    sc_core::sc_in<bool> in_vsync{"in_vsync"};
    sc_core::sc_in<sc_dt::sc_uint<BITS>> in_raw{"in_raw"};

    // Direct RGB stream, selected frame-atomically by ISP_JOB_CONTROL[2].
    sc_core::sc_in<bool> in_href_rgb{"in_href_rgb"};
    sc_core::sc_in<bool> in_vsync_rgb{"in_vsync_rgb"};
    sc_core::sc_in<sc_dt::sc_uint<BITS>> in_r{"in_r"};
    sc_core::sc_in<sc_dt::sc_uint<BITS>> in_g{"in_g"};
    sc_core::sc_in<sc_dt::sc_uint<BITS>> in_b{"in_b"};

    // RGB observation point after gamma, retained from the RTL video
    // interface. It is not a tuning/configuration interface.
    sc_core::sc_out<bool> out_gamma_href{"out_gamma_href"};
    sc_core::sc_out<bool> out_gamma_vsync{"out_gamma_vsync"};
    sc_core::sc_out<sc_dt::sc_uint<BITS>> out_gamma_r{"out_gamma_r"};
    sc_core::sc_out<sc_dt::sc_uint<BITS>> out_gamma_g{"out_gamma_g"};
    sc_core::sc_out<sc_dt::sc_uint<BITS>> out_gamma_b{"out_gamma_b"};

    // Sole VIP output. Y is meaningful on every active pixel. U and V are
    // meaningful only at even-x/even-y samples and are zero otherwise.
    sc_core::sc_out<bool> out_href{"out_href"};
    sc_core::sc_out<bool> out_vsync{"out_vsync"};
    sc_core::sc_out<std::uint8_t> out_y{"out_y"};
    sc_core::sc_out<std::uint8_t> out_u{"out_u"};
    sc_core::sc_out<std::uint8_t> out_v{"out_v"};

    sc_core::sc_out<bool> irq{"irq"};

    // Independent AXI4-Lite control clock/reset. aresetn is active-low and
    // sampled synchronously on aclk.
    sc_core::sc_in<bool> aclk{"aclk"};
    sc_core::sc_in<bool> aresetn{"aresetn"};

    sc_core::sc_in<sc_dt::sc_uint<32>> s_axi_awaddr{"s_axi_awaddr"};
    sc_core::sc_in<sc_dt::sc_uint<3>> s_axi_awprot{"s_axi_awprot"};
    sc_core::sc_in<bool> s_axi_awvalid{"s_axi_awvalid"};
    sc_core::sc_out<bool> s_axi_awready{"s_axi_awready"};

    sc_core::sc_in<sc_dt::sc_uint<32>> s_axi_wdata{"s_axi_wdata"};
    sc_core::sc_in<sc_dt::sc_uint<4>> s_axi_wstrb{"s_axi_wstrb"};
    sc_core::sc_in<bool> s_axi_wvalid{"s_axi_wvalid"};
    sc_core::sc_out<bool> s_axi_wready{"s_axi_wready"};

    sc_core::sc_out<sc_dt::sc_uint<2>> s_axi_bresp{"s_axi_bresp"};
    sc_core::sc_out<bool> s_axi_bvalid{"s_axi_bvalid"};
    sc_core::sc_in<bool> s_axi_bready{"s_axi_bready"};

    sc_core::sc_in<sc_dt::sc_uint<32>> s_axi_araddr{"s_axi_araddr"};
    sc_core::sc_in<sc_dt::sc_uint<3>> s_axi_arprot{"s_axi_arprot"};
    sc_core::sc_in<bool> s_axi_arvalid{"s_axi_arvalid"};
    sc_core::sc_out<bool> s_axi_arready{"s_axi_arready"};

    sc_core::sc_out<sc_dt::sc_uint<32>> s_axi_rdata{"s_axi_rdata"};
    sc_core::sc_out<sc_dt::sc_uint<2>> s_axi_rresp{"s_axi_rresp"};
    sc_core::sc_out<bool> s_axi_rvalid{"s_axi_rvalid"};
    sc_core::sc_in<bool> s_axi_rready{"s_axi_rready"};

private:
    isp_tlm::registers::IspRegisterBank register_bank_;
    isp_tlm::tlm_frontend::IspControlTarget control_frontend_;
    isp_tlm::axi::IspAxiLiteAdapter axi_frontend_;
    cdc::components::isp::IspFrameMemory frame_memory_;

public:
    // Hierarchical socket aliases are the public transaction interfaces.
    tlm_utils::simple_target_socket<
        isp_tlm::tlm_frontend::IspControlTarget>& control_socket;
    tlm_utils::simple_initiator_socket<
        cdc::components::isp::IspFrameMemory>& memory_socket;

private:
    isp_crop<BITS, WIDTH, HEIGHT> crop_;
    isp_dpc<BITS, BAYER> dpc_;
    isp_blc<BITS> blc_;
    isp_oecf<BITS, BAYER> oecf_;
    isp_dgain<BITS> dgain_;
    isp_lsc<BITS> lsc_;
    isp_bnr<BITS, BAYER> bnr_;
    isp_wb<BITS, BAYER> wb_;
    isp_demosaic<BITS, BAYER> demosaic_;
    isp_ccm<BITS> ccm_;
    isp_gamma<BITS> gamma_;
    isp_csc<BITS> csc_;
    isp_ldci ldci_;
    isp_sharpen sharpen_;
    isp_2dnr nr2d_;
    isp_awb<BITS, BAYER> awb_;
    isp_ae<BITS> ae_;
    VIP<8> vip_;

public:
    SC_HAS_PROCESS(isp_top);

    explicit isp_top(const sc_core::sc_module_name& name,
                     sc_core::sc_time control_latency =
                         sc_core::SC_ZERO_TIME)
        : sc_core::sc_module(name)
        , register_bank_(make_build_configuration())
        , control_frontend_("control", register_bank_, control_latency)
        , axi_frontend_("axi", register_bank_)
        , frame_memory_("frame_memory")
        , control_socket(control_frontend_.socket)
        , memory_socket(frame_memory_.memory_socket)
        , crop_("crop")
        , dpc_("dpc")
        , blc_("blc")
        , oecf_("oecf")
        , dgain_("dgain")
        , lsc_("lsc")
        , bnr_("bnr")
        , wb_("wb")
        , demosaic_("demosaic")
        , ccm_("ccm")
        , gamma_("gamma")
        , csc_("csc")
        , ldci_("ldci")
        , sharpen_("sharpen")
        , nr2d_("2dnr")
        , awb_("awb")
        , ae_("ae")
        , vip_("VIP")
    {
        bind_axi();
        bind_pipeline();
        set_metric_dimensions();

        SC_METHOD(source_mux);
        sensitive << rst_n << s_memory_select_ << s_memory_href_
                  << s_memory_vsync_ << s_memory_raw_
                  << in_href << in_vsync << in_raw;

        SC_METHOD(ccm_input_mux);
        sensitive << rst_n << s_memory_select_ << s_direct_rgb_active_
                  << s_demosaic_href_ << s_demosaic_vsync_
                  << s_demosaic_r_ << s_demosaic_g_ << s_demosaic_b_
                  << in_href_rgb << in_vsync_rgb << in_r << in_g << in_b;

        SC_METHOD(vip_input_bridge);
        sensitive << s_nr2d_y_ << s_nr2d_u_ << s_nr2d_v_;

        SC_METHOD(video_output_bridge);
        sensitive << rst_n << s_vip_href_ << s_vip_vsync_
                  << s_vip_y_ << s_vip_u_ << s_vip_v_
                  << s_gamma_href_ << s_gamma_vsync_
                  << s_gamma_r_ << s_gamma_g_ << s_gamma_b_;

        SC_METHOD(irq_bridge);
        sensitive << pclk.pos() << aclk.pos() << rst_n.neg();
        dont_initialize();

        SC_THREAD(control_thread);
        sensitive << pclk.pos() << rst_n.neg();
        dont_initialize();

        SC_METHOD(output_monitor);
        sensitive << pclk.pos() << rst_n.neg();
        dont_initialize();

        SC_THREAD(job_worker);
    }

    IspPerformanceSnapshot performance() const { return performance_; }

    CropMetricsCollector& get_crop_metrics() { return crop_.get_metrics(); }
    BlcMetricsCollector& get_blc_metrics() { return blc_.get_metrics(); }
    DpcMetricsCollector& get_dpc_metrics() { return dpc_.get_metrics(); }
    OecfMetricsCollector& get_oecf_metrics() { return oecf_.get_metrics(); }
    WbMetricsCollector& get_wb_metrics() { return wb_.get_metrics(); }
    BnrMetricsCollector& get_bnr_metrics() { return bnr_.get_metrics(); }
    DemosaicMetricsCollector& get_demosaic_metrics() {
        return demosaic_.get_metrics();
    }
    CcmMetricsCollector& get_ccm_metrics() { return ccm_.get_metrics(); }
    GammaMetricsCollector& get_gamma_metrics() { return gamma_.get_metrics(); }
    CscMetricsCollector& get_csc_metrics() { return csc_.get_metrics(); }
    SharpenMetricsCollector& get_sharpen_metrics() {
        return sharpen_.get_metrics();
    }
    Nr2dMetricsCollector& get_nr2d_metrics() { return nr2d_.get_metrics(); }
    AwbMetricsCollector& get_awb_metrics() { return awb_.get_metrics(); }
    AeMetricsCollector& get_ae_metrics() { return ae_.get_metrics(); }

private:
    // RAW-chain signals.
    sc_core::sc_signal<bool> s_source_href_{"source_href"};
    sc_core::sc_signal<bool> s_source_vsync_{"source_vsync"};
    sc_core::sc_signal<std::uint16_t> s_source_raw_{"source_raw"};

    sc_core::sc_signal<bool> s_crop_href_{"crop_href"};
    sc_core::sc_signal<bool> s_crop_vsync_{"crop_vsync"};
    sc_core::sc_signal<std::uint16_t> s_crop_raw_{"crop_raw"};
    sc_core::sc_signal<bool> s_dpc_href_{"dpc_href"};
    sc_core::sc_signal<bool> s_dpc_vsync_{"dpc_vsync"};
    sc_core::sc_signal<std::uint16_t> s_dpc_raw_{"dpc_raw"};
    sc_core::sc_signal<bool> s_blc_href_{"blc_href"};
    sc_core::sc_signal<bool> s_blc_vsync_{"blc_vsync"};
    sc_core::sc_signal<std::uint16_t> s_blc_raw_{"blc_raw"};
    sc_core::sc_signal<bool> s_oecf_href_{"oecf_href"};
    sc_core::sc_signal<bool> s_oecf_vsync_{"oecf_vsync"};
    sc_core::sc_signal<std::uint16_t> s_oecf_raw_{"oecf_raw"};
    sc_core::sc_signal<bool> s_dgain_href_{"dgain_href"};
    sc_core::sc_signal<bool> s_dgain_vsync_{"dgain_vsync"};
    sc_core::sc_signal<std::uint16_t> s_dgain_raw_{"dgain_raw"};
    sc_core::sc_signal<bool> s_lsc_href_{"lsc_href"};
    sc_core::sc_signal<bool> s_lsc_vsync_{"lsc_vsync"};
    sc_core::sc_signal<std::uint16_t> s_lsc_raw_{"lsc_raw"};
    sc_core::sc_signal<bool> s_bnr_href_{"bnr_href"};
    sc_core::sc_signal<bool> s_bnr_vsync_{"bnr_vsync"};
    sc_core::sc_signal<std::uint16_t> s_bnr_raw_{"bnr_raw"};
    sc_core::sc_signal<bool> s_wb_href_{"wb_href"};
    sc_core::sc_signal<bool> s_wb_vsync_{"wb_vsync"};
    sc_core::sc_signal<std::uint16_t> s_wb_raw_{"wb_raw"};

    // RGB chain and direct-RGB mux.
    sc_core::sc_signal<bool> s_demosaic_href_{"demosaic_href"};
    sc_core::sc_signal<bool> s_demosaic_vsync_{"demosaic_vsync"};
    sc_core::sc_signal<std::uint16_t> s_demosaic_r_{"demosaic_r"};
    sc_core::sc_signal<std::uint16_t> s_demosaic_g_{"demosaic_g"};
    sc_core::sc_signal<std::uint16_t> s_demosaic_b_{"demosaic_b"};
    sc_core::sc_signal<bool> s_ccm_input_href_{"ccm_input_href"};
    sc_core::sc_signal<bool> s_ccm_input_vsync_{"ccm_input_vsync"};
    sc_core::sc_signal<std::uint16_t> s_ccm_input_r_{"ccm_input_r"};
    sc_core::sc_signal<std::uint16_t> s_ccm_input_g_{"ccm_input_g"};
    sc_core::sc_signal<std::uint16_t> s_ccm_input_b_{"ccm_input_b"};
    sc_core::sc_signal<bool> s_ccm_href_{"ccm_href"};
    sc_core::sc_signal<bool> s_ccm_vsync_{"ccm_vsync"};
    sc_core::sc_signal<std::uint16_t> s_ccm_r_{"ccm_r"};
    sc_core::sc_signal<std::uint16_t> s_ccm_g_{"ccm_g"};
    sc_core::sc_signal<std::uint16_t> s_ccm_b_{"ccm_b"};
    sc_core::sc_signal<bool> s_gamma_href_{"gamma_href"};
    sc_core::sc_signal<bool> s_gamma_vsync_{"gamma_vsync"};
    sc_core::sc_signal<std::uint16_t> s_gamma_r_{"gamma_r"};
    sc_core::sc_signal<std::uint16_t> s_gamma_g_{"gamma_g"};
    sc_core::sc_signal<std::uint16_t> s_gamma_b_{"gamma_b"};

    // YUV444 chain.
    sc_core::sc_signal<bool> s_csc_href_{"csc_href"};
    sc_core::sc_signal<bool> s_csc_vsync_{"csc_vsync"};
    sc_core::sc_signal<std::uint8_t> s_csc_y_{"csc_y"};
    sc_core::sc_signal<std::uint8_t> s_csc_u_{"csc_u"};
    sc_core::sc_signal<std::uint8_t> s_csc_v_{"csc_v"};
    sc_core::sc_signal<bool> s_ldci_href_{"ldci_href"};
    sc_core::sc_signal<bool> s_ldci_vsync_{"ldci_vsync"};
    sc_core::sc_signal<std::uint8_t> s_ldci_y_{"ldci_y"};
    sc_core::sc_signal<std::uint8_t> s_ldci_u_{"ldci_u"};
    sc_core::sc_signal<std::uint8_t> s_ldci_v_{"ldci_v"};
    sc_core::sc_signal<bool> s_sharpen_href_{"sharpen_href"};
    sc_core::sc_signal<bool> s_sharpen_vsync_{"sharpen_vsync"};
    sc_core::sc_signal<std::uint8_t> s_sharpen_y_{"sharpen_y"};
    sc_core::sc_signal<std::uint8_t> s_sharpen_u_{"sharpen_u"};
    sc_core::sc_signal<std::uint8_t> s_sharpen_v_{"sharpen_v"};
    sc_core::sc_signal<bool> s_nr2d_href_{"nr2d_href"};
    sc_core::sc_signal<bool> s_nr2d_vsync_{"nr2d_vsync"};
    sc_core::sc_signal<std::uint8_t> s_nr2d_y_{"nr2d_y"};
    sc_core::sc_signal<std::uint8_t> s_nr2d_u_{"nr2d_u"};
    sc_core::sc_signal<std::uint8_t> s_nr2d_v_{"nr2d_v"};

    // VIP bridge/output signals.
    sc_core::sc_signal<sc_dt::sc_uint<8>> s_vip_input_y_{"vip_input_y"};
    sc_core::sc_signal<sc_dt::sc_uint<8>> s_vip_input_u_{"vip_input_u"};
    sc_core::sc_signal<sc_dt::sc_uint<8>> s_vip_input_v_{"vip_input_v"};
    sc_core::sc_signal<bool> s_vip_href_{"vip_href"};
    sc_core::sc_signal<bool> s_vip_vsync_{"vip_vsync"};
    sc_core::sc_signal<std::uint8_t> s_vip_y_{"vip_y"};
    sc_core::sc_signal<std::uint8_t> s_vip_u_{"vip_u"};
    sc_core::sc_signal<std::uint8_t> s_vip_v_{"vip_v"};
    sc_core::sc_signal<bool> s_vip_error_{"vip_error"};

    // Memory-generated RAW stream.
    sc_core::sc_signal<bool> s_memory_select_{"memory_select"};
    sc_core::sc_signal<bool> s_memory_href_{"memory_href"};
    sc_core::sc_signal<bool> s_memory_vsync_{"memory_vsync"};
    sc_core::sc_signal<std::uint16_t> s_memory_raw_{"memory_raw"};

    // Private register-driven configuration signals.
    sc_core::sc_signal<bool> s_crop_enable_{"crop_enable"};
    sc_core::sc_signal<bool> s_dpc_enable_{"dpc_enable"};
    sc_core::sc_signal<bool> s_blc_enable_{"blc_enable"};
    sc_core::sc_signal<bool> s_linear_enable_{"linear_enable"};
    sc_core::sc_signal<bool> s_oecf_enable_{"oecf_enable"};
    sc_core::sc_signal<bool> s_dgain_enable_{"dgain_enable"};
    sc_core::sc_signal<bool> s_lsc_enable_{"lsc_enable"};
    sc_core::sc_signal<bool> s_bnr_enable_{"bnr_enable"};
    sc_core::sc_signal<bool> s_wb_enable_{"wb_enable"};
    sc_core::sc_signal<bool> s_demosaic_enable_{"demosaic_enable"};
    sc_core::sc_signal<bool> s_ccm_enable_{"ccm_enable"};
    sc_core::sc_signal<bool> s_gamma_enable_{"gamma_enable"};
    sc_core::sc_signal<bool> s_csc_enable_{"csc_enable"};
    sc_core::sc_signal<bool> s_ldci_enable_{"ldci_enable"};
    sc_core::sc_signal<bool> s_nr2d_enable_{"nr2d_enable"};
    sc_core::sc_signal<bool> s_sharpen_enable_{"sharpen_enable"};
    sc_core::sc_signal<bool> s_ae_enable_{"ae_enable"};
    sc_core::sc_signal<bool> s_awb_enable_{"awb_enable"};
    sc_core::sc_signal<bool> s_direct_rgb_active_{"direct_rgb_active"};
    sc_core::sc_signal<bool> s_vip_enable_{"vip_enable"};
    sc_core::sc_signal<bool> s_vip_input_is_yuv_{"vip_input_is_yuv"};

    sc_core::sc_signal<std::uint16_t> s_crop_width_{"crop_width"};
    sc_core::sc_signal<std::uint16_t> s_crop_height_{"crop_height"};
    sc_core::sc_signal<std::uint16_t> s_crop_x_{"crop_x"};
    sc_core::sc_signal<std::uint16_t> s_crop_y_{"crop_y"};
    sc_core::sc_signal<std::uint16_t> s_dpc_threshold_{"dpc_threshold"};
    sc_core::sc_signal<std::uint16_t> s_blc_r_{"blc_r"};
    sc_core::sc_signal<std::uint16_t> s_blc_gr_{"blc_gr"};
    sc_core::sc_signal<std::uint16_t> s_blc_gb_{"blc_gb"};
    sc_core::sc_signal<std::uint16_t> s_blc_b_{"blc_b"};
    sc_core::sc_signal<std::uint16_t> s_linear_r_{"linear_r"};
    sc_core::sc_signal<std::uint16_t> s_linear_gr_{"linear_gr"};
    sc_core::sc_signal<std::uint16_t> s_linear_gb_{"linear_gb"};
    sc_core::sc_signal<std::uint16_t> s_linear_b_{"linear_b"};
    sc_core::sc_signal<bool> s_dgain_manual_{"dgain_manual"};
    sc_core::sc_signal<std::uint8_t> s_dgain_manual_index_{
        "dgain_manual_index"};
    sc_core::sc_signal<std::uint8_t> s_dgain_ae_index_{"dgain_ae_index"};
    sc_core::sc_signal<std::uint8_t> s_dgain_applied_index_{
        "dgain_applied_index"};
    sc_core::sc_signal<std::uint8_t>
        s_dgain_array_[isp_dgain<BITS>::GAIN_ARRAY_SIZE];
    sc_core::sc_signal<std::uint16_t> s_wb_r_gain_{"wb_r_gain"};
    sc_core::sc_signal<std::uint16_t> s_wb_b_gain_{"wb_b_gain"};
    sc_core::sc_signal<std::int16_t> s_ccm_rr_{"ccm_rr"};
    sc_core::sc_signal<std::int16_t> s_ccm_rg_{"ccm_rg"};
    sc_core::sc_signal<std::int16_t> s_ccm_rb_{"ccm_rb"};
    sc_core::sc_signal<std::int16_t> s_ccm_gr_{"ccm_gr"};
    sc_core::sc_signal<std::int16_t> s_ccm_gg_{"ccm_gg"};
    sc_core::sc_signal<std::int16_t> s_ccm_gb_{"ccm_gb"};
    sc_core::sc_signal<std::int16_t> s_ccm_br_{"ccm_br"};
    sc_core::sc_signal<std::int16_t> s_ccm_bg_{"ccm_bg"};
    sc_core::sc_signal<std::int16_t> s_ccm_bb_{"ccm_bb"};
    sc_core::sc_signal<std::uint8_t> s_csc_standard_{"csc_standard"};
    sc_core::sc_signal<sc_dt::sc_uint<2>> s_vip_csc_standard_{
        "vip_csc_standard"};
    sc_core::sc_signal<std::uint16_t> s_sharpen_strength_{
        "sharpen_strength"};
    sc_core::sc_signal<std::uint16_t> s_awb_underexposed_{
        "awb_underexposed"};
    sc_core::sc_signal<std::uint16_t> s_awb_overexposed_{
        "awb_overexposed"};
    sc_core::sc_signal<std::uint8_t> s_awb_frames_{"awb_frames"};
    sc_core::sc_signal<std::uint16_t> s_awb_r_gain_{"awb_r_gain"};
    sc_core::sc_signal<std::uint16_t> s_awb_b_gain_{"awb_b_gain"};
    sc_core::sc_signal<std::uint8_t> s_ae_center_{"ae_center"};
    sc_core::sc_signal<std::uint16_t> s_ae_skewness_{"ae_skewness"};
    sc_core::sc_signal<std::uint16_t> s_ae_crop_left_{"ae_crop_left"};
    sc_core::sc_signal<std::uint16_t> s_ae_crop_right_{"ae_crop_right"};
    sc_core::sc_signal<std::uint16_t> s_ae_crop_top_{"ae_crop_top"};
    sc_core::sc_signal<std::uint16_t> s_ae_crop_bottom_{"ae_crop_bottom"};
    sc_core::sc_signal<std::int8_t> s_ae_response_{"ae_response"};
    sc_core::sc_signal<std::uint16_t> s_ae_result_skewness_{
        "ae_result_skewness"};
    sc_core::sc_signal<bool> s_ae_done_{"ae_done"};

    struct FrameTiming {
        std::uint64_t cycle = 0;
        sc_core::sc_time time = sc_core::SC_ZERO_TIME;
    };

    IspPerformanceSnapshot performance_;
    std::map<std::uint64_t, FrameTiming> input_frame_timing_;
    std::uint64_t input_sequence_ = 0;
    std::uint64_t output_sequence_ = 0;
    bool previous_selected_vsync_ = false;
    bool previous_output_vsync_ = false;
    bool previous_ae_done_ = false;
    bool active_direct_rgb_ = false;

    bool job_active_ = false;
    bool job_memory_mode_ = false;
    bool stream_waiting_for_frame_ = false;
    std::uint64_t job_target_sequence_ = 0;
    bool capture_active_ = false;
    bool capture_complete_ = false;
    bool capture_error_ = false;
    std::vector<std::uint8_t> capture_y_;
    std::vector<std::uint8_t> capture_u_;
    std::vector<std::uint8_t> capture_v_;
    cdc::components::isp::Raw12RggbLe16Descriptor source_descriptor_;
    cdc::components::isp::I420FrameDescriptor destination_descriptor_;
    sc_core::sc_event job_start_event_;
    sc_core::sc_event capture_complete_event_;

    static isp_tlm::registers::BuildConfiguration
    make_build_configuration()
    {
        isp_tlm::registers::BuildConfiguration configuration;
        configuration.sensor_width = WIDTH;
        configuration.sensor_height = HEIGHT;
        configuration.crop_width = WIDTH;
        configuration.crop_height = HEIGHT;
        configuration.bits = BITS;
        configuration.bayer = static_cast<std::uint32_t>(BAYER);
        configuration.output_width = WIDTH;
        configuration.output_height = HEIGHT;
        configuration.vip_bits = 8;
        return configuration;
    }

    static std::uint32_t narrow_counter(std::uint64_t value)
    {
        return value > std::numeric_limits<std::uint32_t>::max()
                   ? std::numeric_limits<std::uint32_t>::max()
                   : static_cast<std::uint32_t>(value);
    }

    std::uint32_t active_register(std::uint32_t address) const
    {
        std::uint32_t value = 0;
        const isp_tlm::registers::AccessResult result =
            register_bank_.read_active(address, value);
        return result == isp_tlm::registers::AccessResult::Ok ? value : 0;
    }

    void bind_axi()
    {
        axi_frontend_.aclk(aclk);
        axi_frontend_.aresetn(aresetn);
        axi_frontend_.awaddr(s_axi_awaddr);
        axi_frontend_.awprot(s_axi_awprot);
        axi_frontend_.awvalid(s_axi_awvalid);
        axi_frontend_.awready(s_axi_awready);
        axi_frontend_.wdata(s_axi_wdata);
        axi_frontend_.wstrb(s_axi_wstrb);
        axi_frontend_.wvalid(s_axi_wvalid);
        axi_frontend_.wready(s_axi_wready);
        axi_frontend_.bresp(s_axi_bresp);
        axi_frontend_.bvalid(s_axi_bvalid);
        axi_frontend_.bready(s_axi_bready);
        axi_frontend_.araddr(s_axi_araddr);
        axi_frontend_.arprot(s_axi_arprot);
        axi_frontend_.arvalid(s_axi_arvalid);
        axi_frontend_.arready(s_axi_arready);
        axi_frontend_.rdata(s_axi_rdata);
        axi_frontend_.rresp(s_axi_rresp);
        axi_frontend_.rvalid(s_axi_rvalid);
        axi_frontend_.rready(s_axi_rready);
    }

    template<typename Block>
    void bind_clock_reset(Block& block)
    {
        block.pclk(pclk);
        block.rst_n(rst_n);
    }

    void bind_pipeline()
    {
        bind_clock_reset(crop_);
        bind_clock_reset(dpc_);
        bind_clock_reset(blc_);
        bind_clock_reset(oecf_);
        bind_clock_reset(dgain_);
        bind_clock_reset(lsc_);
        bind_clock_reset(bnr_);
        bind_clock_reset(wb_);
        bind_clock_reset(demosaic_);
        bind_clock_reset(ccm_);
        bind_clock_reset(gamma_);
        bind_clock_reset(csc_);
        bind_clock_reset(ldci_);
        bind_clock_reset(sharpen_);
        bind_clock_reset(nr2d_);
        bind_clock_reset(awb_);
        bind_clock_reset(ae_);
        bind_clock_reset(vip_);

        crop_.enable(s_crop_enable_);
        crop_.i_href(s_source_href_);
        crop_.i_vsync(s_source_vsync_);
        crop_.i_data(s_source_raw_);
        crop_.i_crop_w(s_crop_width_);
        crop_.i_crop_h(s_crop_height_);
        crop_.i_crop_x(s_crop_x_);
        crop_.i_crop_y(s_crop_y_);
        crop_.o_href(s_crop_href_);
        crop_.o_vsync(s_crop_vsync_);
        crop_.o_data(s_crop_raw_);

        dpc_.enable(s_dpc_enable_);
        dpc_.i_href(s_crop_href_);
        dpc_.i_vsync(s_crop_vsync_);
        dpc_.i_raw(s_crop_raw_);
        dpc_.i_threshold(s_dpc_threshold_);
        dpc_.o_href(s_dpc_href_);
        dpc_.o_vsync(s_dpc_vsync_);
        dpc_.o_raw(s_dpc_raw_);

        blc_.enable(s_blc_enable_);
        blc_.i_href(s_dpc_href_);
        blc_.i_vsync(s_dpc_vsync_);
        blc_.i_data(s_dpc_raw_);
        blc_.i_blc_r(s_blc_r_);
        blc_.i_blc_gr(s_blc_gr_);
        blc_.i_blc_gb(s_blc_gb_);
        blc_.i_blc_b(s_blc_b_);
        blc_.i_linear_en(s_linear_enable_);
        blc_.i_linear_r(s_linear_r_);
        blc_.i_linear_gr(s_linear_gr_);
        blc_.i_linear_gb(s_linear_gb_);
        blc_.i_linear_b(s_linear_b_);
        blc_.o_href(s_blc_href_);
        blc_.o_vsync(s_blc_vsync_);
        blc_.o_data(s_blc_raw_);

        oecf_.enable(s_oecf_enable_);
        oecf_.i_href(s_blc_href_);
        oecf_.i_vsync(s_blc_vsync_);
        oecf_.i_raw(s_blc_raw_);
        oecf_.o_href(s_oecf_href_);
        oecf_.o_vsync(s_oecf_vsync_);
        oecf_.o_raw(s_oecf_raw_);

        dgain_.enable(s_dgain_enable_);
        dgain_.i_href(s_oecf_href_);
        dgain_.i_vsync(s_oecf_vsync_);
        dgain_.i_raw(s_oecf_raw_);
        dgain_.i_is_manual(s_dgain_manual_);
        dgain_.i_manual_index(s_dgain_manual_index_);
        dgain_.i_ae_feedback_index(s_dgain_ae_index_);
        dgain_.o_href(s_dgain_href_);
        dgain_.o_vsync(s_dgain_vsync_);
        dgain_.o_raw(s_dgain_raw_);
        dgain_.o_applied_index(s_dgain_applied_index_);
        for (unsigned index = 0;
             index < isp_dgain<BITS>::GAIN_ARRAY_SIZE; ++index) {
            dgain_.i_dgain_array[index](s_dgain_array_[index]);
        }

        lsc_.enable(s_lsc_enable_);
        lsc_.i_href(s_dgain_href_);
        lsc_.i_vsync(s_dgain_vsync_);
        lsc_.i_raw(s_dgain_raw_);
        lsc_.o_href(s_lsc_href_);
        lsc_.o_vsync(s_lsc_vsync_);
        lsc_.o_raw(s_lsc_raw_);

        bnr_.enable(s_bnr_enable_);
        bnr_.i_href(s_lsc_href_);
        bnr_.i_vsync(s_lsc_vsync_);
        bnr_.i_raw(s_lsc_raw_);
        bnr_.o_href(s_bnr_href_);
        bnr_.o_vsync(s_bnr_vsync_);
        bnr_.o_raw(s_bnr_raw_);

        awb_.enable(s_awb_enable_);
        awb_.i_href(s_lsc_href_);
        awb_.i_vsync(s_lsc_vsync_);
        awb_.i_raw(s_lsc_raw_);
        awb_.i_underexposed_limit(s_awb_underexposed_);
        awb_.i_overexposed_limit(s_awb_overexposed_);
        awb_.i_frames(s_awb_frames_);
        awb_.o_r_gain(s_awb_r_gain_);
        awb_.o_b_gain(s_awb_b_gain_);

        wb_.enable(s_wb_enable_);
        wb_.i_href(s_bnr_href_);
        wb_.i_vsync(s_bnr_vsync_);
        wb_.i_data(s_bnr_raw_);
        wb_.i_gain_r(s_wb_r_gain_);
        wb_.i_gain_b(s_wb_b_gain_);
        wb_.o_href(s_wb_href_);
        wb_.o_vsync(s_wb_vsync_);
        wb_.o_data(s_wb_raw_);

        demosaic_.enable(s_demosaic_enable_);
        demosaic_.i_href(s_wb_href_);
        demosaic_.i_vsync(s_wb_vsync_);
        demosaic_.i_raw(s_wb_raw_);
        demosaic_.o_href(s_demosaic_href_);
        demosaic_.o_vsync(s_demosaic_vsync_);
        demosaic_.o_r(s_demosaic_r_);
        demosaic_.o_g(s_demosaic_g_);
        demosaic_.o_b(s_demosaic_b_);

        ccm_.enable(s_ccm_enable_);
        ccm_.i_href(s_ccm_input_href_);
        ccm_.i_vsync(s_ccm_input_vsync_);
        ccm_.i_r(s_ccm_input_r_);
        ccm_.i_g(s_ccm_input_g_);
        ccm_.i_b(s_ccm_input_b_);
        ccm_.i_m_rr(s_ccm_rr_);
        ccm_.i_m_rg(s_ccm_rg_);
        ccm_.i_m_rb(s_ccm_rb_);
        ccm_.i_m_gr(s_ccm_gr_);
        ccm_.i_m_gg(s_ccm_gg_);
        ccm_.i_m_gb(s_ccm_gb_);
        ccm_.i_m_br(s_ccm_br_);
        ccm_.i_m_bg(s_ccm_bg_);
        ccm_.i_m_bb(s_ccm_bb_);
        ccm_.o_href(s_ccm_href_);
        ccm_.o_vsync(s_ccm_vsync_);
        ccm_.o_r(s_ccm_r_);
        ccm_.o_g(s_ccm_g_);
        ccm_.o_b(s_ccm_b_);

        gamma_.enable(s_gamma_enable_);
        gamma_.i_href(s_ccm_href_);
        gamma_.i_vsync(s_ccm_vsync_);
        gamma_.i_data_r(s_ccm_r_);
        gamma_.i_data_g(s_ccm_g_);
        gamma_.i_data_b(s_ccm_b_);
        gamma_.o_href(s_gamma_href_);
        gamma_.o_vsync(s_gamma_vsync_);
        gamma_.o_data_r(s_gamma_r_);
        gamma_.o_data_g(s_gamma_g_);
        gamma_.o_data_b(s_gamma_b_);

        ae_.enable(s_ae_enable_);
        ae_.i_href(s_gamma_href_);
        ae_.i_vsync(s_gamma_vsync_);
        ae_.i_data(s_gamma_g_);
        ae_.i_center_illuminance(s_ae_center_);
        ae_.i_skewness(s_ae_skewness_);
        ae_.i_ae_crop_left(s_ae_crop_left_);
        ae_.i_ae_crop_right(s_ae_crop_right_);
        ae_.i_ae_crop_top(s_ae_crop_top_);
        ae_.i_ae_crop_bottom(s_ae_crop_bottom_);
        ae_.o_ae_response(s_ae_response_);
        ae_.o_ae_result_skewness(s_ae_result_skewness_);
        ae_.o_ae_done(s_ae_done_);

        csc_.enable(s_csc_enable_);
        csc_.i_href(s_gamma_href_);
        csc_.i_vsync(s_gamma_vsync_);
        csc_.i_r(s_gamma_r_);
        csc_.i_g(s_gamma_g_);
        csc_.i_b(s_gamma_b_);
        csc_.i_conv_standard(s_csc_standard_);
        csc_.o_href(s_csc_href_);
        csc_.o_vsync(s_csc_vsync_);
        csc_.o_y(s_csc_y_);
        csc_.o_u(s_csc_u_);
        csc_.o_v(s_csc_v_);

        ldci_.enable(s_ldci_enable_);
        ldci_.i_href(s_csc_href_);
        ldci_.i_vsync(s_csc_vsync_);
        ldci_.i_data_y(s_csc_y_);
        ldci_.i_data_u(s_csc_u_);
        ldci_.i_data_v(s_csc_v_);
        ldci_.o_href(s_ldci_href_);
        ldci_.o_vsync(s_ldci_vsync_);
        ldci_.o_data_y(s_ldci_y_);
        ldci_.o_data_u(s_ldci_u_);
        ldci_.o_data_v(s_ldci_v_);

        sharpen_.enable(s_sharpen_enable_);
        sharpen_.i_href(s_ldci_href_);
        sharpen_.i_vsync(s_ldci_vsync_);
        sharpen_.i_data_y(s_ldci_y_);
        sharpen_.i_data_u(s_ldci_u_);
        sharpen_.i_data_v(s_ldci_v_);
        sharpen_.i_sharpen_strength(s_sharpen_strength_);
        sharpen_.o_href(s_sharpen_href_);
        sharpen_.o_vsync(s_sharpen_vsync_);
        sharpen_.o_data_y(s_sharpen_y_);
        sharpen_.o_data_u(s_sharpen_u_);
        sharpen_.o_data_v(s_sharpen_v_);

        nr2d_.enable(s_nr2d_enable_);
        nr2d_.i_href(s_sharpen_href_);
        nr2d_.i_vsync(s_sharpen_vsync_);
        nr2d_.i_data_y(s_sharpen_y_);
        nr2d_.i_data_u(s_sharpen_u_);
        nr2d_.i_data_v(s_sharpen_v_);
        nr2d_.o_href(s_nr2d_href_);
        nr2d_.o_vsync(s_nr2d_vsync_);
        nr2d_.o_data_y(s_nr2d_y_);
        nr2d_.o_data_u(s_nr2d_u_);
        nr2d_.o_data_v(s_nr2d_v_);

        vip_.i_enable(s_vip_enable_);
        vip_.i_input_is_yuv(s_vip_input_is_yuv_);
        vip_.i_href(s_nr2d_href_);
        vip_.i_vsync(s_nr2d_vsync_);
        vip_.i_r(s_vip_input_y_);
        vip_.i_g(s_vip_input_u_);
        vip_.i_b(s_vip_input_v_);
        vip_.i_csc_standard(s_vip_csc_standard_);
        vip_.o_href(s_vip_href_);
        vip_.o_vsync(s_vip_vsync_);
        vip_.o_y(s_vip_y_);
        vip_.o_u(s_vip_u_);
        vip_.o_v(s_vip_v_);
        vip_.o_error(s_vip_error_);
    }

    void set_metric_dimensions()
    {
        crop_.set_image_size(WIDTH, HEIGHT);
        dpc_.set_image_size(WIDTH, HEIGHT);
        blc_.set_image_size(WIDTH, HEIGHT);
        oecf_.set_image_size(WIDTH, HEIGHT);
        bnr_.set_image_size(WIDTH, HEIGHT);
        wb_.set_image_size(WIDTH, HEIGHT);
        demosaic_.set_image_size(WIDTH, HEIGHT);
        ccm_.set_image_size(WIDTH, HEIGHT);
        gamma_.set_image_size(WIDTH, HEIGHT);
        csc_.set_image_size(WIDTH, HEIGHT);
        sharpen_.set_image_size(WIDTH, HEIGHT);
        nr2d_.set_image_size(WIDTH, HEIGHT);
        awb_.set_image_size(WIDTH, HEIGHT);
        ae_.set_image_size(WIDTH, HEIGHT);
    }

    void source_mux()
    {
        if (!rst_n.read()) {
            s_source_href_.write(false);
            s_source_vsync_.write(false);
            s_source_raw_.write(0);
        } else if (s_memory_select_.read()) {
            s_source_href_.write(s_memory_href_.read());
            s_source_vsync_.write(s_memory_vsync_.read());
            s_source_raw_.write(s_memory_raw_.read());
        } else {
            s_source_href_.write(in_href.read());
            s_source_vsync_.write(in_vsync.read());
            s_source_raw_.write(
                static_cast<std::uint16_t>(in_raw.read().to_uint()));
        }
    }

    void ccm_input_mux()
    {
        const bool use_direct =
            rst_n.read() && !s_memory_select_.read() &&
            s_direct_rgb_active_.read();
        if (use_direct) {
            s_ccm_input_href_.write(in_href_rgb.read());
            s_ccm_input_vsync_.write(in_vsync_rgb.read());
            s_ccm_input_r_.write(
                static_cast<std::uint16_t>(in_r.read().to_uint()));
            s_ccm_input_g_.write(
                static_cast<std::uint16_t>(in_g.read().to_uint()));
            s_ccm_input_b_.write(
                static_cast<std::uint16_t>(in_b.read().to_uint()));
        } else if (rst_n.read()) {
            s_ccm_input_href_.write(s_demosaic_href_.read());
            s_ccm_input_vsync_.write(s_demosaic_vsync_.read());
            s_ccm_input_r_.write(s_demosaic_r_.read());
            s_ccm_input_g_.write(s_demosaic_g_.read());
            s_ccm_input_b_.write(s_demosaic_b_.read());
        } else {
            s_ccm_input_href_.write(false);
            s_ccm_input_vsync_.write(false);
            s_ccm_input_r_.write(0);
            s_ccm_input_g_.write(0);
            s_ccm_input_b_.write(0);
        }
    }

    void vip_input_bridge()
    {
        s_vip_input_y_.write(s_nr2d_y_.read());
        s_vip_input_u_.write(s_nr2d_u_.read());
        s_vip_input_v_.write(s_nr2d_v_.read());
    }

    void video_output_bridge()
    {
        if (!rst_n.read()) {
            out_href.write(false);
            out_vsync.write(false);
            out_y.write(0);
            out_u.write(0);
            out_v.write(0);
            out_gamma_href.write(false);
            out_gamma_vsync.write(false);
            out_gamma_r.write(0);
            out_gamma_g.write(0);
            out_gamma_b.write(0);
            return;
        }

        out_href.write(s_vip_href_.read());
        out_vsync.write(s_vip_vsync_.read());
        out_y.write(s_vip_y_.read());
        out_u.write(s_vip_u_.read());
        out_v.write(s_vip_v_.read());
        out_gamma_href.write(s_gamma_href_.read());
        out_gamma_vsync.write(s_gamma_vsync_.read());
        out_gamma_r.write(s_gamma_r_.read());
        out_gamma_g.write(s_gamma_g_.read());
        out_gamma_b.write(s_gamma_b_.read());
    }

    void irq_bridge()
    {
        irq.write(rst_n.read() &&
                  (register_bank_.irq_level() ||
                   register_bank_.vip_irq_level()));
    }

    void load_active_configuration()
    {
        using namespace isp_tlm::registers;
        const std::uint32_t enables = active_register(kIspTopEnable);
        s_dpc_enable_.write((enables & (1U << 0U)) != 0);
        s_blc_enable_.write((enables & (1U << 1U)) != 0);
        s_linear_enable_.write((enables & (1U << 2U)) != 0);
        s_oecf_enable_.write((enables & (1U << 3U)) != 0);
        s_dgain_enable_.write((enables & (1U << 4U)) != 0);
        s_lsc_enable_.write((enables & (1U << 5U)) != 0);
        s_bnr_enable_.write((enables & (1U << 6U)) != 0);
        s_wb_enable_.write((enables & (1U << 7U)) != 0);
        s_demosaic_enable_.write((enables & (1U << 8U)) != 0);
        s_ccm_enable_.write((enables & (1U << 9U)) != 0);
        s_gamma_enable_.write((enables & (1U << 10U)) != 0);
        s_csc_enable_.write((enables & (1U << 11U)) != 0);
        s_ldci_enable_.write((enables & (1U << 12U)) != 0);
        s_nr2d_enable_.write((enables & (1U << 13U)) != 0);
        s_sharpen_enable_.write((enables & (1U << 14U)) != 0);
        s_ae_enable_.write((enables & (1U << 15U)) != 0);
        s_awb_enable_.write((enables & (1U << 16U)) != 0);
        s_crop_enable_.write((enables & (1U << 17U)) != 0);

        const std::uint32_t job_control =
            active_register(kIspJobControl);
        active_direct_rgb_ =
            (job_control & kJobControlDirectRgbInput) != 0;
        s_direct_rgb_active_.write(active_direct_rgb_);

        const std::uint32_t vip_enable = active_register(kVipTopEnable);
        s_vip_enable_.write((vip_enable & (1U << 4U)) != 0);
        s_vip_input_is_yuv_.write(true);

        s_crop_width_.write(WIDTH);
        s_crop_height_.write(HEIGHT);
        s_crop_x_.write(0);
        s_crop_y_.write(0);
        s_dpc_threshold_.write(active_register(kDpcThreshold));
        s_blc_r_.write(active_register(kBlcR));
        s_blc_gr_.write(active_register(kBlcGr));
        s_blc_gb_.write(active_register(kBlcGb));
        s_blc_b_.write(active_register(kBlcB));
        s_linear_r_.write(active_register(kLinearR));
        s_linear_gr_.write(active_register(kLinearGr));
        s_linear_gb_.write(active_register(kLinearGb));
        s_linear_b_.write(active_register(kLinearB));
        s_dgain_manual_.write(active_register(kDgainIsManual) != 0);
        s_dgain_manual_index_.write(active_register(kDgainManualIndex));
        for (std::uint32_t index = 0;
             index < isp_dgain<BITS>::GAIN_ARRAY_SIZE; ++index) {
            s_dgain_array_[index].write(
                active_register(kDgainArrayBase +
                                index * kRegisterBytes));
        }
        s_wb_r_gain_.write(active_register(kWbRGain));
        s_wb_b_gain_.write(active_register(kWbBGain));
        s_ccm_rr_.write(static_cast<std::int16_t>(
            active_register(kCcmRr)));
        s_ccm_rg_.write(static_cast<std::int16_t>(
            active_register(kCcmRg)));
        s_ccm_rb_.write(static_cast<std::int16_t>(
            active_register(kCcmRb)));
        s_ccm_gr_.write(static_cast<std::int16_t>(
            active_register(kCcmGr)));
        s_ccm_gg_.write(static_cast<std::int16_t>(
            active_register(kCcmGg)));
        s_ccm_gb_.write(static_cast<std::int16_t>(
            active_register(kCcmGb)));
        s_ccm_br_.write(static_cast<std::int16_t>(
            active_register(kCcmBr)));
        s_ccm_bg_.write(static_cast<std::int16_t>(
            active_register(kCcmBg)));
        s_ccm_bb_.write(static_cast<std::int16_t>(
            active_register(kCcmBb)));
        const std::uint32_t csc_standard =
            active_register(kCscConversionStandard);
        s_csc_standard_.write(csc_standard);
        s_vip_csc_standard_.write(csc_standard);
        s_sharpen_strength_.write(active_register(kSharpenStrength));
        s_awb_underexposed_.write(
            active_register(kAwbUnderexposedLimit));
        s_awb_overexposed_.write(
            active_register(kAwbOverexposedLimit));
        s_awb_frames_.write(active_register(kAwbFrames));
        s_ae_center_.write(active_register(kAeCenterIlluminance));
        s_ae_skewness_.write(active_register(kAeSkewness));
        s_ae_crop_left_.write(active_register(kAeCropLeft));
        s_ae_crop_right_.write(active_register(kAeCropRight));
        s_ae_crop_top_.write(active_register(kAeCropTop));
        s_ae_crop_bottom_.write(active_register(kAeCropBottom));
        load_active_array_controls();
        load_active_luts();
    }

    void load_active_array_controls()
    {
        using namespace isp_tlm::registers;
        const std::uint32_t spatial_bases[3] = {
            kBnrSpatialRBase,
            kBnrSpatialGBase,
            kBnrSpatialBBase,
        };
        const std::uint32_t color_bases[3] = {
            kBnrColorRBase,
            kBnrColorGBase,
            kBnrColorBBase,
        };
        for (unsigned channel = 0; channel < 3; ++channel) {
            for (unsigned row = 0; row < 5; ++row) {
                const std::uint32_t first_four =
                    active_register(
                        spatial_bases[channel] +
                        row * 2U * kRegisterBytes);
                const std::uint32_t fifth =
                    active_register(
                        spatial_bases[channel] +
                        (row * 2U + 1U) * kRegisterBytes);
                for (unsigned column = 0; column < 4; ++column) {
                    (void)bnr_.set_spatial_entry(
                        channel,
                        row,
                        column,
                        static_cast<std::uint8_t>(
                            first_four >> (column * 8U)));
                }
                (void)bnr_.set_spatial_entry(
                    channel,
                    row,
                    4,
                    static_cast<std::uint8_t>(fifth));
            }
            for (unsigned index = 0; index < kBnrColorWordsPerChannel;
                 ++index) {
                const std::uint32_t packed =
                    active_register(
                        color_bases[channel] +
                        index * kRegisterBytes);
                (void)bnr_.set_color_entry(
                    channel,
                    index,
                    static_cast<std::uint16_t>(packed),
                    static_cast<std::uint8_t>(packed >> 16U));
            }
        }
        for (std::uint32_t index = 0; index < kSharpenKernelWords;
             ++index) {
            (void)sharpen_.set_kernel_entry(
                index / isp_sharpen::WINDOW_SIZE,
                index % isp_sharpen::WINDOW_SIZE,
                static_cast<int>(active_register(
                    kSharpenKernelBase + index * kRegisterBytes)));
        }
        for (std::uint32_t word = 0; word < kNr2dDifferenceWords;
             ++word) {
            const std::uint32_t packed_difference =
                active_register(kNr2dDifferenceBase +
                                word * kRegisterBytes);
            const std::uint32_t packed_weight =
                active_register(kNr2dWeightBase +
                                word * kRegisterBytes);
            for (std::uint32_t byte = 0; byte < 4; ++byte) {
                const unsigned index =
                    static_cast<unsigned>(word * 4U + byte);
                (void)nr2d_.set_difference_entry(
                    index,
                    static_cast<std::uint8_t>(
                        packed_difference >> (byte * 8U)));
                (void)nr2d_.set_weight_entry(
                    index,
                    static_cast<std::uint8_t>(
                        packed_weight >> (byte * 8U)));
            }
        }
    }

    void load_active_luts()
    {
        const std::size_t entries = register_bank_.oecf_lut_size();
        for (std::size_t index = 0; index < entries; ++index) {
            std::uint32_t value = 0;
            if (register_bank_.read_active_gamma(index, value) ==
                isp_tlm::registers::AccessResult::Ok) {
                (void)gamma_.set_lut_entry(
                    static_cast<unsigned>(index),
                    static_cast<std::uint16_t>(value));
            }
            for (std::size_t channel = 0; channel < 4; ++channel) {
                if (register_bank_.read_active_oecf(
                        channel, index, value) ==
                    isp_tlm::registers::AccessResult::Ok) {
                    (void)oecf_.set_lut_entry(
                        static_cast<unsigned>(channel),
                        static_cast<unsigned>(index),
                        static_cast<std::uint16_t>(value));
                }
            }
        }
    }

    void reset_control_state()
    {
        input_sequence_ = 0;
        output_sequence_ = 0;
        previous_selected_vsync_ = false;
        active_direct_rgb_ = false;
        job_active_ = false;
        job_memory_mode_ = false;
        stream_waiting_for_frame_ = false;
        job_target_sequence_ = 0;
        capture_active_ = false;
        capture_complete_ = false;
        capture_error_ = false;
        capture_y_.clear();
        capture_u_.clear();
        capture_v_.clear();
        input_frame_timing_.clear();
        performance_ = {};
    }

    bool selected_stream_vsync() const
    {
        return active_direct_rgb_ ? in_vsync_rgb.read()
                                  : in_vsync.read();
    }

    void snapshot_descriptors()
    {
        using namespace isp_tlm::registers;
        source_descriptor_.address =
            active_register(kIspSourceAddress);
        source_descriptor_.stride_bytes =
            active_register(kIspSourceStrideBytes);
        source_descriptor_.size_bytes =
            active_register(kIspSourceSizeBytes);
        source_descriptor_.width = WIDTH;
        source_descriptor_.height = HEIGHT;

        destination_descriptor_.y.address =
            active_register(kIspDestinationYAddress);
        destination_descriptor_.cb.address =
            active_register(kIspDestinationUAddress);
        destination_descriptor_.cr.address =
            active_register(kIspDestinationVAddress);
        destination_descriptor_.y.stride_bytes =
            active_register(kIspDestinationYStrideBytes);
        const std::uint32_t uv_stride =
            active_register(kIspDestinationUvStrideBytes);
        destination_descriptor_.cb.stride_bytes = uv_stride;
        destination_descriptor_.cr.stride_bytes = uv_stride;
        destination_descriptor_.y.size_bytes =
            active_register(kIspDestinationYSizeBytes);
        destination_descriptor_.cb.size_bytes =
            active_register(kIspDestinationUSizeBytes);
        destination_descriptor_.cr.size_bytes =
            active_register(kIspDestinationVSizeBytes);
        destination_descriptor_.width = WIDTH;
        destination_descriptor_.height = HEIGHT;
    }

    void finish_job(bool error, std::uint32_t error_code)
    {
        const cdc::components::isp::MemoryIoCounters& counters =
            frame_memory_.counters();
        isp_tlm::registers::JobStatistics statistics;
        statistics.read_bytes = narrow_counter(counters.read_bytes);
        statistics.write_bytes = narrow_counter(counters.write_bytes);
        statistics.read_transactions =
            narrow_counter(counters.read_transactions);
        statistics.write_transactions =
            narrow_counter(counters.write_transactions);
        register_bank_.complete_job(statistics, error, error_code);
        job_active_ = false;
        stream_waiting_for_frame_ = false;
        job_target_sequence_ = 0;
        capture_active_ = false;
        capture_complete_ = false;
    }

    void start_requested_job()
    {
        using namespace isp_tlm::registers;
        register_bank_.commit_frame();
        load_active_configuration();
        snapshot_descriptors();
        frame_memory_.reset_counters();

        const std::uint32_t job_control =
            active_register(kIspJobControl);
        job_memory_mode_ =
            (job_control & kJobControlSourceMode) != 0;
        const bool direct_rgb =
            (job_control & kJobControlDirectRgbInput) != 0;
        const std::uint32_t enables = active_register(kIspTopEnable);
        const bool csc_enabled = (enables & (1U << 11U)) != 0;
        const std::uint32_t csc_standard =
            active_register(kCscConversionStandard);
        const bool vip_enabled =
            (active_register(kVipTopEnable) & (1U << 4U)) != 0;

        register_bank_.set_job_busy(true);
        job_active_ = true;
        capture_complete_ = false;
        capture_error_ = false;
        capture_active_ = false;
        job_target_sequence_ = 0;

        if (!vip_enabled || !csc_enabled ||
            (csc_standard != isp_csc<BITS>::CSC_BT601 &&
             csc_standard != isp_csc<BITS>::CSC_BT709) ||
            (job_memory_mode_ &&
             (BITS != 12 || BAYER != BayerPattern::RGGB || direct_rgb))) {
            finish_job(
                true,
                kJobErrorUnsupportedConfiguration);
            return;
        }

        if (job_memory_mode_) {
            stream_waiting_for_frame_ = false;
        } else {
            stream_waiting_for_frame_ = true;
        }
        job_start_event_.notify(sc_core::SC_ZERO_TIME);
    }

    void control_thread()
    {
        using namespace isp_tlm::registers;
        while (true) {
            wait();
            if (!rst_n.read()) {
                register_bank_.reset();
                reset_control_state();
                load_active_configuration();
                continue;
            }

            const bool start_requested =
                register_bank_.consume_start_request();
            if (start_requested) {
                start_requested_job();
            }

            if (job_memory_mode_ && job_active_) {
                continue;
            }

            const bool selected_vsync = selected_stream_vsync();
            const bool frame_start =
                previous_selected_vsync_ && !selected_vsync;
            previous_selected_vsync_ = selected_vsync;
            if (!frame_start) {
                continue;
            }

            if (!start_requested) {
                register_bank_.commit_frame();
                load_active_configuration();
            }

            ++input_sequence_;
            input_frame_timing_[input_sequence_] = {
                performance_.pclk_cycles,
                sc_core::sc_time_stamp(),
            };
            register_bank_.raise_interrupt(kInterruptFrameStart);

            if (stream_waiting_for_frame_ && job_active_) {
                job_target_sequence_ = input_sequence_;
                stream_waiting_for_frame_ = false;
            }
        }
    }

    void reset_output_state()
    {
        previous_output_vsync_ = false;
        previous_ae_done_ = false;
        capture_active_ = false;
        capture_complete_ = false;
        capture_error_ = false;
        capture_y_.clear();
        capture_u_.clear();
        capture_v_.clear();
    }

    void record_completed_frame(std::uint64_t sequence)
    {
        const auto iterator = input_frame_timing_.find(sequence);
        if (iterator == input_frame_timing_.end()) {
            return;
        }
        const FrameTiming timing = iterator->second;
        input_frame_timing_.erase(iterator);
        performance_.last_frame_cycles =
            performance_.pclk_cycles - timing.cycle;
        performance_.last_frame_time =
            sc_core::sc_time_stamp() - timing.time;
        performance_.last_frame_fps =
            performance_.last_frame_time == sc_core::SC_ZERO_TIME
                ? 0.0
                : 1.0 / performance_.last_frame_time.to_seconds();
        ++performance_.frames_completed;
    }

    void begin_capture()
    {
        capture_y_.clear();
        capture_u_.clear();
        capture_v_.clear();
        capture_y_.reserve(static_cast<std::size_t>(WIDTH) * HEIGHT);
        capture_u_.reserve(static_cast<std::size_t>(WIDTH / 2U) *
                           (HEIGHT / 2U));
        capture_v_.reserve(static_cast<std::size_t>(WIDTH / 2U) *
                           (HEIGHT / 2U));
        capture_active_ = true;
        capture_complete_ = false;
        capture_error_ = false;
    }

    void capture_pixel()
    {
        const std::size_t pixel_index = capture_y_.size();
        const std::size_t expected =
            static_cast<std::size_t>(WIDTH) * HEIGHT;
        if (pixel_index >= expected) {
            capture_error_ = true;
            return;
        }
        capture_y_.push_back(s_vip_y_.read());
        const std::size_t x = pixel_index % WIDTH;
        const std::size_t y = pixel_index / WIDTH;
        if ((x & 1U) == 0U && (y & 1U) == 0U) {
            capture_u_.push_back(s_vip_u_.read());
            capture_v_.push_back(s_vip_v_.read());
        }
    }

    void end_capture()
    {
        const std::size_t expected_y =
            static_cast<std::size_t>(WIDTH) * HEIGHT;
        const std::size_t expected_chroma = expected_y / 4U;
        capture_error_ =
            capture_error_ || capture_y_.size() != expected_y ||
            capture_u_.size() != expected_chroma ||
            capture_v_.size() != expected_chroma;
        capture_active_ = false;
        capture_complete_ = true;
        capture_complete_event_.notify(sc_core::SC_ZERO_TIME);
    }

    void update_hardware_registers()
    {
        using namespace isp_tlm::registers;
        (void)register_bank_.set_hardware_value(
            kDgainIndexOut, s_dgain_applied_index_.read());
        (void)register_bank_.set_hardware_value(
            kAwbFinalRGain, s_awb_r_gain_.read());
        (void)register_bank_.set_hardware_value(
            kAwbFinalBGain, s_awb_b_gain_.read());
        (void)register_bank_.set_hardware_value(
            kAeResponse,
            static_cast<std::uint8_t>(s_ae_response_.read()));
        (void)register_bank_.set_hardware_value(
            kAeResponseDebug,
            static_cast<std::uint8_t>(s_ae_response_.read()));
        (void)register_bank_.set_hardware_value(
            kAeResultSkewness, s_ae_result_skewness_.read());
        (void)register_bank_.set_hardware_value(
            kAeDone, s_ae_done_.read() ? 1U : 0U);

        const std::int8_t ae_response = s_ae_response_.read();
        s_dgain_ae_index_.write(
            ae_response > 0 ? static_cast<std::uint8_t>(ae_response) : 0U);
        const bool ae_done = s_ae_done_.read();
        if (ae_done && !previous_ae_done_) {
            register_bank_.raise_interrupt(kInterruptAeDone);
        }
        previous_ae_done_ = ae_done;
    }

    void output_monitor()
    {
        using namespace isp_tlm::registers;
        if (!rst_n.read()) {
            reset_output_state();
            return;
        }

        ++performance_.pclk_cycles;
        update_hardware_registers();

        const bool current_vsync = s_vip_vsync_.read();
        const bool frame_start =
            previous_output_vsync_ && !current_vsync;
        const bool frame_done =
            output_sequence_ != 0 &&
            !previous_output_vsync_ && current_vsync;

        if (frame_start) {
            ++output_sequence_;
            register_bank_.raise_vip_interrupt(
                kVipInterruptFrameStart);
            if (job_active_ && job_target_sequence_ != 0 &&
                output_sequence_ == job_target_sequence_) {
                begin_capture();
            }
        }

        if (capture_active_ && s_vip_href_.read()) {
            capture_pixel();
        }

        if (frame_done) {
            register_bank_.raise_interrupt(kInterruptFrameDone);
            register_bank_.raise_vip_interrupt(
                kVipInterruptFrameDone);
            if (s_awb_enable_.read()) {
                register_bank_.raise_interrupt(kInterruptAwbDone);
            }
            record_completed_frame(output_sequence_);
            if (capture_active_) {
                end_capture();
            } else {
                register_bank_.increment_frame_count();
            }
        }
        previous_output_vsync_ = current_vsync;
    }

    bool wait_pixel_clock_or_reset()
    {
        wait(pclk.posedge_event() | rst_n.negedge_event());
        return rst_n.read();
    }

    void clear_memory_stream()
    {
        s_memory_href_.write(false);
        s_memory_vsync_.write(false);
        s_memory_raw_.write(0);
        s_memory_select_.write(false);
    }

    void register_memory_frame_start()
    {
        using namespace isp_tlm::registers;
        ++input_sequence_;
        job_target_sequence_ = input_sequence_;
        input_frame_timing_[input_sequence_] = {
            performance_.pclk_cycles,
            sc_core::sc_time_stamp(),
        };
        register_bank_.raise_interrupt(kInterruptFrameStart);
    }

    bool drive_memory_frame(const std::vector<std::uint16_t>& pixels)
    {
        s_memory_select_.write(true);
        s_memory_href_.write(false);
        s_memory_raw_.write(0);
        s_memory_vsync_.write(true);
        if (!wait_pixel_clock_or_reset()) {
            return false;
        }

        register_memory_frame_start();
        s_memory_vsync_.write(false);
        if (!wait_pixel_clock_or_reset()) {
            return false;
        }

        for (unsigned row = 0; row < HEIGHT; ++row) {
            s_memory_href_.write(true);
            for (unsigned column = 0; column < WIDTH; ++column) {
                const std::size_t index =
                    static_cast<std::size_t>(row) * WIDTH + column;
                s_memory_raw_.write(pixels[index]);
                if (!wait_pixel_clock_or_reset()) {
                    return false;
                }
            }
            s_memory_href_.write(false);
            s_memory_raw_.write(0);
            if (!wait_pixel_clock_or_reset()) {
                return false;
            }
        }

        s_memory_href_.write(false);
        s_memory_vsync_.write(true);
        s_memory_raw_.write(0);
        return wait_pixel_clock_or_reset();
    }

    bool wait_for_capture()
    {
        while (job_active_ && !capture_complete_) {
            wait(capture_complete_event_ | rst_n.negedge_event());
            if (!rst_n.read()) {
                return false;
            }
        }
        return job_active_ && capture_complete_;
    }

    std::uint32_t read_error_code(
        const cdc::components::isp::MemoryIoResult& result) const
    {
        using namespace isp_tlm::registers;
        return result.error ==
                       cdc::components::isp::MemoryError::transport_error
                   ? kJobErrorMemoryRead
                   : kJobErrorSourceDescriptor;
    }

    std::uint32_t write_error_code(
        const cdc::components::isp::MemoryIoResult& result) const
    {
        using namespace isp_tlm::registers;
        return result.error ==
                       cdc::components::isp::MemoryError::transport_error
                   ? kJobErrorMemoryWrite
                   : kJobErrorDestinationDescriptor;
    }

    bool write_captured_frame()
    {
        using namespace isp_tlm::registers;
        if (capture_error_) {
            finish_job(true, kJobErrorOutputFrame);
            return false;
        }
        const cdc::components::isp::MemoryIoResult result =
            frame_memory_.write_i420_frame(
                destination_descriptor_,
                capture_y_,
                capture_u_,
                capture_v_);
        if (!result) {
            register_bank_.record_tlm_error();
            finish_job(true, write_error_code(result));
            return false;
        }
        finish_job(false, kJobErrorNone);
        return true;
    }

    void job_worker()
    {
        using namespace isp_tlm::registers;
        clear_memory_stream();
        while (true) {
            wait(job_start_event_ | rst_n.negedge_event());
            if (!rst_n.read()) {
                clear_memory_stream();
                continue;
            }
            if (!job_active_) {
                continue;
            }

            if (!job_memory_mode_) {
                if (wait_for_capture()) {
                    (void)write_captured_frame();
                }
                continue;
            }

            std::vector<std::uint16_t> raw_pixels;
            const cdc::components::isp::MemoryIoResult read_result =
                frame_memory_.read_raw12_frame(
                    source_descriptor_, raw_pixels);
            if (!read_result) {
                register_bank_.record_tlm_error();
                finish_job(true, read_error_code(read_result));
                clear_memory_stream();
                continue;
            }
            if (!rst_n.read() || !job_active_) {
                clear_memory_stream();
                continue;
            }

            if (!drive_memory_frame(raw_pixels)) {
                clear_memory_stream();
                continue;
            }
            if (!wait_for_capture()) {
                clear_memory_stream();
                continue;
            }
            (void)write_captured_frame();
            clear_memory_stream();
        }
    }
};

using isp_top_12b_rggb =
    isp_top<12, BayerPattern::RGGB, 2592, 1536>;
using isp_top_10b_rggb =
    isp_top<10, BayerPattern::RGGB, 2592, 1536>;

#endif  // ISP_TOP_H
