/*
 * AXI4-Lite Wrapper for ISP Pipeline
 * Provides register interface for configuration
 * Matches RTL: infinite_isp_AXI_wrapper.v
 */

#ifndef ISP_AXI_WRAPPER_H
#define ISP_AXI_WRAPPER_H

#include <systemc>
#include <tlm>
#include "common/common_defs.h"
#include "common/isp_types.h"

//=============================================================================
// AXI4-Lite Address Map
//=============================================================================
namespace axi_addr {
    constexpr uint32_t CONFIG_BASE     = 0x0000;
    constexpr uint32_t DPC_BASE         = 0x0100;
    constexpr uint32_t BLC_BASE        = 0x0200;
    constexpr uint32_t AE_BASE          = 0x0300;
    constexpr uint32_t DGAIN_BASE      = 0x0400;
    constexpr uint32_t LSC_BASE        = 0x0500;
    constexpr uint32_t AWB_BASE         = 0x0600;
    constexpr uint32_t WB_BASE         = 0x0700;
    constexpr uint32_t CFA_BASE        = 0x0800;
    constexpr uint32_t CCM_BASE        = 0x0900;
    constexpr uint32_t CSC_BASE         = 0x0A00;
    constexpr uint32_t OECF_BASE       = 0x0B00;
    constexpr uint32_t SHARP_BASE      = 0x0E00;
    constexpr uint32_t BNR_BASE        = 0x1000;
    constexpr uint32_t NR2D_BASE       = 0x1500;

    constexpr uint32_t RESET           = 0x00;
    constexpr uint32_t SNS_WIDTH        = 0x04;
    constexpr uint32_t SNS_HEIGHT       = 0x08;
    constexpr uint32_t CROP_W          = 0x0C;
    constexpr uint32_t CROP_H          = 0x10;
    constexpr uint32_t CROP_X          = 0x14;
    constexpr uint32_t CROP_Y          = 0x18;
    constexpr uint32_t BITS            = 0x1C;
    constexpr uint32_t BAYER            = 0x20;
    constexpr uint32_t EN               = 0x24;
}

//=============================================================================
// ISP AXI Register Map
//=============================================================================
struct IspRegisters {
    // CONFIG (0x0000-0x00FF)
    uint32_t reset;
    uint32_t sns_width;
    uint32_t sns_height;
    uint32_t crop_w;
    uint32_t crop_h;
    uint32_t crop_x;
    uint32_t crop_y;
    uint32_t bits;
    uint32_t bayer;
    uint32_t enable;

    // DPC (0x0100-0x01FF)
    uint32_t dpc_threshold;

    // BLC (0x0200-0x02FF)
    uint32_t blc_r, blc_gr, blc_gb, blc_b;
    uint32_t linear_en;
    uint32_t linear_r, linear_gr, linear_gb, linear_b;

    // AE (0x0300-0x03FF)
    uint32_t ae_center_illum;
    uint32_t ae_skewness;
    uint32_t ae_response;
    uint32_t ae_done;

    // DGAIN (0x0400-0x04FF)
    uint32_t dgain_is_manual;
    uint32_t dgain_man_index;
    uint32_t dgain_index_out;

    // AWB (0x0600-0x06FF)
    uint32_t awb_under_exp;
    uint32_t awb_over_exp;
    uint32_t awb_frames;
    uint32_t awb_r_gain;
    uint32_t awb_b_gain;

    // WB (0x0700-0x07FF)
    uint32_t wb_r_gain;
    uint32_t wb_b_gain;

    // CCM (0x0900-0x09FF)
    uint32_t ccm_rr, ccm_rg, ccm_rb;
    uint32_t ccm_gr, ccm_gg, ccm_gb;
    uint32_t ccm_br, ccm_bg, ccm_bb;

    // CSC (0x0A00-0x0AFF)
    uint32_t csc_conv_std;

    // SHARP (0x0E00-0x0EFF)
    uint32_t sharp_strength;

