/*
 * ISP_TOP Testbench
 * Tests the isp_top module with all blocks integrated
 *
 * Usage:
 *   make test-isp_top         - Build and run with default config (64x32, 2 frames)
 *   make run-isp_top-64x32   - Run at 64x32 resolution
 *   ./isp_top_tb -w 1920 -h 1080 -f 1  - Custom resolution
 */

#include <systemc>
#include <iostream>
#include <iomanip>

using namespace sc_core;
using namespace sc_dt;

//=============================================================================
// Constants
//=============================================================================
const unsigned BITS = 10;
const unsigned DEFAULT_WIDTH = 64;
const unsigned DEFAULT_HEIGHT = 32;
const unsigned DEFAULT_FRAMES = 2;

//=============================================================================
// Include ISP Components
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
#include "pipeline/isp_top.h"
#include "metrics/isp_metrics_base.h"

//=============================================================================
// Metrics Collector
//=============================================================================
struct IspMetrics {
    uint64_t total_cycles;
    uint64_t input_pixels;
    uint64_t output_pixels;
    uint64_t frames_processed;
    double throughput_fps;
    double pixels_per_cycle;

    void reset() {
        total_cycles = 0;
        input_pixels = 0;
        output_pixels = 0;
        frames_processed = 0;
        throughput_fps = 0.0;
        pixels_per_cycle = 0.0;
    }

    void calculate(double elapsed_ns, unsigned width, unsigned height, unsigned frames) {
        double elapsed_s = elapsed_ns / 1e9;
        if (elapsed_s > 0) {
            throughput_fps = frames / elapsed_s;
        }
        if (total_cycles > 0) {
            pixels_per_cycle = (double)(width * height * frames) / total_cycles;
        }
    }
};

//=============================================================================
// ISP_TOP Testbench
//=============================================================================
class IspTopTestbench : public sc_module {
public:
    // Clock and reset
    sc_in<bool> i_clk{"i_clk"};

    // DUT instance
    isp_top<BITS, BayerPattern::RGGB, DEFAULT_WIDTH, DEFAULT_HEIGHT>* dut;

    // Configuration
    unsigned cfg_width;
    unsigned cfg_height;
    unsigned cfg_frames;

    // Metrics
    IspMetrics metrics;

    IspTopTestbench(const sc_module_name& name)
        : sc_module(name)
        , cfg_width(DEFAULT_WIDTH)
        , cfg_height(DEFAULT_HEIGHT)
        , cfg_frames(DEFAULT_FRAMES)
    {
        // Instantiate DUT
        dut = new isp_top<BITS, BayerPattern::RGGB, DEFAULT_WIDTH, DEFAULT_HEIGHT>("dut");

        // Bind all ports immediately (required by SystemC elaboration)
        bind_ports();

        // Register processes
        SC_THREAD(test_process);
        sensitive << i_clk.pos();

        // Constructor: Don't create reset_process separately
    }

    void set_config(unsigned width, unsigned height, unsigned frames) {
        cfg_width = width;
        cfg_height = height;
        cfg_frames = frames;
        dut->set_image_size(width, height);
    }

private:
    unsigned pixel_count;
    unsigned output_count;
    unsigned frame_count;
    sc_time sim_start;
    sc_time sim_end;

    // Internal signals for connecting DUT
    sc_signal<bool> rst_n{"rst_n"};

    // Input signals (RAW from sensor)
    sc_signal<bool> s_in_href{"s_in_href"};
    sc_signal<bool> s_in_vsync{"s_in_vsync"};
    sc_signal<uint16_t> s_in_raw{"s_in_raw"};

    // Output signals (YUV) - connected to DUT outputs
    sc_signal<bool> s_out_href{"s_out_href"};
    sc_signal<bool> s_out_vsync{"s_out_vsync"};
    sc_signal<uint8_t> s_out_y{"s_out_y"};
    sc_signal<uint8_t> s_out_u{"s_out_u"};
    sc_signal<uint8_t> s_out_v{"s_out_v"};

    // Enable signals
    sc_signal<bool> crop_en{"crop_en"}, dpc_en{"dpc_en"}, blc_en{"blc_en"};
    sc_signal<bool> oecf_en{"oecf_en"}, dgain_en{"dgain_en"}, lsc_en{"lsc_en"};
    sc_signal<bool> bnr_en{"bnr_en"}, wb_en{"wb_en"}, demosaic_en{"demosaic_en"};
    sc_signal<bool> ccm_en{"ccm_en"}, gamma_en{"gamma_en"}, csc_en{"csc_en"};
    sc_signal<bool> ldci_en{"ldci_en"}, sharpen_en{"sharpen_en"}, nr2d_en{"nr2d_en"};
    sc_signal<bool> awb_en{"awb_en"}, ae_en{"ae_en"};

