#include <systemc>

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "vip/vip.h"

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

std::uint16_t to_10bit(std::uint8_t value) {
    return static_cast<std::uint16_t>((static_cast<unsigned>(value) * 1023U +
                                      127U) /
                                     255U);
}

void put_rgb(std::vector<std::uint16_t>& rgb,
             std::size_t stride,
             unsigned x,
             unsigned y,
             std::uint8_t r,
             std::uint8_t g,
             std::uint8_t b) {
    const std::size_t index = static_cast<std::size_t>(y) * stride + 3U * x;
    rgb[index] = to_10bit(r);
    rgb[index + 1U] = to_10bit(g);
    rgb[index + 2U] = to_10bit(b);
}

void put_yuv(std::vector<std::uint8_t>& yuv,
             std::size_t stride,
             unsigned x,
             unsigned y,
             std::uint8_t y_value,
             std::uint8_t u_value,
             std::uint8_t v_value) {
    const std::size_t index = static_cast<std::size_t>(y) * stride + 3U * x;
    yuv[index] = y_value;
    yuv[index + 1U] = u_value;
    yuv[index + 2U] = v_value;
}

void test_buffer_api() {
    constexpr unsigned width = 4;
    constexpr unsigned height = 2;
    constexpr std::size_t rgb_stride = width * 3U + 2U;
    std::vector<std::uint16_t> rgb(rgb_stride * height, 0);

    put_rgb(rgb, rgb_stride, 0, 0, 255, 0, 0);       // red
    put_rgb(rgb, rgb_stride, 1, 0, 0, 255, 0);       // green
    put_rgb(rgb, rgb_stride, 2, 0, 0, 0, 255);       // blue
    put_rgb(rgb, rgb_stride, 3, 0, 255, 255, 255);   // white
    put_rgb(rgb, rgb_stride, 0, 1, 255, 255, 0);     // yellow
    put_rgb(rgb, rgb_stride, 1, 1, 0, 255, 255);     // cyan
    put_rgb(rgb, rgb_stride, 2, 1, 255, 0, 255);     // magenta
    put_rgb(rgb, rgb_stride, 3, 1, 0, 0, 0);         // black

    std::vector<std::uint8_t> packed(width * height * 3U / 2U, 0xEE);
    std::string error;
    const bool ok = VIP<10>::convert_i420_contiguous(
        rgb.data(), rgb.size(), rgb_stride, width, height, packed.data(),
        packed.size(), VIP<10>::CSC_BT601, &error);
    expect(ok, "valid contiguous I420 conversion: " + error);

    const std::vector<std::uint8_t> expected{
        77, 149, 29, 255, 226, 178, 106, 0,  // Y
        85, 255,                              // U from row 0, x=0 and x=2
        255, 107                              // V from row 0, x=0 and x=2
    };
    expect(packed == expected,
           "packed order and BT.601 upper-left chroma samples");

    constexpr std::size_t y_stride = 6;
    constexpr std::size_t uv_stride = 3;
    std::vector<std::uint8_t> y(y_stride * height, 0xEE);
    std::vector<std::uint8_t> u(uv_stride, 0xEE);
    std::vector<std::uint8_t> v(uv_stride, 0xEE);
    expect(VIP<10>::convert_i420(
               rgb.data(), rgb.size(), rgb_stride, width, height,
               y.data(), y.size(), y_stride,
               u.data(), u.size(), uv_stride,
               v.data(), v.size(), uv_stride),
           "strided I420 conversion uses default BT.601 standard 2");
    expect(y[4] == 0xEE && y[5] == 0xEE && y[10] == 0xEE && y[11] == 0xEE,
           "Y row padding is not modified");
    expect(u[2] == 0xEE && v[2] == 0xEE,
           "chroma row padding is not modified");
    expect(u[0] == 85 && u[1] == 255 && v[0] == 255 && v[1] == 107,
           "strided U/V data uses upper-left decimation");

    VIP<10>::YuvSample bt709{};
    expect(VIP<10>::convert_normalized(255, 0, 0,
                                        VIP<10>::CSC_BT709, bt709),
           "BT.709 standard 1 is supported");
    expect(bt709.y == 54 && bt709.cb == 99 && bt709.cr == 255,
           "BT.709 red conversion is full-range and rounded");

    VIP<10>::YuvSample rejected{1, 2, 3};
    expect(!VIP<10>::convert_normalized(255, 0, 0, 0, rejected) &&
               rejected.y == 0 && rejected.cb == 0 && rejected.cr == 0,
           "CSC standard 0 is rejected");
    expect(!VIP<10>::convert_normalized(255, 0, 0, 3, rejected),
           "CSC standard 3 is rejected");

    expect(!VIP<10>::convert_i420_contiguous(
               rgb.data(), rgb.size(), rgb_stride, 3, height, packed.data(),
               packed.size(), VIP<10>::CSC_BT601, &error) &&
               error.find("even") != std::string::npos,
           "odd I420 dimensions are rejected");
    expect(!VIP<10>::convert_i420_contiguous(
               rgb.data(), rgb.size(), rgb_stride, width, height, packed.data(),
               packed.size() - 1U, VIP<10>::CSC_BT601, &error),
           "undersized packed output is rejected");

    auto bad_rgb = rgb;
    bad_rgb[0] = 1024;
    std::fill(packed.begin(), packed.end(), 0xA5);
    expect(!VIP<10>::convert_i420_contiguous(
               bad_rgb.data(), bad_rgb.size(), rgb_stride, width, height,
               packed.data(), packed.size(), VIP<10>::CSC_BT601, &error),
           "input values wider than BITS are rejected");
    expect(std::all_of(packed.begin(), packed.end(),
                       [](std::uint8_t value) { return value == 0xA5; }),
           "validation failure does not partially modify output");
}