    void reset() {
        reset = 0;
        sns_width = 2048;
        sns_height = 1536;
        crop_w = 2048;
        crop_h = 1536;
        crop_x = 0;
        crop_y = 0;
        bits = 10;
        bayer = 0;
        enable = 0xFFFF;  // All enabled

        dpc_threshold = 64;

        blc_r = blc_gr = blc_gb = blc_b = 0;
        linear_en = 0;
        linear_r = linear_gr = linear_gb = linear_b = 4096;

        ae_center_illum = 128;
        ae_skewness = 0;
        ae_response = 0;
        ae_done = 0;

        dgain_is_manual = 1;
        dgain_man_index = 50;
        dgain_index_out = 0;

        awb_under_exp = 64;
        awb_over_exp = 960;
        awb_frames = 1;
        awb_r_gain = 1024;
        awb_b_gain = 1024;

        wb_r_gain = 1024;
        wb_b_gain = 1024;

        ccm_rr = 256; ccm_rg = 0; ccm_rb = 0;
        ccm_gr = 0; ccm_gg = 256; ccm_gb = 0;
        ccm_br = 0; ccm_bg = 0; ccm_bb = 256;

        csc_conv_std = 0;

        sharp_strength = 256;
    }
};

//=============================================================================
// ISP AXI Wrapper Module
//=============================================================================
class isp_axi_wrapper : public sc_module {
public:
    // Clock and Reset
    sc_in<bool> aclk{"aclk"};
    sc_in<bool> aresetn{"aresetn"};

    // AXI4-Lite Write Address Channel
    sc_in<uint32_t> s_awaddr{"s_awaddr"};
    sc_in<uint32_t> s_awprot{"s_awprot"};
    sc_in<bool> s_awvalid{"s_awvalid"};
    sc_out<bool> s_awready{"s_awready"};

    // AXI4-Lite Write Data Channel
    sc_in<uint32_t> s_wdata{"s_wdata"};
    sc_in<uint32_t> s_wstrb{"s_wstrb"};
    sc_in<bool> s_wvalid{"s_wvalid"};
    sc_out<bool> s_wready{"s_wready"};

    // AXI4-Lite Write Response Channel
    sc_out<uint32_t> s_bresp{"s_bresp"};
    sc_out<bool> s_bvalid{"s_bvalid"};
    sc_in<bool> s_bready{"s_bready"};

    // AXI4-Lite Read Address Channel
    sc_in<uint32_t> s_araddr{"s_araddr"};
    sc_in<uint32_t> s_arprot{"s_arprot"};
    sc_in<bool> s_arvalid{"s_arvalid"};
    sc_out<bool> s_arready{"s_arready"};

    // AXI4-Lite Read Data Channel
    sc_out<uint32_t> s_rdata{"s_rdata"};
    sc_out<uint32_t> s_rresp{"s_rresp"};
    sc_out<bool> s_rvalid{"s_rvalid"};
    sc_in<bool> s_rready{"s_rready"};

    // Register outputs (to ISP blocks)
    sc_out<uint32_t> reg_sns_width{"reg_sns_width"};
    sc_out<uint32_t> reg_sns_height{"reg_sns_height"};
    sc_out<uint32_t> reg_crop_w{"reg_crop_w"};
    sc_out<uint32_t> reg_crop_h{"reg_crop_h"};
    sc_out<uint32_t> reg_bayer{"reg_bayer"};
    sc_out<uint32_t> reg_enable{"reg_enable"};

    sc_out<uint32_t> reg_dpc_threshold{"reg_dpc_threshold"};

    sc_out<uint32_t> reg_blc_r{"reg_blc_r"}, reg_blc_gr{"reg_blc_gr"};
    sc_out<uint32_t> reg_blc_gb{"reg_blc_gb"}, reg_blc_b{"reg_blc_b"};
    sc_out<uint32_t> reg_linear_en{"reg_linear_en"};

    sc_out<uint32_t> reg_wb_rgain{"reg_wb_rgain"}, reg_wb_bgain{"reg_wb_bgain"};

    sc_out<int32_t> reg_ccm_rr{"reg_ccm_rr"}, reg_ccm_rg{"reg_ccm_rg"}, reg_ccm_rb{"reg_ccm_rb"};
    sc_out<int32_t> reg_ccm_gr{"reg_ccm_gr"}, reg_ccm_gg{"reg_ccm_gg"}, reg_ccm_gb{"reg_ccm_gb"};
    sc_out<int32_t> reg_ccm_br{"reg_ccm_br"}, reg_ccm_bg{"reg_ccm_bg"}, reg_ccm_bb{"reg_ccm_bb"};

    sc_out<uint32_t> reg_csc_std{"reg_csc_std"};
    sc_out<uint32_t> reg_sharp_strength{"reg_sharp_strength"};

    sc_out<uint32_t> reg_awb_rgain{"reg_awb_rgain"}, reg_awb_bgain{"reg_awb_bgain"};

