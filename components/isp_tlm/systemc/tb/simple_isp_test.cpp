/*
 * Simple ISP Pipeline Testbench (BLC + CROP only)
 * Tests basic pipeline functionality
 */

#include <systemc>
#include <iostream>
#include <iomanip>

#ifndef VERILATED_H
#define VERILATED_H
inline void vl_finish(const char* filename, int lineno) {
    std::cout << "Verilator finish at " << filename << ":" << lineno << std::endl;
}
inline void vl_print_dump(FILE* fp) {}
namespace Verilated { inline void commandArgs(int argc, char** argv) {} }
#endif

#include "blocks/blc/isp_blc.h"
#include "blocks/crop/isp_crop.h"

using namespace sc_core;

class SimpleIspTestbench : public sc_module {
public:
    sc_in<bool> pclk{"pclk"};
    sc_signal<bool> rst_n{"rst_n"};

    sc_signal<bool> in_href{"in_href"};
    sc_signal<bool> in_vsync{"in_vsync"};
    sc_signal<uint16_t> in_raw{"in_raw"};

    sc_signal<bool> crop_en{"crop_en"};
    sc_signal<uint16_t> crop_w{"crop_w"}, crop_h{"crop_h"};
    sc_signal<uint16_t> crop_x{"crop_x"}, crop_y{"crop_y"};

    sc_signal<bool> blc_en{"blc_en"};
    sc_signal<uint16_t> blc_r{"blc_r"}, blc_gr{"blc_gr"};
    sc_signal<uint16_t> blc_gb{"blc_gb"}, blc_b{"blc_b"};
    sc_signal<bool> blc_linear_en{"blc_linear_en"};
    sc_signal<uint16_t> blc_linear_r{"blc_linear_r"}, blc_linear_gr{"blc_linear_gr"};
    sc_signal<uint16_t> blc_linear_gb{"blc_linear_gb"}, blc_linear_b{"blc_linear_b"};

    sc_signal<bool> mid_href{"mid_href"}, mid_vsync{"mid_vsync"};
    sc_signal<uint16_t> mid_raw{"mid_raw"};

    sc_signal<bool> out_href{"out_href"}, out_vsync{"out_vsync"};
    sc_signal<uint16_t> out_data{"out_data"};

    isp_crop<10, 64, 32>* crop;
    isp_blc<10>* blc;

    SimpleIspTestbench(const sc_module_name& name) : sc_module(name) {
        crop = new isp_crop<10, 64, 32>("crop");
        crop->pclk(pclk);
        crop->rst_n(rst_n);
        crop->enable(crop_en);
        crop->i_href(in_href);
        crop->i_vsync(in_vsync);
        crop->i_data(in_raw);
        crop->o_href(mid_href);
        crop->o_vsync(mid_vsync);
        crop->o_data(mid_raw);
        crop->i_crop_w(crop_w);
        crop->i_crop_h(crop_h);
        crop->i_crop_x(crop_x);
        crop->i_crop_y(crop_y);

        blc = new isp_blc<10>("blc");
        blc->pclk(pclk);
        blc->rst_n(rst_n);
        blc->enable(blc_en);
        blc->i_href(mid_href);
        blc->i_vsync(mid_vsync);
        blc->i_data(mid_raw);
        blc->i_blc_r(blc_r);
        blc->i_blc_gr(blc_gr);
        blc->i_blc_gb(blc_gb);
        blc->i_blc_b(blc_b);
        blc->i_linear_en(blc_linear_en);
        blc->i_linear_r(blc_linear_r);
        blc->i_linear_gr(blc_linear_gr);
        blc->i_linear_gb(blc_linear_gb);
        blc->i_linear_b(blc_linear_b);
        blc->o_href(out_href);
        blc->o_vsync(out_vsync);
        blc->o_data(out_data);

        SC_THREAD(clk_gen);
        sensitive << pclk.pos();

        SC_THREAD(run_test);
        sensitive << pclk.pos();
    }

    void run_test() {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "Simple ISP Pipeline Test (CROP + BLC)" << std::endl;
        std::cout << std::string(60, '=') << std::endl;

        wait();
        init_signals();
        reset_dut();
        wait();

        std::cout << "\nSending test frame (64x32)..." << std::endl;
        send_frame(64, 32);

        wait(200);
        wait(200);

        std::cout << "\n--- CROP Metrics ---" << std::endl;
        crop->get_metrics().print_summary();

        std::cout << "\n--- BLC Metrics ---" << std::endl;
        blc->get_metrics().print_summary();

        sc_stop();
    }

private:
    sc_signal<bool> clk_sig{"clk_sig"};

    void clk_gen() {
        while (true) {
            clk_sig.write(false);
            wait(5, SC_NS);
            clk_sig.write(true);
            wait(5, SC_NS);
        }
    }

    void init_signals() {
        crop_en.write(true);
        blc_en.write(true);
        crop_w.write(64);
        crop_h.write(32);
        crop_x.write(0);
        crop_y.write(0);
        blc_r.write(10);
        blc_gr.write(10);
        blc_gb.write(10);
        blc_b.write(10);
        blc_linear_en.write(false);
        blc_linear_r.write(0);
        blc_linear_gr.write(0);
        blc_linear_gb.write(0);
        blc_linear_b.write(0);
    }

    void reset_dut() {
        rst_n.write(false);
        in_href.write(false);
        in_vsync.write(true);
        in_raw.write(0);
        wait();
        rst_n.write(true);
        wait();
        std::cout << "Reset complete" << std::endl;
    }

    void send_frame(unsigned width, unsigned height) {
        in_vsync.write(false);
        wait();

        for (unsigned y = 0; y < height; y++) {
            in_href.write(false);
            wait();

            for (unsigned x = 0; x < width; x++) {
                uint16_t val = ((x + y) % 256) * 4;
                in_raw.write(val);
                in_href.write(true);
                wait();
            }
            in_href.write(false);
        }

        in_vsync.write(true);
        std::cout << "Frame sent" << std::endl;
    }
};

int sc_main(int argc, char* argv[]) {
    Verilated::commandArgs(argc, argv);

    sc_clock pclk("pclk", 10, SC_NS);
    SimpleIspTestbench tb("tb");
    tb.pclk(pclk);

    std::cout << "Starting Simple ISP Testbench" << std::endl;
    sc_start(1, SC_MS);

    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Test Complete" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    return 0;
}