void test_yuv_pack_api() {
    constexpr unsigned width = 4;
    constexpr unsigned height = 2;
    constexpr std::size_t input_stride = width * 3U + 2U;
    constexpr std::size_t y_stride = width + 2U;
    constexpr std::size_t uv_stride = width / 2U + 1U;

    std::vector<std::uint8_t> yuv(input_stride * height, 0xEE);
    put_yuv(yuv, input_stride, 0, 0, 10, 20, 30);
    put_yuv(yuv, input_stride, 1, 0, 11, 21, 31);
    put_yuv(yuv, input_stride, 2, 0, 12, 22, 32);
    put_yuv(yuv, input_stride, 3, 0, 13, 23, 33);
    put_yuv(yuv, input_stride, 0, 1, 14, 24, 34);
    put_yuv(yuv, input_stride, 1, 1, 15, 25, 35);
    put_yuv(yuv, input_stride, 2, 1, 16, 26, 36);
    put_yuv(yuv, input_stride, 3, 1, 17, 27, 37);

    std::vector<std::uint8_t> y(y_stride * height, 0xEE);
    std::vector<std::uint8_t> u(uv_stride, 0xEE);
    std::vector<std::uint8_t> v(uv_stride, 0xEE);
    std::string error;
    expect(VIP<10>::pack_yuv444_i420(
               yuv.data(), yuv.size(), input_stride, width, height,
               y.data(), y.size(), y_stride,
               u.data(), u.size(), uv_stride,
               v.data(), v.size(), uv_stride, &error),
           "valid strided YUV444-to-I420 packing: " + error);

    const std::vector<std::uint8_t> expected_y{
        10, 11, 12, 13, 0xEE, 0xEE,
        14, 15, 16, 17, 0xEE, 0xEE
    };
    expect(y == expected_y, "YUV444 pack copies Y and preserves row padding");
    expect(u[0] == 20 && u[1] == 22 && u[2] == 0xEE,
           "YUV444 pack uses upper-left U samples and preserves padding");
    expect(v[0] == 30 && v[1] == 32 && v[2] == 0xEE,
           "YUV444 pack uses upper-left V samples and preserves padding");

    std::fill(y.begin(), y.end(), 0xA5);
    std::fill(u.begin(), u.end(), 0xA5);
    std::fill(v.begin(), v.end(), 0xA5);
    expect(!VIP<10>::pack_yuv444_i420(
               yuv.data(), input_stride + width * 3U - 1U, input_stride,
               width, height,
               y.data(), y.size(), y_stride,
               u.data(), u.size(), uv_stride,
               v.data(), v.size(), uv_stride, &error),
           "undersized YUV444 input capacity is rejected");
    expect(std::all_of(y.begin(), y.end(),
                       [](std::uint8_t value) { return value == 0xA5; }) &&
               std::all_of(u.begin(), u.end(),
                           [](std::uint8_t value) { return value == 0xA5; }) &&
               std::all_of(v.begin(), v.end(),
                           [](std::uint8_t value) { return value == 0xA5; }),
           "YUV444 validation failure does not partially modify output");
}