    // BLC params
    sc_signal<uint16_t> blc_r{"blc_r"}, blc_gr{"blc_gr"};
    sc_signal<uint16_t> blc_gb{"blc_gb"}, blc_b{"blc_b"};
    sc_signal<bool> linear_en{"linear_en"};
    sc_signal<uint16_t> linear_r{"linear_r"}, linear_gr{"linear_gr"};
    sc_signal<uint16_t> linear_gb{"linear_gb"}, linear_b{"linear_b"};

    // DPC params
    sc_signal<uint16_t> dpc_threshold{"dpc_threshold"};

    // CROP params
    sc_signal<uint16_t> crop_w{"crop_w"}, crop_h{"crop_h"};
    sc_signal<uint16_t> crop_x{"crop_x"}, crop_y{"crop_y"};

    // WB params
    sc_signal<uint16_t> wb_rgain{"wb_rgain"}, wb_bgain{"wb_bgain"};

    // DGain params
    sc_signal<bool> dgain_manual{"dgain_manual"};
    sc_signal<uint8_t> dgain_manual_index{"dgain_manual_index"};
    sc_signal<uint8_t> dgain_ae_feedback_index{"dgain_ae_feedback_index"};
    sc_signal<uint8_t> dgain_array[100];

    // CCM params
    sc_signal<int16_t> ccm_rr{"ccm_rr"}, ccm_rg{"ccm_rg"}, ccm_rb{"ccm_rb"};
    sc_signal<int16_t> ccm_gr{"ccm_gr"}, ccm_gg{"ccm_gg"}, ccm_gb{"ccm_gb"};
    sc_signal<int16_t> ccm_br{"ccm_br"}, ccm_bg{"ccm_bg"}, ccm_bb{"ccm_bb"};

    // CSC params
    sc_signal<uint8_t> csc_conv_std{"csc_conv_std"};

    // Sharpen params
    sc_signal<uint16_t> sharpen_strength{"sharpen_strength"};

    // AWB/AE params
    sc_signal<uint16_t> awb_underexp{"awb_underexp"}, awb_overexp{"awb_overexp"};
    sc_signal<uint8_t> awb_frames{"awb_frames"};
    sc_signal<uint8_t> ae_center_illum{"ae_center_illum"};
    sc_signal<uint16_t> ae_crop_left{"ae_crop_left"}, ae_crop_right{"ae_crop_right"};
    sc_signal<uint16_t> ae_crop_top{"ae_crop_top"}, ae_crop_bottom{"ae_crop_bottom"};
    sc_signal<uint16_t> ae_target_skewness{"ae_target_skewness"};

    // AWB/AE output monitoring signals
    sc_signal<uint16_t> awb_r_gain_signal{"awb_r_gain_signal"};
    sc_signal<uint16_t> awb_b_gain_signal{"awb_b_gain_signal"};
    sc_signal<int8_t> ae_response_signal{"ae_response_signal"};
    sc_signal<uint16_t> ae_result_skewness_signal{"ae_result_skewness_signal"};
    sc_signal<bool> ae_done_signal{"ae_done_signal"};

    // DGain output monitoring
    sc_signal<uint8_t> dgain_applied_index_signal{"dgain_applied_index_signal"};

