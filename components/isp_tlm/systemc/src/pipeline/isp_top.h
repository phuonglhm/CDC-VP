/*
 * ISP Top Module - Main pipeline orchestrator
 * Matches RTL isp_top module structure
 */

#ifndef ISP_TOP_H
#define ISP_TOP_H

#include <systemc>
#include <tlm>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "common/isp_params.h"
#include "pipeline/isp_config.h"

// Forward declarations of block classes
template<unsigned int BITS> class isp_crop;
template<unsigned int BITS> class isp_dpc;
template<unsigned int BITS> class isp_blc;
template<unsigned int BITS> class isp_oecf;
template<unsigned int BITS> class isp_dgain;
template<unsigned int BITS> class isp_bnr;
template<unsigned int BITS> class isp_wb;
template<unsigned int BITS> class isp_demosaic;
template<unsigned int BITS> class isp_ccm;
template<unsigned int BITS> class isp_gamma;
template<unsigned int BITS> class isp_csc;
template<unsigned int BITS> class isp_sharpen;
template<unsigned int BITS> class isp_2dnr;

//=============================================================================
// ISP Top Module
// Main signal processing pipeline
//=============================================================================
template<unsigned int BITS = 10>
class isp_top : public sc_module {
public:
    //=============================================================================
    // Clock and Reset
    //=============================================================================
    sc_in<bool> pclk{"pclk"};
    sc_in<bool> rst_n{"rst_n"};

    //=============================================================================
    // Input Interface (RAW from sensor)
    //=============================================================================
    sc_in<bool> in_href{"in_href"};
    sc_in<bool> in_vsync{"in_vsync"};
    sc_in<uint16_t> in_raw{"in_raw"};

    //=============================================================================
    // Input Interface (Direct RGB - for RAW bypass testing)
    //=============================================================================
    sc_in<bool> in_href_rgb{"in_href_rgb"};
    sc_in<bool> in_vsync_rgb{"in_vsync_rgb"};
    sc_in<uint16_t> in_r{"in_r"};
    sc_in<uint16_t> in_g{"in_g"};
    sc_in<uint16_t> in_b{"in_b"};

    // RGB input enable
    sc_in<bool> rgb_inp_en{"rgb_inp_en"};

    //=============================================================================
    // Output Interface (RGB after gamma)
    //=============================================================================
    sc_out<bool> out_gamma_href{"out_gamma_href"};
    sc_out<bool> out_gamma_vsync{"out_gamma_vsync"};
    sc_out<uint16_t> out_gamma_r{"out_gamma_r"};
    sc_out<uint16_t> out_gamma_g{"out_gamma_g"};
    sc_out<uint16_t> out_gamma_b{"out_gamma_b"};

    //=============================================================================
    // Output Interface (YUV after 2DNR)
    //=============================================================================
    sc_out<bool> out_href{"out_href"};
    sc_out<bool> out_vsync{"out_vsync"};
    sc_out<uint8_t> out_y{"out_y"};
    sc_out<uint8_t> out_u{"out_u"};
    sc_out<uint8_t> out_v{"out_v"};

    //=============================================================================
    // Module Enables
    //=============================================================================
    sc_in<bool> crop_en{"crop_en"};
    sc_in<bool> dpc_en{"dpc_en"};
    sc_in<bool> blc_en{"blc_en"};
    sc_in<bool> oecf_en{"oecf_en"};
    sc_in<bool> dgain_en{"dgain_en"};
    sc_in<bool> lsc_en{"lsc_en"};
    sc_in<bool> bnr_en{"bnr_en"};
    sc_in<bool> wb_en{"wb_en"};
    sc_in<bool> demosic_en{"demosic_en"};
    sc_in<bool> ccm_en{"ccm_en"};
    sc_in<bool> gamma_en{"gamma_en"};
    sc_in<bool> csc_en{"csc_en"};
    sc_in<bool> ldci_en{"ldci_en"};
    sc_in<bool> sharpen_en{"sharpen_en"};
    sc_in<bool> nr2d_en{"nr2d_en"};
    sc_in<bool> stat_ae_en{"stat_ae_en"};
    sc_in<bool> awb_en{"awb_en"};
    sc_in<bool> ae_en{"ae_en"};

    //=============================================================================
    // Block Parameters (bound to external registers)
    //=============================================================================

    // DPC
    sc_in<uint16_t> dpc_threshold{"dpc_threshold"};