class VipPinTester : public sc_core::sc_module {
public:
    sc_core::sc_in<bool> pclk{"pclk"};
    sc_core::sc_out<bool> rst_n{"rst_n"};
    sc_core::sc_out<bool> href{"href"};
    sc_core::sc_out<bool> vsync{"vsync"};
    sc_core::sc_out<sc_dt::sc_uint<10>> r{"r"};
    sc_core::sc_out<sc_dt::sc_uint<10>> g{"g"};
    sc_core::sc_out<sc_dt::sc_uint<10>> b{"b"};
    sc_core::sc_out<sc_dt::sc_uint<2>> standard{"standard"};
    sc_core::sc_out<bool> input_is_yuv{"input_is_yuv"};
    sc_core::sc_out<bool> enable{"enable"};
    sc_core::sc_in<bool> out_href{"out_href"};
    sc_core::sc_in<bool> out_vsync{"out_vsync"};
    sc_core::sc_in<std::uint8_t> out_y{"out_y"};
    sc_core::sc_in<std::uint8_t> out_u{"out_u"};
    sc_core::sc_in<std::uint8_t> out_v{"out_v"};
    sc_core::sc_in<bool> out_error{"out_error"};

    SC_HAS_PROCESS(VipPinTester);
    explicit VipPinTester(const sc_core::sc_module_name& name)
        : sc_core::sc_module(name) {
        SC_THREAD(run);
    }

private:
    void drive(bool active,
               bool frame_blank,
               std::uint16_t rv,
               std::uint16_t gv,
               std::uint16_t bv,
               std::uint8_t std_value,
               bool yuv_mode = false,
               bool enabled = true) {
        href.write(active);
        vsync.write(frame_blank);
        r.write(rv);
        g.write(gv);
        b.write(bv);
        standard.write(std_value);
        input_is_yuv.write(yuv_mode);
        enable.write(enabled);
        wait(pclk.posedge_event());
        wait(sc_core::SC_ZERO_TIME);
    }

    void expect_pin(std::uint8_t y,
                    std::uint8_t u,
                    std::uint8_t v,
                    bool error,
                    const char* label) {
        expect(out_y.read() == y && out_u.read() == u && out_v.read() == v &&
                   out_error.read() == error,
               label);
    }

