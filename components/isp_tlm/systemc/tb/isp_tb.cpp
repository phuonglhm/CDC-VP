/*
 * ISP Testbench
 * Top-level testbench for ISP TLM model
 */

#include <systemc>
#include <iostream>
#include "pipeline/isp_top.h"
#include "common/isp_params.h"

using namespace sc_core;
using namespace std;

//=============================================================================
// Testbench Module
//=============================================================================
class isp_tb : public sc_module {
public:
    // Clock generator
    sc_clock pclk_gen{"pclk", sc_time(10, SC_NS)};  // 100MHz

    // DUT
    isp_top<10> dut{"dut"};

    // Testbench internal signals
    sc_signal<bool> rst_n{"rst_n"};
    sc_signal<bool> in_href{"in_href"};
    sc_signal<bool> in_vsync{"in_vsync"};
    sc_signal<uint16_t> in_raw{"in_raw"};

    // Module enables
    sc_signal<bool> crop_en{"crop_en"};
    sc_signal<bool> dpc_en{"dpc_en"};
    sc_signal<bool> blc_en{"blc_en"};
    sc_signal<bool> oecf_en{"oecf_en"};
    sc_signal<bool> dgain_en{"dgain_en"};
    sc_signal<bool> lsc_en{"lsc_en"};
    sc_signal<bool> bnr_en{"bnr_en"};
    sc_signal<bool> wb_en{"wb_en"};
    sc_signal<bool> demosic_en{"demosic_en"};
    sc_signal<bool> ccm_en{"ccm_en"};
    sc_signal<bool> gamma_en{"gamma_en"};
    sc_signal<bool> csc_en{"csc_en"};
    sc_signal<bool> ldci_en{"ldci_en"};
    sc_signal<bool> sharpen_en{"sharpen_en"};
    sc_signal<bool> nr2d_en{"nr2d_en"};
    sc_signal<bool> stat_ae_en{"stat_ae_en"};
    sc_signal<bool> awb_en{"awb_en"};
    sc_signal<bool> ae_en{"ae_en"};

    // RGB bypass (disabled)
    sc_signal<bool> rgb_inp_en{"rgb_inp_en"};
    sc_signal<bool> in_href_rgb{"in_href_rgb"};
    sc_signal<bool> in_vsync_rgb{"in_vsync_rgb"};
    sc_signal<uint16_t> in_r{"in_r"};
    sc_signal<uint16_t> in_g{"in_g"};
    sc_signal<uint16_t> in_b{"in_b"};

    // Parameters
    sc_signal<uint16_t> dpc_threshold{"dpc_threshold"};
    sc_signal<uint16_t> blc_r{"blc_r"}, blc_gr{"blc_gr"}, blc_gb{"blc_gb"}, blc_b{"blc_b"};
    sc_signal<bool> linear_en{"linear_en"};
    sc_signal<uint16_t> linear_r{"linear_r"}, linear_gr{"linear_gr"}, linear_gb{"linear_gb"}, linear_b{"linear_b"};
    sc_signal<uint16_t> bnr_space_kernel_r{"bnr_space_kernel_r"};
    sc_signal<bool> dgain_is_manual{"dgain_is_manual"};
    sc_signal<uint8_t> dgain_man_index{"dgain_man_index"};
    sc_signal<uint8_t> dgain_index_out{"dgain_index_out"};
    sc_signal<uint16_t> wb_rgain{"wb_rgain"}, wb_bgain{"wb_bgain"};
    sc_signal<uint16_t> ccm_rr{"ccm_rr"}, ccm_rg{"ccm_rg"}, ccm_rb{"ccm_rb"};
    sc_signal<uint16_t> ccm_gr{"ccm_gr"}, ccm_gg{"ccm_gg"}, ccm_gb{"ccm_gb"};
    sc_signal<uint16_t> ccm_br{"ccm_br"}, ccm_bg{"ccm_bg"}, ccm_bb{"ccm_bb"};
    sc_signal<uint8_t> in_conv_standard{"in_conv_standard"};
    sc_signal<uint16_t> sharpen_strength{"sharpen_strength"};
    sc_signal<uint16_t> final_r_gain{"final_r_gain"};
    sc_signal<uint16_t> final_b_gain{"final_b_gain"};
    sc_signal<uint8_t> ae_response{"ae_response"};
    sc_signal<bool> ae_done{"ae_done"};

