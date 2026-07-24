/*
 * BLC Testbench - Account for 1-cycle latency
 */

#include <systemc>
#include <iostream>
#include <fstream>
#include <vector>
#include "blocks/blc/isp_blc.h"
#include "blocks/blc/blc_metrics.h"

using namespace sc_core;
using namespace std;

class ConfigDriver : public sc_module {
public:
    sc_in<bool> pclk;
    sc_out<bool> rst_n_o, enable_o, linear_en_o;
    sc_out<uint16_t> blc_r_o, blc_gr_o, blc_gb_o, blc_b_o;
    sc_out<uint16_t> linear_r_o, linear_gr_o, linear_gb_o, linear_b_o;

    bool m_rst_n = false, m_enable = false, m_linear_en = false;
    uint16_t m_blc_r = 0, m_blc_gr = 0, m_blc_gb = 0, m_blc_b = 0;
    uint16_t m_linear_r = 1024, m_linear_gr = 1024, m_linear_gb = 1024, m_linear_b = 1024;

    SC_HAS_PROCESS(ConfigDriver);

    ConfigDriver(sc_module_name n) : sc_module(n) {
        SC_METHOD(drive_method);
        sensitive << pclk.pos();
        dont_initialize();
    }

    void drive_method() {
        rst_n_o.write(m_rst_n);
        enable_o.write(m_enable);
        linear_en_o.write(m_linear_en);
        blc_r_o.write(m_blc_r);
        blc_gr_o.write(m_blc_gr);
        blc_gb_o.write(m_blc_gb);
        blc_b_o.write(m_blc_b);
        linear_r_o.write(m_linear_r);
        linear_gr_o.write(m_linear_gr);
        linear_gb_o.write(m_linear_gb);
        linear_b_o.write(m_linear_b);
    }
};

class BlcTestbench : public sc_module {
public:
    sc_clock pclk{"pclk", sc_time(10, SC_NS)};
    isp_blc<10> dut{"blc_dut"};
    ConfigDriver cfg{"cfg"};

    sc_signal<bool> rst_n{"rst_n"}, enable{"enable"};
    sc_signal<bool> i_href{"i_href"}, i_vsync{"i_vsync"};
    sc_signal<uint16_t> i_data{"i_data"};
    sc_signal<bool> o_href{"o_href"}, o_vsync{"o_vsync"};
    sc_signal<uint16_t> o_data{"o_data"};

    sc_signal<uint16_t> blc_r{"blc_r"}, blc_gr{"blc_gr"};
    sc_signal<uint16_t> blc_gb{"blc_gb"}, blc_b{"blc_b"};
    sc_signal<bool> linear_en{"linear_en"};
    sc_signal<uint16_t> linear_r{"linear_r"}, linear_gr{"linear_gr"};
    sc_signal<uint16_t> linear_gb{"linear_gb"}, linear_b{"linear_b"};

    unsigned errors = 0, verified = 0;

    SC_HAS_PROCESS(BlcTestbench);

    BlcTestbench(const sc_module_name& n) : sc_module(n), errors(0), verified(0) {
        cfg.pclk(pclk);
        dut.pclk(pclk); dut.rst_n(rst_n); dut.enable(enable);

        cfg.rst_n_o(rst_n); cfg.enable_o(enable); cfg.linear_en_o(linear_en);
        cfg.blc_r_o(blc_r); cfg.blc_gr_o(blc_gr);
        cfg.blc_gb_o(blc_gb); cfg.blc_b_o(blc_b);
        cfg.linear_r_o(linear_r); cfg.linear_gr_o(linear_gr);
        cfg.linear_gb_o(linear_gb); cfg.linear_b_o(linear_b);

        dut.i_href(i_href); dut.i_vsync(i_vsync); dut.i_data(i_data);
        dut.o_href(o_href); dut.o_vsync(o_vsync); dut.o_data(o_data);

        dut.i_blc_r(blc_r); dut.i_blc_gr(blc_gr);
        dut.i_blc_gb(blc_gb); dut.i_blc_b(blc_b);
        dut.i_linear_en(linear_en);
        dut.i_linear_r(linear_r); dut.i_linear_gr(linear_gr);
        dut.i_linear_gb(linear_gb); dut.i_linear_b(linear_b);

        SC_THREAD(run_tests);
    }

    unsigned channel(unsigned x, unsigned y) {
        bool ey = (y & 1) == 0, ex = (x & 1) == 0;
        return ey ? (ex ? 0 : 1) : (ex ? 2 : 3);
    }