    void run() {
        rst_n.write(false);
        href.write(false);
        vsync.write(true);
        r.write(0);
        g.write(0);
        b.write(0);
        standard.write(VIP<10>::CSC_BT601);
        input_is_yuv.write(false);
        enable.write(true);
        wait(pclk.posedge_event());
        wait(sc_core::SC_ZERO_TIME);
        expect(!out_href.read() && !out_vsync.read() && !out_error.read(),
               "pin reset clears outputs");

        rst_n.write(true);
        drive(false, true, 0, 0, 0, VIP<10>::CSC_BT601);
        expect(out_vsync.read(), "pin VSYNC is forwarded with one-cycle sampling");

        drive(true, false, to_10bit(255), 0, 0, VIP<10>::CSC_BT601);
        expect_pin(77, 85, 255, false,
                   "even/even pin sample carries Y, U, and V");
        drive(true, false, 0, to_10bit(255), 0, VIP<10>::CSC_BT601);
        expect_pin(149, 0, 0, false,
                   "odd-x pin sample carries Y and zero chroma");
        drive(false, false, 0, 0, 0, VIP<10>::CSC_BT601);
        drive(true, false, 0, 0, to_10bit(255), VIP<10>::CSC_BT601);
        expect_pin(29, 0, 0, false,
                   "odd-y pin sample carries Y and zero chroma");

        drive(false, false, 0, 0, 0, VIP<10>::CSC_BT601);
        drive(true, false, to_10bit(255), 0, 0, 0);
        expect_pin(0, 0, 0, true,
                   "reserved standard is rejected in RGB pin mode");

        drive(false, true, 0, 0, 0, VIP<10>::CSC_BT709);
        drive(true, false, to_10bit(255), 0, 0, VIP<10>::CSC_BT709);
        expect_pin(54, 99, 255, false,
                   "BT.709 pin sample is full-range at even/even origin");

        drive(false, true, 0, 0, 0, 0, true);
        drive(true, false, 0x311, 0x222, 0x333, 0, true);
        expect_pin(0x11, 0x22, 0x33, false,
                   "YUV pin mode uses low 8 bits and skips reserved CSC");
        drive(true, false, 0x344, 0x255, 0x366, 0, true);
        expect_pin(0x44, 0, 0, false,
                   "YUV pin mode emits only Y at odd x");
        drive(false, false, 0, 0, 0, 0, true);
        drive(true, false, 0x377, 0x288, 0x399, 0, true);
        expect_pin(0x77, 0, 0, false,
                   "YUV pin mode emits only Y on an odd row");

        drive(false, true, 0, 0, 0, VIP<10>::CSC_BT601);
        drive(true, false, to_10bit(255), 0, 0,
              VIP<10>::CSC_BT601, false, false);
        expect(!out_href.read(), "disabled VIP suppresses active href");
        expect_pin(0, 0, 0, false, "disabled VIP emits no active sample");

        sc_core::sc_stop();
    }
};

}  // namespace

int sc_main(int, char**) {
    test_buffer_api();
    test_yuv_pack_api();

    sc_core::sc_clock pclk("pclk", sc_core::sc_time(10, sc_core::SC_NS));
    sc_core::sc_signal<bool> rst_n;
    sc_core::sc_signal<bool> href;
    sc_core::sc_signal<bool> vsync;
    sc_core::sc_signal<sc_dt::sc_uint<10>> r;
    sc_core::sc_signal<sc_dt::sc_uint<10>> g;
    sc_core::sc_signal<sc_dt::sc_uint<10>> b;
    sc_core::sc_signal<sc_dt::sc_uint<2>> standard;
    sc_core::sc_signal<bool> input_is_yuv;
    sc_core::sc_signal<bool> enable;
    sc_core::sc_signal<bool> out_href;
    sc_core::sc_signal<bool> out_vsync;
    sc_core::sc_signal<std::uint8_t> out_y;
    sc_core::sc_signal<std::uint8_t> out_u;
    sc_core::sc_signal<std::uint8_t> out_v;
    sc_core::sc_signal<bool> out_error;

    VIP<10> dut("vip");
    dut.pclk(pclk);
    dut.rst_n(rst_n);
    dut.i_href(href);
    dut.i_vsync(vsync);
    dut.i_r(r);
    dut.i_g(g);
    dut.i_b(b);
    dut.i_csc_standard(standard);
    dut.i_input_is_yuv(input_is_yuv);
    dut.i_enable(enable);
    dut.o_href(out_href);
    dut.o_vsync(out_vsync);
    dut.o_y(out_y);
    dut.o_u(out_u);
    dut.o_v(out_v);
    dut.o_error(out_error);

    VipPinTester tester("tester");
    tester.pclk(pclk);
    tester.rst_n(rst_n);
    tester.href(href);
    tester.vsync(vsync);
    tester.r(r);
    tester.g(g);
    tester.b(b);
    tester.standard(standard);
    tester.input_is_yuv(input_is_yuv);
    tester.enable(enable);
    tester.out_href(out_href);
    tester.out_vsync(out_vsync);
    tester.out_y(out_y);
    tester.out_u(out_u);
    tester.out_v(out_v);
    tester.out_error(out_error);

    sc_core::sc_start();
    if (failures == 0) {
        std::cout << "PASS: VIP RGB/YUV444 and I420 tests\n";
    }
    return failures == 0 ? 0 : 1;
}