    // Outputs
    sc_signal<bool> out_gamma_href{"out_gamma_href"};
    sc_signal<bool> out_gamma_vsync{"out_gamma_vsync"};
    sc_signal<uint16_t> out_gamma_r{"out_gamma_r"};
    sc_signal<uint16_t> out_gamma_g{"out_gamma_g"};
    sc_signal<uint16_t> out_gamma_b{"out_gamma_b"};
    sc_signal<bool> out_href{"out_href"};
    sc_signal<bool> out_vsync{"out_vsync"};
    sc_signal<uint8_t> out_y{"out_y"};
    sc_signal<uint8_t> out_u{"out_u"};
    sc_signal<uint8_t> out_v{"out_v"};

    // Constructor
    isp_tb(const sc_module_name& name)
        : sc_module(name)
    {
        // Bind clock and reset
        dut.pclk(pclk_gen);
        dut.rst_n(rst_n);

        // Bind input
        dut.in_href(in_href);
        dut.in_vsync(in_vsync);
        dut.in_raw(in_raw);

        // Bind RGB bypass
        dut.in_href_rgb(in_href_rgb);
        dut.in_vsync_rgb(in_vsync_rgb);
        dut.in_r(in_r);
        dut.in_g(in_g);
        dut.in_b(in_b);
        dut.rgb_inp_en(rgb_inp_en);

        // Bind enables
        dut.crop_en(crop_en);
        dut.dpc_en(dpc_en);
        dut.blc_en(blc_en);
        dut.oecf_en(oecf_en);
        dut.dgain_en(dgain_en);
        dut.lsc_en(lsc_en);
        dut.bnr_en(bnr_en);
        dut.wb_en(wb_en);
        dut.demosic_en(demosic_en);
        dut.ccm_en(ccm_en);
        dut.gamma_en(gamma_en);
        dut.csc_en(csc_en);
        dut.ldci_en(ldci_en);
        dut.sharpen_en(sharpen_en);
        dut.nr2d_en(nr2d_en);
        dut.stat_ae_en(stat_ae_en);
        dut.awb_en(awb_en);
        dut.ae_en(ae_en);

        // Bind parameters
        dut.dpc_threshold(dpc_threshold);
        dut.blc_r(blc_r);
        dut.blc_gr(blc_gr);
        dut.blc_gb(blc_gb);
        dut.blc_b(blc_b);
        dut.linear_en(linear_en);
        dut.linear_r(linear_r);
        dut.linear_gr(linear_gr);
        dut.linear_gb(linear_gb);
        dut.linear_b(linear_b);
        dut.dgain_is_manual(dgain_is_manual);
        dut.dgain_man_index(dgain_man_index);
        dut.dgain_index_out(dgain_index_out);
        dut.wb_rgain(wb_rgain);
        dut.wb_bgain(wb_bgain);
        dut.ccm_rr(ccm_rr);
        dut.ccm_rg(ccm_rg);
        dut.ccm_rb(ccm_rb);
        dut.ccm_gr(ccm_gr);
        dut.ccm_gg(ccm_gg);
        dut.ccm_gb(ccm_gb);
        dut.ccm_br(ccm_br);
        dut.ccm_bg(ccm_bg);
        dut.ccm_bb(ccm_bb);
        dut.in_conv_standard(in_conv_standard);
        dut.sharpen_strength(sharpen_strength);
        dut.final_r_gain(final_r_gain);
        dut.final_b_gain(final_b_gain);
        dut.ae_response(ae_response);
        dut.ae_done(ae_done);

        // Bind outputs
        dut.out_gamma_href(out_gamma_href);
        dut.out_gamma_vsync(out_gamma_vsync);
        dut.out_gamma_r(out_gamma_r);
        dut.out_gamma_g(out_gamma_g);
        dut.out_gamma_b(out_gamma_b);
        dut.out_href(out_href);
        dut.out_vsync(out_vsync);
        dut.out_y(out_y);
        dut.out_u(out_u);
        dut.out_v(out_v);

        // Register processes
        SC_THREAD(stimulus_thread);
        SC_THREAD(monitor_thread);
    }

