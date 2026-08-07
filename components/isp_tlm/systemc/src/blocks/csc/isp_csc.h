/*
 * CSC (Color Space Conversion) Block
 * Converts RGB to YUV color space
 * Pipeline latency: 9 cycles (RTL DLY_CLK = 9)
 * Matches RTL: isp_csc.v
 *
 * Inputs are normalized from BITS-wide RGB to 8-bit full range.
 * Conversion-standard encoding:
 *   1: BT.709 full range
 *   2: BT.601 full range
 *   0/3: reserved (zero output)
 */

#ifndef ISP_CSC_H
#define ISP_CSC_H

#include <systemc>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "blocks/csc/csc_metrics.h"

template<unsigned int BITS = 10>
class isp_csc : public sc_module {
public:
    SC_HAS_PROCESS(isp_csc);

    static_assert(BITS > 0 && BITS <= 16,
                  "CSC supports RGB sample widths from 1 through 16 bits");

    static constexpr unsigned DLY_CLK = 9;
    static constexpr uint8_t CSC_BT709 = 1;
    static constexpr uint8_t CSC_BT601 = 2;

    sc_in<bool> pclk{"pclk"};
    sc_in<bool> rst_n{"rst_n"};
    sc_in<bool> enable{"enable"};

    sc_in<bool> i_href{"i_href"};
    sc_in<bool> i_vsync{"i_vsync"};

    sc_in<uint16_t> i_r{"i_r"};
    sc_in<uint16_t> i_g{"i_g"};
    sc_in<uint16_t> i_b{"i_b"};

    sc_out<bool> o_href{"o_href"};
    sc_out<bool> o_vsync{"o_vsync"};

    sc_out<uint8_t> o_y{"o_y"};
    sc_out<uint8_t> o_u{"o_u"};
    sc_out<uint8_t> o_v{"o_v"};

    sc_in<uint8_t> i_conv_standard{"i_conv_standard"};

    isp_csc(const sc_module_name& name)
        : sc_module(name)
        , m_pixel_count(0)
        , m_line_count(0)
        , m_frame_count(0)
        , m_prev_vsync(false)
        , m_prev_href(false)
        , m_in_frame(false)
    {
        SC_THREAD(process_thread);
        sensitive << pclk.pos();
        dont_initialize();

        SC_THREAD(reset_handler);
        sensitive << rst_n.neg();

        for (unsigned i = 0; i < DLY_CLK; i++) {
            m_href_delay[i] = false;
            m_vsync_delay[i] = false;
            m_r_delay[i] = m_g_delay[i] = m_b_delay[i] = 0;
        }
    }

    CscMetricsCollector& get_metrics() { return m_metrics; }
    const CscMetricsCollector& get_metrics() const { return m_metrics; }

    void set_image_size(unsigned w, unsigned h) { m_metrics.set_config(w, h); }

    unsigned get_pixel_count() const { return m_pixel_count >= 0 ? (unsigned)m_pixel_count : 0; }
    unsigned get_frame_count() const { return m_frame_count >= 0 ? (unsigned)m_frame_count : 0; }

private:
    static constexpr uint32_t MAX_SAMPLE =
        (uint32_t{1} << BITS) - uint32_t{1};

    int m_pixel_count;
    int m_line_count;
    int m_frame_count;
    bool m_prev_vsync;
    bool m_prev_href;
    bool m_in_frame;

    bool m_href_delay[DLY_CLK];
    bool m_vsync_delay[DLY_CLK];
    uint16_t m_r_delay[DLY_CLK];
    uint16_t m_g_delay[DLY_CLK];
    uint16_t m_b_delay[DLY_CLK];

    CscMetricsCollector m_metrics;

