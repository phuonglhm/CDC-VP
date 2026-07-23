/*
 * BLC (Black Level Correction) Block
 * Back to SC_THREAD with proper timing
 */

#ifndef ISP_BLC_H
#define ISP_BLC_H

#include <systemc>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "blocks/blc/blc_metrics.h"

template<unsigned int BITS>
class isp_blc : public sc_module {
public:
    sc_in<bool> pclk{"pclk"};
    sc_in<bool> rst_n{"rst_n"};
    sc_in<bool> enable{"enable"};

    sc_in<bool> i_href{"i_href"};
    sc_in<bool> i_vsync{"i_vsync"};
    sc_in<uint16_t> i_data{"i_data"};

    sc_out<bool> o_href{"o_href"};
    sc_out<bool> o_vsync{"o_vsync"};
    sc_out<uint16_t> o_data{"o_data"};

    sc_in<uint16_t> i_blc_r{"i_blc_r"}, i_blc_gr{"i_blc_gr"};
    sc_in<uint16_t> i_blc_gb{"i_blc_gb"}, i_blc_b{"i_blc_b"};
    sc_in<bool> i_linear_en{"i_linear_en"};
    sc_in<uint16_t> i_linear_r{"i_linear_r"}, i_linear_gr{"i_linear_gr"};
    sc_in<uint16_t> i_linear_gb{"i_linear_gb"}, i_linear_b{"i_linear_b"};

    isp_blc(const sc_module_name& name)
        : sc_module(name)
        , m_pixel_count(0), m_line_count(0), m_frame_count(0)
        , m_prev_vsync(false), m_prev_href(false), m_in_frame(false)
    {
        SC_THREAD(process_thread);
        sensitive << pclk.pos();
        dont_initialize();

        SC_THREAD(reset_handler);
        sensitive << rst_n.neg();
    }

    BlcMetricsCollector& get_metrics() { return m_metrics; }
    const BlcMetricsCollector& get_metrics() const { return m_metrics; }

    void set_image_size(unsigned w, unsigned h) { m_metrics.set_config(w, h); }

    unsigned get_pixel_count() const { return m_pixel_count; }
    unsigned get_frame_count() const { return m_frame_count; }
    unsigned get_width() const { return m_metrics.get_width(); }
    unsigned get_height() const { return m_metrics.get_height(); }
    bool in_frame() const { return m_in_frame; }

private:
    static constexpr uint16_t MAX = (1 << BITS) - 1;

    unsigned m_pixel_count, m_line_count, m_frame_count;
    bool m_prev_vsync, m_prev_href, m_in_frame;
    BlcMetricsCollector m_metrics;

    void process_thread() {
        o_href.write(false);
        o_vsync.write(false);
        o_data.write(0);

        while (true) {
            wait();

            if (!rst_n.read()) {
                m_pixel_count = 0;
                m_line_count = 0;
                m_in_frame = false;
                continue;
            }

            bool curr_href = i_href.read();
            bool curr_vsync = i_vsync.read();
            uint16_t curr_data = i_data.read();

            bool vsync_fall = m_prev_vsync && !curr_vsync;
            bool vsync_rise = !m_prev_vsync && curr_vsync;
            bool href_rise = !m_prev_href && curr_href;

            if (vsync_fall) {
                m_frame_count++;
                m_line_count = 0;
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

            uint16_t out_data = 0;
            bool out_href = false;
            bool out_vsync = curr_vsync;

            if (enable.read() && m_in_frame && curr_href) {
                unsigned ch = calculate_channel(m_pixel_count, m_line_count);
                uint16_t off = get_blc_offset(ch);

                uint16_t corrected;
                if (i_linear_en.read()) {
                    uint16_t param = get_linear_param(ch);
                    uint32_t result = ((uint32_t)curr_data * param) >> BITS;
                    corrected = result > MAX ? MAX : (uint16_t)result;
                    m_metrics.record_linear_correction();
                    m_metrics.record_mul();
                } else {
                    corrected = (curr_data > off) ? (curr_data - off) : 0;
                    m_metrics.record_nonlinear_correction();
                    m_metrics.record_sub();
                }

                if (corrected > MAX) corrected = MAX;

                m_metrics.record_pixel();
                m_metrics.record_channel_correction(ch);
                m_metrics.record_pixel_value(corrected);
                m_metrics.record_active_cycle();
                if (corrected >= MAX) m_metrics.record_saturation_clip();

                m_pixel_count++;
                out_data = corrected;
                out_href = true;
            }

            o_data.write(out_data);
            o_href.write(out_href);
            o_vsync.write(out_vsync);

            m_prev_vsync = curr_vsync;
            m_prev_href = curr_href;
        }
    }

    void reset_handler() {
        wait();
        m_pixel_count = 0;
        m_line_count = 0;
        m_frame_count = 0;
        m_in_frame = false;
        m_prev_vsync = false;
        m_prev_href = false;
        m_metrics.reset();
    }

    unsigned calculate_channel(unsigned x, unsigned y) {
        bool even_y = (y & 1) == 0;
        bool even_x = (x & 1) == 0;
        if (even_y) return even_x ? 0 : 1;
        return even_x ? 2 : 3;
    }

    uint16_t get_blc_offset(unsigned channel) {
        switch (channel) {
            case 0: return i_blc_r.read();
            case 1: return i_blc_gr.read();
            case 2: return i_blc_gb.read();
            default: return i_blc_b.read();
        }
    }

    uint16_t get_linear_param(unsigned channel) {
        switch (channel) {
            case 0: return i_linear_r.read();
            case 1: return i_linear_gr.read();
            case 2: return i_linear_gb.read();
            default: return i_linear_b.read();
        }
    }
};

using isp_blc_10b = isp_blc<10>;
using isp_blc_8b = isp_blc<8>;

#endif // ISP_BLC_H