    //=============================================================================
    // Stimulus Generation
    //=============================================================================
    void stimulus_thread() {
        // Reset
        rst_n = false;
        in_href = false;
        in_vsync = false;
        in_raw = 0;
        rgb_inp_en = false;

        // Default enables (matching RTL)
        crop_en = true;
        dpc_en = true;
        blc_en = true;
        oecf_en = true;
        dgain_en = true;
        lsc_en = false;
        bnr_en = true;
        wb_en = true;
        demosic_en = true;
        ccm_en = true;
        gamma_en = true;
        csc_en = true;
        ldci_en = false;
        sharpen_en = true;
        nr2d_en = true;
        stat_ae_en = false;
        awb_en = true;
        ae_en = true;

        // Default parameters
        dpc_threshold = 50;
        blc_r = 64;
        blc_gr = 64;
        blc_gb = 64;
        blc_b = 64;
        linear_en = false;
        linear_r = 4096;
        linear_gr = 4096;
        linear_gb = 4096;
        linear_b = 4096;
        dgain_is_manual = true;
        dgain_man_index = 50;
        wb_rgain = 512;
        wb_bgain = 512;
        ccm_rr = 512;
        ccm_rg = 0;
        ccm_rb = 0;
        ccm_gr = 0;
        ccm_gg = 512;
        ccm_gb = 0;
        ccm_br = 0;
        ccm_bg = 0;
        ccm_bb = 512;
        in_conv_standard = 0;
        sharpen_strength = 256;

        wait(100, SC_NS);
        rst_n = true;
        wait(100, SC_NS);

        cout << "Starting frame capture..." << endl;

        // Send one frame of test data
        send_test_frame();

        wait(10, SC_US);

        cout << "Testbench completed." << endl;
        sc_stop();
    }

    void send_test_frame() {
        const unsigned WIDTH = 640;
        const unsigned HEIGHT = 480;

        // VSYNC pulse
        in_vsync = true;
        wait(1, SC_US);
        in_vsync = false;
        wait(1, SC_US);

        // Send pixels
        for (unsigned y = 0; y < HEIGHT; ++y) {
            in_href = true;
            for (unsigned x = 0; x < WIDTH; ++x) {
                // Simple gradient pattern
                uint16_t pixel = (x + y) & 0x3FF;  // 10-bit
                in_raw = pixel;
                wait(100, SC_NS);  // 10ns = 100MHz
            }
            in_href = false;
            wait(100, SC_NS);
        }
    }

    //=============================================================================
    // Output Monitoring
    //=============================================================================
    void monitor_thread() {
        unsigned frame_count = 0;
        unsigned pixel_count = 0;

        wait(rst_n.posedge_event());

        cout << "Monitoring output..." << endl;

        while (true) {
            wait(out_vsync.posedge_event());

            if (out_gamma_vsync.read()) {
                frame_count++;
                cout << "Frame " << frame_count << " started" << endl;
            }

            wait(sc_time(1, SC_US));
        }
    }
};

//=============================================================================
// Main
//=============================================================================
int sc_main(int argc, char* argv[]) {
    cout << "========================================" << endl;
    cout << "ISP TLM SystemC Testbench" << endl;
    cout << "========================================" << endl;

    isp_tb tb("isp_tb");

    // Start simulation
    sc_start(100, SC_MS);

    // Print statistics
    cout << "Simulation completed." << endl;
    cout << "Frames processed: " << tb.dut.get_frame_count() << endl;

    return 0;
}