    void process_thread() {
        o_href.write(false);
        o_vsync.write(false);
        o_y.write(0);
        o_u.write(0);
        o_v.write(0);

        while (true) {
            wait();

            if (!rst_n.read()) {
                m_pixel_count = 0;
                m_line_count = -1;
                m_in_frame = false;
                for (unsigned i = 0; i < DLY_CLK; i++) {
                    m_href_delay[i] = false;
                    m_vsync_delay[i] = false;
                    m_r_delay[i] = m_g_delay[i] = m_b_delay[i] = 0;
                }
                continue;
            }

            bool curr_href = i_href.read();
            bool curr_vsync = i_vsync.read();
            uint16_t curr_r = i_r.read();
            uint16_t curr_g = i_g.read();
            uint16_t curr_b = i_b.read();

            bool vsync_fall = m_prev_vsync && !curr_vsync;
            bool vsync_rise = !m_prev_vsync && curr_vsync;
            bool href_rise = !m_prev_href && curr_href;

            if (vsync_fall) {
                m_frame_count++;
                m_line_count = -1;
                m_pixel_count = 0;
                m_in_frame = true;
                m_metrics.record_frame();
            }
            if (vsync_rise) {
                m_in_frame = false;
            }
            if (href_rise && m_in_frame) {
                m_line_count++;
                m_pixel_count = 0;
                m_metrics.record_line();
            }

            m_metrics.record_total_cycle();

            for (unsigned i = DLY_CLK - 1; i > 0; i--) {
                m_href_delay[i] = m_href_delay[i - 1];
                m_vsync_delay[i] = m_vsync_delay[i - 1];
                m_r_delay[i] = m_r_delay[i - 1];
                m_g_delay[i] = m_g_delay[i - 1];
                m_b_delay[i] = m_b_delay[i - 1];
            }
            m_href_delay[0] = curr_href && m_in_frame;
            m_vsync_delay[0] = curr_vsync;
            m_r_delay[0] = curr_r;
            m_g_delay[0] = curr_g;
            m_b_delay[0] = curr_b;

            uint8_t out_y = 0, out_u = 0, out_v = 0;
            bool out_href = m_href_delay[DLY_CLK - 1];
            bool out_vsync = m_vsync_delay[DLY_CLK - 1];

            // The delayed href is authoritative here.  Do not gate with the
            // current frame state: delayed pixels must drain after input vsync
            // has already ended the frame.
            if (enable.read() && out_href) {
                uint16_t r_in = m_r_delay[DLY_CLK - 1];
                uint16_t g_in = m_g_delay[DLY_CLK - 1];
                uint16_t b_in = m_b_delay[DLY_CLK - 1];

                uint8_t standard = i_conv_standard.read();
                int32_t y_val = 0, u_val = 0, v_val = 0;
                bool converted = false;

                if (standard == CSC_BT601) {
                    convert_bt601(r_in, g_in, b_in, y_val, u_val, v_val);
                    m_metrics.record_bt601();
                    converted = true;
                } else if (standard == CSC_BT709) {
                    convert_bt709(r_in, g_in, b_in, y_val, u_val, v_val);
                    m_metrics.record_bt709();
                    converted = true;
                }

                // Encodings 0 and 3 are reserved and deliberately retain the
                // zero defaults above.
                if (converted) {
                    out_y = static_cast<uint8_t>(y_val);
                    out_u = static_cast<uint8_t>(u_val);
                    out_v = static_cast<uint8_t>(v_val);

                    m_metrics.record_rgb_to_yuv();
                    m_metrics.record_y_pixel();
                    m_metrics.record_u_pixel();
                    m_metrics.record_v_pixel();
                    m_metrics.record_pixel();
                    m_metrics.record_active_cycle();
                    m_metrics.record_mul();
                    m_metrics.record_mul();
                    m_metrics.record_mul();
                }
            }

            o_y.write(out_y);
            o_u.write(out_u);
            o_v.write(out_v);
            o_href.write(out_href);
            o_vsync.write(out_vsync);

            if (curr_href && m_in_frame) {
                m_pixel_count++;
            }

            m_prev_vsync = curr_vsync;
            m_prev_href = curr_href;
        }
    }

    void reset_handler() {
        wait();
        m_pixel_count = 0;
        m_line_count = -1;
        m_frame_count = 0;
        m_in_frame = false;
        m_prev_vsync = false;
        m_prev_href = false;
        for (unsigned i = 0; i < DLY_CLK; i++) {
            m_href_delay[i] = false;
            m_vsync_delay[i] = false;
            m_r_delay[i] = m_g_delay[i] = m_b_delay[i] = 0;
        }
        m_metrics.reset();
    }

    static uint8_t normalize_to_8bit(uint16_t value) {
        const uint32_t bounded =
            value > MAX_SAMPLE ? MAX_SAMPLE : static_cast<uint32_t>(value);
        return static_cast<uint8_t>(
            (bounded * 255U + MAX_SAMPLE / 2U) / MAX_SAMPLE);
    }

    // Round signed fixed-point values symmetrically to nearest, with exact
    // halves rounded away from zero.
    static int32_t round_div_256(int32_t value) {
        return value >= 0 ? (value + 128) / 256
                          : -((-value + 128) / 256);
    }

    static void convert_bt601(uint16_t r, uint16_t g, uint16_t b,
                              int32_t& y, int32_t& u, int32_t& v) {
        const int32_t r8 = normalize_to_8bit(r);
        const int32_t g8 = normalize_to_8bit(g);
        const int32_t b8 = normalize_to_8bit(b);

        y = clamp_8bit(round_div_256(77 * r8 + 150 * g8 + 29 * b8));
        u = clamp_8bit(round_div_256(-43 * r8 - 85 * g8 + 128 * b8) +
                       128);
        v = clamp_8bit(round_div_256(128 * r8 - 107 * g8 - 21 * b8) +
                       128);
    }

    static void convert_bt709(uint16_t r, uint16_t g, uint16_t b,
                              int32_t& y, int32_t& u, int32_t& v) {
        const int32_t r8 = normalize_to_8bit(r);
        const int32_t g8 = normalize_to_8bit(g);
        const int32_t b8 = normalize_to_8bit(b);

        y = clamp_8bit(round_div_256(54 * r8 + 183 * g8 + 18 * b8));
        u = clamp_8bit(round_div_256(-29 * r8 - 99 * g8 + 128 * b8) +
                       128);
        v = clamp_8bit(round_div_256(128 * r8 - 116 * g8 - 12 * b8) +
                       128);
    }

    static int32_t clamp_8bit(int32_t val) {
        if (val < 0) return 0;
        if (val > 255) return 255;
        return val;
    }
};

using isp_csc_10b = isp_csc<10>;
using isp_csc_8b = isp_csc<8>;

#endif // ISP_CSC_H