    // BLC
    sc_in<uint16_t> blc_r{"blc_r"};
    sc_in<uint16_t> blc_gr{"blc_gr"};
    sc_in<uint16_t> blc_gb{"blc_gb"};
    sc_in<uint16_t> blc_b{"blc_b"};
    sc_in<bool> linear_en{"linear_en"};
    sc_in<uint16_t> linear_r{"linear_r"};
    sc_in<uint16_t> linear_gr{"linear_gr"};
    sc_in<uint16_t> linear_gb{"linear_gb"};
    sc_in<uint16_t> linear_b{"linear_b"};

    // BNR
    sc_in<uint16_t> bnr_space_kernel_r{"bnr_space_kernel_r"};
    sc_in<uint16_t> bnr_space_kernel_g{"bnr_space_kernel_g"};
    sc_in<uint16_t> bnr_space_kernel_b{"bnr_space_kernel_b"};
    // Note: These would be bound to actual kernel arrays

    // Digital Gain
    sc_in<bool> dgain_is_manual{"dgain_is_manual"};
    sc_in<uint8_t> dgain_man_index{"dgain_man_index"};
    sc_out<uint8_t> dgain_index_out{"dgain_index_out"};

    // White Balance
    sc_in<uint16_t> wb_rgain{"wb_rgain"};
    sc_in<uint16_t> wb_bgain{"wb_bgain"};

    // CCM
    sc_in<uint16_t> ccm_rr{"ccm_rr"}, ccm_rg{"ccm_rg"}, ccm_rb{"ccm_rb"};
    sc_in<uint16_t> ccm_gr{"ccm_gr"}, ccm_gg{"ccm_gg"}, ccm_gb{"ccm_gb"};
    sc_in<uint16_t> ccm_br{"ccm_br"}, ccm_bg{"ccm_bg"}, ccm_bb{"ccm_bb"};

    // CSC
    sc_in<uint8_t> in_conv_standard{"in_conv_standard"};

    // Sharpening
    sc_in<uint16_t> sharpen_strength{"sharpen_strength"};

    // AWB outputs
    sc_out<uint16_t> final_r_gain{"final_r_gain"};
    sc_out<uint16_t> final_b_gain{"final_b_gain"};

    // AE outputs
    sc_out<uint8_t> ae_response{"ae_response"};
    sc_out<bool> ae_done{"ae_done"};

    //=============================================================================
    // Constructor
    //=============================================================================
    isp_top(const sc_module_name& name)
        : sc_module(name)
    {
        // Instantiate blocks - will be implemented when blocks are created
        // m_crop = new isp_crop<BITS>("crop");
        // m_dpc = new isp_dpc<BITS>("dpc");
        // etc.

        SC_METHOD(clock_edge);
        sensitive << pclk.pos();
    }

    virtual ~isp_top() = default;

    //=============================================================================
    // Configuration
    //=============================================================================
    void configure(const isp_config& cfg) {
        m_config = cfg;
    }

    void set_image_params(unsigned sns_w, unsigned sns_h,
                         unsigned crop_w, unsigned crop_h,
                         BayerPattern bayer) {
        m_sns_width = sns_w;
        m_sns_height = sns_h;
        m_crop_width = crop_w;
        m_crop_height = crop_h;
        m_bayer = bayer;
    }

    //=============================================================================
    // Status
    //=============================================================================
    unsigned int get_frame_count() const { return m_frame_count; }

private:
    //=============================================================================
    // Internal blocks (to be instantiated)
    //=============================================================================
    // isp_crop<BITS>* m_crop;
    // isp_dpc<BITS>* m_dpc;
    // isp_blc<BITS>* m_blc;
    // ... etc

    //=============================================================================
    // Configuration
    //=============================================================================
    isp_config m_config;

    //=============================================================================
    // Image Parameters
    //=============================================================================
    unsigned m_sns_width = isp_params::DEFAULT_SNS_WIDTH;
    unsigned m_sns_height = isp_params::DEFAULT_SNS_HEIGHT;
    unsigned m_crop_width = isp_params::DEFAULT_CROP_WIDTH;
    unsigned m_crop_height = isp_params::DEFAULT_CROP_HEIGHT;
    BayerPattern m_bayer = isp_params::DEFAULT_BAYER;

    //=============================================================================
    // Status
    //=============================================================================
    unsigned int m_frame_count = 0;
    bool m_prev_vsync = false;

    //=============================================================================
    // Internal signals (wire connections)
    //=============================================================================

    // Crop stage
    sc_signal<bool> crop_href{"crop_href"};
    sc_signal<bool> crop_vsync{"crop_vsync"};
    sc_signal<uint16_t> crop_raw{"crop_raw"};

