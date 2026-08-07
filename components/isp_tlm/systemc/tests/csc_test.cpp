#include <systemc>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "blocks/csc/isp_csc.h"

namespace {

struct Expected {
    std::uint8_t y;
    std::uint8_t u;
    std::uint8_t v;
};

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

class CscTester : public sc_core::sc_module {
public:
    sc_core::sc_clock clock{"clock", sc_core::sc_time(10, sc_core::SC_NS)};
    sc_core::sc_signal<bool> reset_n{"reset_n"};
    sc_core::sc_signal<bool> enable{"enable"};
    sc_core::sc_signal<bool> href{"href"};
    sc_core::sc_signal<bool> vsync{"vsync"};
    sc_core::sc_signal<std::uint16_t> r{"r"};
    sc_core::sc_signal<std::uint16_t> g{"g"};
    sc_core::sc_signal<std::uint16_t> b{"b"};
    sc_core::sc_signal<std::uint8_t> standard{"standard"};
    sc_core::sc_signal<bool> out_href{"out_href"};
    sc_core::sc_signal<bool> out_vsync{"out_vsync"};
    sc_core::sc_signal<std::uint8_t> y{"y"};
    sc_core::sc_signal<std::uint8_t> u{"u"};
    sc_core::sc_signal<std::uint8_t> v{"v"};

    isp_csc<12> dut{"dut"};

    SC_HAS_PROCESS(CscTester);

    explicit CscTester(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
    {
        dut.pclk(clock);
        dut.rst_n(reset_n);
        dut.enable(enable);
        dut.i_href(href);
        dut.i_vsync(vsync);
        dut.i_r(r);
        dut.i_g(g);
        dut.i_b(b);
        dut.i_conv_standard(standard);
        dut.o_href(out_href);
        dut.o_vsync(out_vsync);
        dut.o_y(y);
        dut.o_u(u);
        dut.o_v(v);

        SC_THREAD(run);
    }

private:
    std::vector<Expected> observed_;

    void cycle()
    {
        wait(clock.posedge_event());
        wait(sc_core::SC_ZERO_TIME);
        if (out_href.read()) {
            observed_.push_back({y.read(), u.read(), v.read()});
        }
    }

    void idle(unsigned cycles)
    {
        href.write(false);
        r.write(0);
        g.write(0);
        b.write(0);
        for (unsigned index = 0; index < cycles; ++index) {
            cycle();
        }
    }

    void begin_frame(std::uint8_t conversion_standard)
    {
        standard.write(conversion_standard);
        href.write(false);
        vsync.write(true);
        cycle();
        vsync.write(false);
        cycle();
    }

    void sample(std::uint16_t red,
                std::uint16_t green,
                std::uint16_t blue)
    {
        href.write(true);
        r.write(red);
        g.write(green);
        b.write(blue);
        cycle();
    }

    void end_frame_and_drain()
    {
        href.write(false);
        vsync.write(true);
        cycle();
        idle(isp_csc<12>::DLY_CLK + 2u);
    }

    void check_frame(const std::vector<Expected>& expected,
                     const std::string& label)
    {
        expect(observed_.size() == expected.size(),
               label + ": all delayed pixels drain after input VSYNC");
        const std::size_t count =
            observed_.size() < expected.size() ? observed_.size()
                                               : expected.size();
        for (std::size_t index = 0; index < count; ++index) {
            expect(observed_[index].y == expected[index].y &&
                       observed_[index].u == expected[index].u &&
                       observed_[index].v == expected[index].v,
                   label + ": sample " + std::to_string(index));
        }
        observed_.clear();
    }

    void run()
    {
        enable.write(true);
        href.write(false);
        vsync.write(false);
        r.write(0);
        g.write(0);
        b.write(0);
        standard.write(isp_csc<12>::CSC_BT601);

        reset_n.write(false);
        cycle();
        reset_n.write(true);
        idle(1);

        begin_frame(isp_csc<12>::CSC_BT601);
        sample(0, 0, 0);
        sample(4095, 4095, 4095);
        sample(4095, 0, 0);
        sample(0, 4095, 0);
        sample(0, 0, 4095);
        end_frame_and_drain();
        check_frame({
                        {0, 128, 128},
                        {255, 128, 128},
                        {77, 85, 255},
                        {149, 43, 21},
                        {29, 255, 107},
                    },
                    "BT.601 full-range");

        begin_frame(isp_csc<12>::CSC_BT709);
        sample(4095, 0, 0);
        sample(0, 4095, 0);
        sample(0, 0, 4095);
        end_frame_and_drain();
        check_frame({
                        {54, 99, 255},
                        {182, 29, 12},
                        {18, 255, 116},
                    },
                    "BT.709 full-range");

        begin_frame(0);
        sample(4095, 2048, 1);
        end_frame_and_drain();
        check_frame({{0, 0, 0}}, "reserved standard 0");

        begin_frame(3);
        sample(4095, 2048, 1);
        end_frame_and_drain();
        check_frame({{0, 0, 0}}, "reserved standard 3");

        sc_core::sc_stop();
    }
};

}  // namespace

int sc_main(int, char**)
{
    CscTester tester{"tester"};
    sc_core::sc_start();
    if (failures == 0) {
        std::cout << "PASS: ISP CSC standards, normalization, and drain tests\n";
    }
    return failures == 0 ? 0 : 1;
}
