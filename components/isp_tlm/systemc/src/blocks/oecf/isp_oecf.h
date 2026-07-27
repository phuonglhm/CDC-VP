/*
 * OECF (Opto-Electronic Conversion Function) Block
 * Applies tone mapping using per-channel LUTs
 * Pipeline latency: 1 cycle (RAM access)
 * Matches RTL: isp_oecf.v
 */

#ifndef ISP_OECF_H
#define ISP_OECF_H

#include <systemc>
#include <array>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "blocks/oecf/oecf_metrics.h"

template<unsigned int BITS = 10, BayerPattern BAYER = BayerPattern::RGGB>
class isp_oecf : public sc_module {
public:
    static constexpr unsigned DLY_CLK = 1;
    static constexpr unsigned LUT_SIZE = 1 << BITS;

    sc_in<bool> pclk{"pclk"};
    sc_in<bool> rst_n{"rst_n"};
    sc_in<bool> enable{"enable"};

    sc_in<bool> i_href{"i_href"};
    sc_in<bool> i_vsync{"i_vsync"};
    sc_in<uint16_t> i_raw{"i_raw"};

    sc_out<bool> o_href{"o_href"};
    sc_out<bool> o_vsync{"o_vsync"};
    sc_out<uint16_t> o_raw{"o_raw"};

    isp_oecf(const sc_module_name& name)
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

        init_identity_lut();

        m_href_delay = false;
        m_vsync_delay = false;
        m_data_delay = 0;
    }

    OecfMetricsCollector& get_metrics() { return m_metrics; }

    void set_image_size(unsigned w, unsigned h) { m_metrics.set_config(w, h); }

private:
    static constexpr uint16_t MAX_VAL = (1 << BITS) - 1;

    int m_pixel_count;
    int m_line_count;
    int m_frame_count;
    bool m_prev_vsync;
    bool m_prev_href;
    bool m_in_frame;

    bool m_href_delay;
    bool m_vsync_delay;
    uint16_t m_data_delay;

    std::array<uint16_t, LUT_SIZE> m_lut_r;
    std::array<uint16_t, LUT_SIZE> m_lut_gr;
    std::array<uint16_t, LUT_SIZE> m_lut_gb;
    std::array<uint16_t, LUT_SIZE> m_lut_b;

    OecfMetricsCollector m_metrics;

    void init_identity_lut() {
        for (unsigned i = 0; i < LUT_SIZE; i++) {
            m_lut_r[i] = m_lut_gr[i] = m_lut_gb[i] = m_lut_b[i] = (uint16_t)i;
        }
    }

    void process_thread() {
        o_href.write(false);
        o_vsync.write(false);
        o_raw.write(0);

        while (true) {
            wait();

            if (!rst_n.read()) {
                m_pixel_count = 0;
                m_line_count = -1;
                m_in_frame = false;
                m_href_delay = false;
                m_vsync_delay = false;
                m_data_delay = 0;
                continue;
            }

            bool curr_href = i_href.read();
            bool curr_vsync = i_vsync.read();
            uint16_t curr_data = i_raw.read();

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

            m_href_delay = curr_href && m_in_frame;
            m_vsync_delay = curr_vsync;
            m_data_delay = curr_data;

            uint16_t out_data = 0;
            bool out_href = m_href_delay;
            bool out_vsync = m_vsync_delay;

            if (enable.read() && m_in_frame && curr_href) {
                unsigned x = (unsigned)m_pixel_count;
                unsigned y = (unsigned)m_line_count;
                unsigned ch = get_bayer_channel(x, y);

                out_data = lut_lookup(curr_data, ch);
                m_metrics.record_lut_read();
                m_metrics.record_pixel();
                m_metrics.record_active_cycle();
                m_metrics.record_mem_read();

                if (ch == 0) m_metrics.record_r_channel();
                else if (ch == 1) m_metrics.record_gr_channel();
                else if (ch == 2) m_metrics.record_gb_channel();
                else m_metrics.record_b_channel();

                m_pixel_count++;
            }

            o_raw.write(out_data);
            o_href.write(out_href);
            o_vsync.write(out_vsync);

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
        m_href_delay = false;
        m_vsync_delay = false;
        m_data_delay = 0;
        m_metrics.reset();
    }

    unsigned get_bayer_channel(unsigned x, unsigned y) {
        bool odd_y = (y & 1);
        bool odd_x = (x & 1);
        switch (BAYER) {
            case BayerPattern::RGGB: return odd_y ? (odd_x ? 3 : 2) : (odd_x ? 1 : 0);
            case BayerPattern::GRBG: return odd_y ? (odd_x ? 3 : 0) : (odd_x ? 1 : 2);
            case BayerPattern::GBRG: return odd_y ? (odd_x ? 0 : 3) : (odd_x ? 1 : 2);
            case BayerPattern::BGGR: return odd_y ? (odd_x ? 1 : 0) : (odd_x ? 3 : 2);
            default: return odd_y ? (odd_x ? 3 : 2) : (odd_x ? 1 : 0);
        }
    }

    uint16_t lut_lookup(uint16_t addr, unsigned channel) {
        if (addr >= LUT_SIZE) return MAX_VAL;
        switch (channel) {
            case 0: return m_lut_r[addr];
            case 1: return m_lut_gr[addr];
            case 2: return m_lut_gb[addr];
            default: return m_lut_b[addr];
        }
    }
};

using isp_oecf_10b_rggb = isp_oecf<10, BayerPattern::RGGB>;

#endif // ISP_OECF_H