    void bind_ports() {
        // Clock and reset
        dut->pclk(i_clk);
        dut->rst_n(rst_n);

        // Inputs (RAW from sensor)
        dut->in_href(s_in_href);
        dut->in_vsync(s_in_vsync);
        dut->in_raw(s_in_raw);

        // Outputs (YUV)
        dut->out_href(s_out_href);
        dut->out_vsync(s_out_vsync);
        dut->out_y(s_out_y);
        dut->out_u(s_out_u);
        dut->out_v(s_out_v);

        // Enables
        dut->crop_en(crop_en);
        dut->dpc_en(dpc_en);
        dut->blc_en(blc_en);
        dut->oecf_en(oecf_en);
        dut->dgain_en(dgain_en);
        dut->lsc_en(lsc_en);
        dut->bnr_en(bnr_en);
        dut->wb_en(wb_en);
        dut->demosaic_en(demosaic_en);
        dut->ccm_en(ccm_en);
        dut->gamma_en(gamma_en);
        dut->csc_en(csc_en);
        dut->ldci_en(ldci_en);
        dut->sharpen_en(sharpen_en);
        dut->nr2d_en(nr2d_en);
        dut->awb_en(awb_en);
        dut->ae_en(ae_en);

        // BLC params
        dut->blc_r(blc_r);
        dut->blc_gr(blc_gr);
        dut->blc_gb(blc_gb);
        dut->blc_b(blc_b);
        dut->linear_en(linear_en);
        dut->linear_r(linear_r);
        dut->linear_gr(linear_gr);
        dut->linear_gb(linear_gb);
        dut->linear_b(linear_b);

        // DPC params
        dut->dpc_threshold(dpc_threshold);

        // CROP params
        dut->crop_w(crop_w);
        dut->crop_h(crop_h);
        dut->crop_x(crop_x);
        dut->crop_y(crop_y);

        // DGain params
        dut->dgain_manual(dgain_manual);
        dut->dgain_manual_index(dgain_manual_index);
        dut->dgain_ae_feedback_index(dgain_ae_feedback_index);
        for (unsigned i = 0; i < 100; i++) {
            dut->dgain_array[i](dgain_array[i]);
        }

        // WB params
        dut->wb_rgain(wb_rgain);
        dut->wb_bgain(wb_bgain);

        // CCM params
        dut->ccm_rr(ccm_rr); dut->ccm_rg(ccm_rg); dut->ccm_rb(ccm_rb);
        dut->ccm_gr(ccm_gr); dut->ccm_gg(ccm_gg); dut->ccm_gb(ccm_gb);
        dut->ccm_br(ccm_br); dut->ccm_bg(ccm_bg); dut->ccm_bb(ccm_bb);

        // CSC params
        dut->csc_conv_standard(csc_conv_std);
        dut->sharpen_strength(sharpen_strength);

        // AWB/AE params
        dut->awb_underexp(awb_underexp);
        dut->awb_overexp(awb_overexp);
        dut->awb_frames(awb_frames);
        dut->ae_center_illum(ae_center_illum);
        dut->ae_crop_left(ae_crop_left);
        dut->ae_crop_right(ae_crop_right);
        dut->ae_crop_top(ae_crop_top);
        dut->ae_crop_bottom(ae_crop_bottom);
        dut->ae_target_skewness(ae_target_skewness);

        // AWB/AE outputs (monitored only - bind to signal)
        dut->awb_r_gain(awb_r_gain_signal);
        dut->awb_b_gain(awb_b_gain_signal);
        dut->ae_response(ae_response_signal);
        dut->ae_result_skewness(ae_result_skewness_signal);
        dut->ae_done(ae_done_signal);

        // DGain output
        dut->dgain_applied_index(dgain_applied_index_signal);
    }

    void init_defaults() {
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

        blc_r.write(0);
        blc_gr.write(0);
        blc_gb.write(0);
        blc_b.write(0);
        linear_en.write(false);

        dpc_threshold.write(64);

        crop_w.write(DEFAULT_WIDTH);
        crop_h.write(DEFAULT_HEIGHT);
        crop_x.write(0);
        crop_y.write(0);

        wb_rgain.write(1024);
        wb_bgain.write(1024);

        dgain_manual.write(false);
        dgain_manual_index.write(0);
        dgain_ae_feedback_index.write(0);
        for (unsigned i = 0; i < 100; i++) {
            dgain_array[i].write(256);
        }

        ccm_rr.write(256); ccm_rg.write(0); ccm_rb.write(0);
        ccm_gr.write(0); ccm_gg.write(256); ccm_gb.write(0);
        ccm_br.write(0); ccm_bg.write(0); ccm_bb.write(256);

        csc_conv_std.write(0);
        sharpen_strength.write(256);

        awb_underexp.write(64);
        awb_overexp.write(960);
        awb_frames.write(1);

        ae_center_illum.write(128);
        ae_crop_left.write(0);
        ae_crop_right.write(0);
        ae_crop_top.write(0);
        ae_crop_bottom.write(0);
        ae_target_skewness.write(0);
    }


    void test_process() {
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "ISP_TOP MODULE TEST" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Configuration: " << cfg_width << "x" << cfg_height
                  << ", " << cfg_frames << " frames" << std::endl;

        // === Reset sequence ===
        rst_n.write(false);
        s_in_href.write(false);
        s_in_vsync.write(true);
        s_in_raw.write(0);
        wait(40);  // Hold reset

        rst_n.write(true);
        wait(20);

        // Initialize all defaults
        init_defaults();

        sim_start = sc_time_stamp();
        pixel_count = 0;
        output_count = 0;
        frame_count = 0;

        // Send frames
        for (unsigned f = 0; f < cfg_frames; f++) {
            std::cout << "\nSending frame " << (f + 1) << "/" << cfg_frames << "..." << std::endl;
            send_frame(cfg_width, cfg_height);
        }

        // Wait for pipeline to flush
        wait(2000);

        sim_end = sc_time_stamp();

        print_metrics();

        sc_stop();
    }