    isp_axi_wrapper(const sc_module_name& name)
        : sc_module(name)
        , m_aw_state(IDLE)
        , m_ar_state(IDLE)
    {
        m_regs.reset();

        SC_THREAD(write_proc);
        sensitive << aclk.pos();

        SC_THREAD(read_proc);
        sensitive << aclk.pos();

        SC_THREAD(update_regs);
        sensitive << aclk.pos();
    }

    const IspRegisters& get_registers() const { return m_regs; }

private:
    enum { IDLE, WRITE_ADDR, WRITE_DATA, WRITE_RESP };
    enum { READ_ADDR, READ_DATA };

    IspRegisters m_regs;
    int m_aw_state, m_ar_state;

    void write_proc() {
        s_awready.write(false);
        s_wready.write(false);
        s_bvalid.write(false);
        s_bresp.write(0);

        while (true) {
            wait();

            if (!aresetn.read()) {
                m_aw_state = IDLE;
                s_awready.write(false);
                s_wready.write(false);
                s_bvalid.write(false);
                continue;
            }

            switch (m_aw_state) {
                case IDLE:
                    s_awready.write(true);
                    if (s_awvalid.read()) {
                        m_aw_addr = s_awaddr.read();
                        m_aw_state = WRITE_DATA;
                        s_awready.write(false);
                    }
                    break;

                case WRITE_DATA:
                    s_wready.write(true);
                    if (s_wvalid.read()) {
                        write_register(m_aw_addr, s_wdata.read());
                        m_aw_state = WRITE_RESP;
                        s_wready.write(false);
                    }
                    break;

                case WRITE_RESP:
                    s_bvalid.write(true);
                    s_bresp.write(0);
                    if (s_bready.read()) {
                        s_bvalid.write(false);
                        m_aw_state = IDLE;
                    }
                    break;
            }
        }
    }

    void read_proc() {
        s_arready.write(false);
        s_rvalid.write(false);
        s_rdata.write(0);
        s_rresp.write(0);

        while (true) {
            wait();

            if (!aresetn.read()) {
                m_ar_state = IDLE;
                s_arready.write(false);
                s_rvalid.write(false);
                continue;
            }

            switch (m_ar_state) {
                case IDLE:
                    s_arready.write(true);
                    if (s_arvalid.read()) {
                        m_ar_addr = s_araddr.read();
                        m_ar_data = read_register(m_ar_addr);
                        m_ar_state = READ_DATA;
                        s_arready.write(false);
                    }
                    break;

                case READ_DATA:
                    s_rvalid.write(true);
                    s_rdata.write(m_ar_data);
                    s_rresp.write(0);
                    if (s_rready.read()) {
                        s_rvalid.write(false);
                        m_ar_state = IDLE;
                    }
                    break;
            }
        }
    }

    void update_regs() {
        while (true) {
            wait();

            if (aresetn.read()) {
                reg_sns_width.write(m_regs.sns_width);
                reg_sns_height.write(m_regs.sns_height);
                reg_crop_w.write(m_regs.crop_w);
                reg_crop_h.write(m_regs.crop_h);
                reg_bayer.write(m_regs.bayer);
                reg_enable.write(m_regs.enable);

                reg_dpc_threshold.write(m_regs.dpc_threshold);

                reg_blc_r.write(m_regs.blc_r);
                reg_blc_gr.write(m_regs.blc_gr);
                reg_blc_gb.write(m_regs.blc_gb);
                reg_blc_b.write(m_regs.blc_b);
                reg_linear_en.write(m_regs.linear_en);

                reg_wb_rgain.write(m_regs.wb_r_gain);
                reg_wb_bgain.write(m_regs.wb_b_gain);

                reg_ccm_rr.write(m_regs.ccm_rr);
                reg_ccm_rg.write(m_regs.ccm_rg);
                reg_ccm_rb.write(m_regs.ccm_rb);
                reg_ccm_gr.write(m_regs.ccm_gr);
                reg_ccm_gg.write(m_regs.ccm_gg);
                reg_ccm_gb.write(m_regs.ccm_gb);
                reg_ccm_br.write(m_regs.ccm_br);
                reg_ccm_bg.write(m_regs.ccm_bg);
                reg_ccm_bb.write(m_regs.ccm_bb);

                reg_csc_std.write(m_regs.csc_conv_std);
                reg_sharp_strength.write(m_regs.sharp_strength);

                reg_awb_rgain.write(m_regs.awb_r_gain);
                reg_awb_bgain.write(m_regs.awb_b_gain);
            }
        }
    }