    // DPC stage
    sc_signal<bool> dpc_href{"dpc_href"};
    sc_signal<bool> dpc_vsync{"dpc_vsync"};
    sc_signal<uint16_t> dpc_raw{"dpc_raw"};

    // BLC stage
    sc_signal<bool> blc_href{"blc_href"};
    sc_signal<bool> blc_vsync{"blc_vsync"};
    sc_signal<uint16_t> blc_raw{"blc_raw"};

    // OECF stage
    sc_signal<bool> oecf_href{"oecf_href"};
    sc_signal<bool> oecf_vsync{"oecf_vsync"};
    sc_signal<uint16_t> oecf_raw{"oecf_raw"};

    // DGain stage
    sc_signal<bool> dgain_href{"dgain_href"};
    sc_signal<bool> dgain_vsync{"dgain_vsync"};
    sc_signal<uint16_t> dgain_raw{"dgain_raw"};
    sc_signal<uint8_t> dgain_index{"dgain_index"};

    // BNR stage
    sc_signal<bool> bnr_href{"bnr_href"};
    sc_signal<bool> bnr_vsync{"bnr_vsync"};
    sc_signal<uint16_t> bnr_raw{"bnr_raw"};

    // WB stage
    sc_signal<bool> wb_href{"wb_href"};
    sc_signal<bool> wb_vsync{"wb_vsync"};
    sc_signal<uint16_t> wb_raw{"wb_raw"};

    // Demosaic stage
    sc_signal<bool> demosaic_href{"demosaic_href"};
    sc_signal<bool> demosaic_vsync{"demosaic_vsync"};
    sc_signal<uint16_t> demosaic_r{"demosaic_r"};
    sc_signal<uint16_t> demosaic_g{"demosaic_g"};
    sc_signal<uint16_t> demosaic_b{"demosaic_b"};

    // CCM stage
    sc_signal<bool> ccm_href{"ccm_href"};
    sc_signal<bool> ccm_vsync{"ccm_vsync"};
    sc_signal<uint16_t> ccm_r{"ccm_r"};
    sc_signal<uint16_t> ccm_g{"ccm_g"};
    sc_signal<uint16_t> ccm_b{"ccm_b"};

    // Gamma stage
    sc_signal<bool> gamma_href{"gamma_href"};
    sc_signal<bool> gamma_vsync{"gamma_vsync"};
    sc_signal<uint16_t> gamma_r{"gamma_r"};
    sc_signal<uint16_t> gamma_g{"gamma_g"};
    sc_signal<uint16_t> gamma_b{"gamma_b"};

    // CSC stage
    sc_signal<bool> csc_href{"csc_href"};
    sc_signal<bool> csc_vsync{"csc_vsync"};
    sc_signal<uint16_t> csc_y{"csc_y"};
    sc_signal<uint16_t> csc_u{"csc_u"};
    sc_signal<uint16_t> csc_v{"csc_v"};

    // Sharpen stage
    sc_signal<bool> sharpen_href{"sharpen_href"};
    sc_signal<bool> sharpen_vsync{"sharpen_vsync"};
    sc_signal<uint8_t> sharpen_y{"sharpen_y"};
    sc_signal<uint8_t> sharpen_u{"sharpen_u"};
    sc_signal<uint8_t> sharpen_v{"sharpen_v"};

    // 2DNR stage
    sc_signal<bool> nr2d_href{"nr2d_href"};
    sc_signal<bool> nr2d_vsync{"nr2d_vsync"};
    sc_signal<uint8_t> nr2d_y{"nr2d_y"};
    sc_signal<uint8_t> nr2d_u{"nr2d_u"};
    sc_signal<uint8_t> nr2d_v{"nr2d_v"};

    //=============================================================================
    // Internal processing
    //=============================================================================
    void clock_edge() {
        if (!rst_n.read()) {
            m_frame_count = 0;
            m_prev_vsync = false;
            return;
        }

        // Count frames
        bool curr_vsync = in_vsync.read();
        if (m_prev_vsync && !curr_vsync) {
            m_frame_count++;
        }
        m_prev_vsync = curr_vsync;

        // Frame processing will be done by individual blocks
        // This is just for counting
    }

    //=============================================================================
    // Placeholder for block wiring
    // Will be implemented when blocks are created
    //=============================================================================
    void bind_blocks();
    void process_input();
    void process_output();
};

//=============================================================================
// Instantiation helper for sc_module
//=============================================================================
template<unsigned int BITS>
using isp_top_10b = isp_top<10>;

#endif // ISP_TOP_H