    void send_frame(unsigned width, unsigned height) {
        // VSYNC pulse (active low)
        s_in_vsync.write(false);
        wait();

        for (unsigned y = 0; y < height; y++) {
            s_in_href.write(false);
            wait();

            for (unsigned x = 0; x < width; x++) {
                uint16_t pixel = generate_bayer(x, y);
                s_in_raw.write(pixel);
                s_in_href.write(true);
                pixel_count++;
                wait();
            }
            s_in_href.write(false);
        }

        s_in_vsync.write(true);
        wait();
        frame_count++;
    }

    uint16_t generate_bayer(unsigned x, unsigned y) {
        bool odd_y = (y & 1);
        bool odd_x = (x & 1);
        uint16_t base = 256 + ((x + y) % 256);
        if (odd_y) {
            return odd_x ? (base + 128) : base;
        } else {
            return odd_x ? base : (base + 128);
        }
    }

    void print_metrics() {
        sc_time elapsed = sim_end - sim_start;
        double elapsed_ns = elapsed.to_double() / 1000.0;
        double elapsed_ms = elapsed_ns / 1e6;

        metrics.total_cycles = (uint64_t)(elapsed_ns / 10.0);  // 10ns clock
        metrics.input_pixels = pixel_count;
        metrics.output_pixels = output_count;
        metrics.frames_processed = frame_count;
        metrics.calculate(elapsed_ns, cfg_width, cfg_height, cfg_frames);

        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "PERFORMANCE METRICS" << std::endl;
        std::cout << std::string(70, '=') << std::endl;

        std::cout << "\n[TIMING]" << std::endl;
        std::cout << "  Total Cycles:      " << std::setw(12) << metrics.total_cycles << std::endl;
        std::cout << "  Elapsed Time:     " << std::fixed << std::setprecision(3)
                  << elapsed_ms << " ms" << std::endl;

        std::cout << "\n[THROUGHPUT]" << std::endl;
        std::cout << "  Throughput:        " << std::fixed << std::setprecision(2)
                  << metrics.throughput_fps << " FPS" << std::endl;
        std::cout << "  Pixels/Cycle:     " << std::fixed << std::setprecision(4)
                  << metrics.pixels_per_cycle << std::endl;
        std::cout << "  Input Pixels:     " << std::setw(12) << metrics.input_pixels << std::endl;
        std::cout << "  Frames:           " << metrics.frames_processed << std::endl;

        std::cout << "\n[BLOCK METRICS]" << std::endl;
        print_block_metrics();

        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "TEST COMPLETE" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
    }

    void print_block_metrics() {
        // CROP
        std::cout << "\n  [CROP]" << std::endl;
        auto& crop_m = dut->get_crop_metrics().get_crop_metrics();
        std::cout << "    Input Pixels:    " << std::setw(12) << crop_m.input_pixels << std::endl;
        std::cout << "    Output Pixels:   " << std::setw(12) << crop_m.output_pixels << std::endl;
        std::cout << "    Cropped Pixels:  " << std::setw(12) << crop_m.cropped_pixels << std::endl;

        // BLC
        std::cout << "\n  [BLC]" << std::endl;
        auto& blc_m = dut->get_blc_metrics().get_blc_metrics();
        std::cout << "    Max Value:       " << std::setw(12) << blc_m.max_value_seen << std::endl;
        std::cout << "    Min Value:       " << std::setw(12) << blc_m.min_value_seen << std::endl;

        // AWB
        std::cout << "\n  [AWB]" << std::endl;
        auto& awb_m = dut->get_awb_metrics().get_awb_metrics();
        std::cout << "    Valid Pixels:     " << std::setw(12) << awb_m.valid_pixels << std::endl;
    }
};

//=============================================================================
// Main
//=============================================================================
int sc_main(int argc, char* argv[]) {
    std::cout << "\n";
    std::cout << "************************************************************\n";
    std::cout << "* ISP_TOP TLM Simulation                                      *\n";
    std::cout << "* SystemC " << SC_VERSION << "                              *\n";
    std::cout << "************************************************************\n";

    // Parse arguments
    unsigned width = DEFAULT_WIDTH;
    unsigned height = DEFAULT_HEIGHT;
    unsigned frames = DEFAULT_FRAMES;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-w" && i + 1 < argc) {
            width = std::atoi(argv[++i]);
        } else if (arg == "-h" && i + 1 < argc) {
            height = std::atoi(argv[++i]);
        } else if (arg == "-f" && i + 1 < argc) {
            frames = std::atoi(argv[++i]);
        }
    }

    // Create module hierarchy
    sc_clock clk("clk", 10, SC_NS);
    IspTopTestbench tb("tb");
    tb.i_clk(clk);

    // Configure
    tb.set_config(width, height, frames);

    std::cout << "\nConfiguration: " << width << "x" << height
              << ", " << frames << " frame(s)\n\n";

    sc_start();

    std::cout << "\nSimulation finished.\n";

    return 0;
}