    void run_tests() {
        cout << "\n=== BLC Test (1-cycle latency) ===" << endl;

        dut.set_image_size(640, 480);
        cfg.m_blc_r = cfg.m_blc_gr = cfg.m_blc_gb = cfg.m_blc_b = 64;
        cfg.m_enable = true;
        cfg.m_rst_n = true;

        // Wait for config
        for (int i = 0; i < 5; i++) wait(pclk.posedge_event());
        cout << "Config: blc_r=" << blc_r << endl;

        // Test 1: offset=64
        cout << "\n--- Test 1: offset=64 ---" << endl;

        // VSYNC
        i_vsync = 1; wait(pclk.posedge_event());
        i_vsync = 0; wait(pclk.posedge_event());

        unsigned ok = 0, err = 0;
        unsigned img_w = dut.get_width();
        unsigned img_h = dut.get_height();
        static std::vector<uint16_t> exp_buf;
        if (exp_buf.size() < img_w * img_h) exp_buf.resize(img_w * img_h);

        for (unsigned y = 0; y < img_h; y++) {
            i_href = 1;
            for (unsigned x = 0; x < img_w; x++) {
                unsigned idx = y * img_w + x;
                uint16_t in = 100 + (idx & 0xFF);
                uint16_t exp = (in > 64) ? (in - 64) : 0;
                exp_buf[idx] = exp;

                i_data = in;
                wait(pclk.posedge_event());

                // DUT output is processed value from PREVIOUS pixel
                if (o_href && idx > 0) {
                    uint16_t got = o_data;
                    if (got == exp_buf[idx - 1]) ok++;
                    else {
                        if (err < 5) cerr << "  ERR x=" << x << " y=" << y
                                           << " in=" << in << " exp=" << exp_buf[idx-1] << " got=" << got << endl;
                        err++;
                    }
                }
            }
            i_href = 0;
            wait(pclk.posedge_event());
        }

        cout << "  Result: " << ok << "/" << (img_w * img_h - 1) << " OK, " << err << " errors" << endl;
        verified += ok; errors += err;

        wait(10, SC_US);

        // Test 2: linear
        cout << "\n--- Test 2: linear gain=1.125x ---" << endl;
        cfg.m_linear_en = true;
        cfg.m_linear_r = cfg.m_linear_gr = cfg.m_linear_gb = cfg.m_linear_b = 1152;
        for (int i = 0; i < 5; i++) wait(pclk.posedge_event());

        i_vsync = 1; wait(pclk.posedge_event());
        i_vsync = 0; wait(pclk.posedge_event());

        ok = 0; err = 0;
        for (unsigned y = 0; y < img_h; y++) {
            i_href = 1;
            for (unsigned x = 0; x < img_w; x++) {
                unsigned idx = y * img_w + x;
                uint16_t in = 100 + (idx & 0xFF);
                uint32_t r = ((uint32_t)in * 1152) >> 10;
                uint16_t exp = (r > 1023) ? 1023 : (uint16_t)r;
                exp_buf[idx] = exp;

                i_data = in;
                wait(pclk.posedge_event());

                if (o_href && idx > 0) {
                    uint16_t got = o_data;
                    if (got == exp_buf[idx - 1]) ok++;
                    else {
                        if (err < 5) cerr << "  ERR x=" << x << " y=" << y
                                           << " in=" << in << " exp=" << exp_buf[idx-1] << " got=" << got << endl;
                        err++;
                    }
                }
            }
            i_href = 0;
            wait(pclk.posedge_event());
        }

        cout << "  Result: " << ok << "/" << (img_w * img_h - 1) << " OK, " << err << " errors" << endl;
        verified += ok; errors += err;

        cout << "\n=== Total: " << verified << " OK, " << errors << " errors ===" << endl;

        dut.get_metrics().print_summary();

        ofstream j("blc_metrics.json");
        if (j) j << dut.get_metrics().to_json();

        wait(10, SC_US);
        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    cout << "BLC Testbench" << endl;

    BlcTestbench tb("tb");

    sc_trace_file* tf = sc_create_vcd_trace_file("blc_trace");
    if (tf) {
        tf->set_time_unit(1, SC_NS);
        sc_trace(tf, tb.pclk, "pclk");
        sc_trace(tf, tb.rst_n, "rst_n");
        sc_trace(tf, tb.enable, "enable");
        sc_trace(tf, tb.i_vsync, "i_vsync");
        sc_trace(tf, tb.i_href, "i_href");
        sc_trace(tf, tb.i_data, "i_data");
        sc_trace(tf, tb.o_href, "o_href");
        sc_trace(tf, tb.o_data, "o_data");
        sc_trace(tf, tb.linear_en, "linear_en");
        sc_trace(tf, tb.blc_r, "blc_r");
    }

    sc_start(30, SC_MS);
    if (tf) sc_close_vcd_trace_file(tf);

    return tb.errors > 0;
}
