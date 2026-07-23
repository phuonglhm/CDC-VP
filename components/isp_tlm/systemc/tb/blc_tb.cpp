/*
 * BLC Testbench - Account for 1-cycle pipeline delay
 */

#include <systemc>
#include <iostream>
#include <fstream>
#include "blocks/blc/isp_blc.h"
#include "blocks/blc/blc_metrics.h"

using namespace sc_core;
using namespace std;

class BlcTestbench : public sc_module {
public:
    sc_clock pclk{"pclk", sc_time(10, SC_NS)};
    isp_blc<10> dut{"blc_dut"};

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
        dut.pclk(pclk); dut.rst_n(rst_n); dut.enable(enable);
        dut.i_href(i_href); dut.i_vsync(i_vsync); dut.i_data(i_data);
        dut.o_href(o_href); dut.o_vsync(o_vsync); dut.o_data(o_data);
        dut.i_blc_r(blc_r); dut.i_blc_gr(blc_gr);
        dut.i_blc_gb(blc_gb); dut.i_blc_b(blc_b);
        dut.i_linear_en(linear_en);
        dut.i_linear_r(linear_r); dut.i_linear_gr(linear_gr);
        dut.i_linear_gb(linear_gb); dut.i_linear_b(linear_b);
        SC_THREAD(run_tests);
    }

    void run_tests() {
        rst_n = false; enable = false;
        i_href = false; i_vsync = false; i_data = 0;
        blc_r = blc_gr = blc_gb = blc_b = 64;
        linear_r = linear_gr = linear_gb = linear_b = 1024;
        linear_en = false;

        wait(pclk.posedge_event());
        rst_n = true; wait(pclk.posedge_event());
        enable = true; wait(pclk.posedge_event());

        cout << "\n=== BLC Test (1-cycle pipeline) ===" << endl;

        // Test 1: Simple offset
        cout << "\n--- Test 1: offset=64 ---" << endl;
        dut.set_image_size(8, 2);
        blc_r = blc_gr = blc_gb = blc_b = 64;
        linear_en = false;
        wait(pclk.posedge_event());
        wait(pclk.posedge_event());

        i_vsync = 1; wait(pclk.posedge_event());
        i_vsync = 0; wait(pclk.posedge_event());

        unsigned ok = 0, err = 0;
        // Pipeline delay: store expected values
        uint16_t expected_buf[16];

        for (unsigned y = 0; y < 2; y++) {
            i_href = 1;
            for (unsigned x = 0; x < 8; x++) {
                unsigned idx = y * 8 + x;
                uint16_t in = 100 + idx;
                // Compute expected (this is what will appear at output 1 cycle later)
                expected_buf[idx] = (in > 64) ? (in - 64) : 0;

                i_data = in;
                wait(pclk.posedge_event());

                // Read output - this is the processed value from PREVIOUS input
                if (o_href && idx > 0) {  // Skip first pixel (pipeline filling)
                    uint16_t got = o_data;
                    uint16_t exp = expected_buf[idx - 1];
                    if (got == exp) ok++;
                    else {
                        if (err < 5) cerr << "  ERR x=" << x << " y=" << y
                                           << " in=" << in << " exp=" << exp << " got=" << got << endl;
                        err++;
                    }
                }
            }
            i_href = 0;
            wait(pclk.posedge_event());
        }
        cout << "  Result: " << ok << " OK, " << err << " errors" << endl;
        verified += ok; errors += err;

        wait(10, SC_US);

        // Test 2: Linear gain
        cout << "\n--- Test 2: linear gain=1.125x ---" << endl;
        linear_en = true;
        linear_r = linear_gr = linear_gb = linear_b = 1152;
        wait(pclk.posedge_event());
        wait(pclk.posedge_event());

        i_vsync = 1; wait(pclk.posedge_event());
        i_vsync = 0; wait(pclk.posedge_event());

        ok = 0; err = 0;
        for (unsigned y = 0; y < 2; y++) {
            i_href = 1;
            for (unsigned x = 0; x < 8; x++) {
                unsigned idx = y * 8 + x;
                uint16_t in = 100 + idx;
                uint32_t r = ((uint32_t)in * 1152) >> 10;
                expected_buf[idx] = (r > 1023) ? 1023 : (uint16_t)r;

                i_data = in;
                wait(pclk.posedge_event());

                if (o_href && idx > 0) {
                    uint16_t got = o_data;
                    uint16_t exp = expected_buf[idx - 1];
                    if (got == exp) ok++;
                    else {
                        if (err < 5) cerr << "  ERR x=" << x << " y=" << y
                                           << " in=" << in << " exp=" << exp << " got=" << got << endl;
                        err++;
                    }
                }
            }
            i_href = 0;
            wait(pclk.posedge_event());
        }
        cout << "  Result: " << ok << " OK, " << err << " errors" << endl;
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