    uint32_t m_aw_addr;
    uint32_t m_ar_addr;
    uint32_t m_ar_data;

    void write_register(uint32_t addr, uint32_t data) {
        uint32_t offset = addr & 0xFF;

        switch (addr & 0xFF00) {
            case axi_addr::CONFIG_BASE:
                switch (offset) {
                    case axi_addr::SNS_WIDTH: m_regs.sns_width = data; break;
                    case axi_addr::SNS_HEIGHT: m_regs.sns_height = data; break;
                    case axi_addr::CROP_W: m_regs.crop_w = data; break;
                    case axi_addr::CROP_H: m_regs.crop_h = data; break;
                    case axi_addr::CROP_X: m_regs.crop_x = data; break;
                    case axi_addr::CROP_Y: m_regs.crop_y = data; break;
                    case axi_addr::BAYER: m_regs.bayer = data; break;
                    case axi_addr::EN: m_regs.enable = data; break;
                }
                break;

            case axi_addr::DPC_BASE:
                m_regs.dpc_threshold = data;
                break;

            case axi_addr::BLC_BASE:
                switch (offset) {
                    case 0x00: m_regs.blc_r = data; break;
                    case 0x04: m_regs.blc_gr = data; break;
                    case 0x08: m_regs.blc_gb = data; break;
                    case 0x0C: m_regs.blc_b = data; break;
                    case 0x10: m_regs.linear_en = data; break;
                    case 0x14: m_regs.linear_r = data; break;
                    case 0x18: m_regs.linear_gr = data; break;
                    case 0x1C: m_regs.linear_gb = data; break;
                    case 0x20: m_regs.linear_b = data; break;
                }
                break;

            case axi_addr::WB_BASE:
                switch (offset) {
                    case 0x00: m_regs.wb_r_gain = data; break;
                    case 0x04: m_regs.wb_b_gain = data; break;
                }
                break;

            case axi_addr::CCM_BASE:
                switch (offset) {
                    case 0x00: m_regs.ccm_rr = data; break;
                    case 0x04: m_regs.ccm_rg = data; break;
                    case 0x08: m_regs.ccm_rb = data; break;
                    case 0x0C: m_regs.ccm_gr = data; break;
                    case 0x10: m_regs.ccm_gg = data; break;
                    case 0x14: m_regs.ccm_gb = data; break;
                    case 0x18: m_regs.ccm_br = data; break;
                    case 0x1C: m_regs.ccm_bg = data; break;
                    case 0x20: m_regs.ccm_bb = data; break;
                }
                break;

            case axi_addr::CSC_BASE:
                m_regs.csc_conv_std = data;
                break;

            case axi_addr::SHARP_BASE:
                m_regs.sharp_strength = data;
                break;
        }
    }

    uint32_t read_register(uint32_t addr) {
        uint32_t offset = addr & 0xFF;

        switch (addr & 0xFF00) {
            case axi_addr::CONFIG_BASE:
                switch (offset) {
                    case axi_addr::SNS_WIDTH: return m_regs.sns_width;
                    case axi_addr::SNS_HEIGHT: return m_regs.sns_height;
                    case axi_addr::CROP_W: return m_regs.crop_w;
                    case axi_addr::CROP_H: return m_regs.crop_h;
                    case axi_addr::BAYER: return m_regs.bayer;
                    case axi_addr::EN: return m_regs.enable;
                }
                break;

            case axi_addr::DPC_BASE:
                return m_regs.dpc_threshold;

            case axi_addr::BLC_BASE:
                switch (offset) {
                    case 0x00: return m_regs.blc_r;
                    case 0x04: return m_regs.blc_gr;
                    case 0x08: return m_regs.blc_gb;
                    case 0x0C: return m_regs.blc_b;
                }
                break;

            case axi_addr::WB_BASE:
                switch (offset) {
                    case 0x00: return m_regs.wb_r_gain;
                    case 0x04: return m_regs.wb_b_gain;
                }
                break;

            case axi_addr::CCM_BASE:
                switch (offset) {
                    case 0x00: return m_regs.ccm_rr;
                    case 0x04: return m_regs.ccm_rg;
                    case 0x08: return m_regs.ccm_rb;
                }
                break;

            case axi_addr::CSC_BASE:
                return m_regs.csc_conv_std;
        }

        return 0;
    }
};

#endif // ISP_AXI_WRAPPER_H
