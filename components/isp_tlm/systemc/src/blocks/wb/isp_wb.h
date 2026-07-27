/*
 * WB (White Balance) Block
 * Applies channel gains for white balance correction
 * Pipeline latency: 3 cycles (RTL DLY_CLK = 3)
 * Matches RTL: isp_wb.v
 */

#ifndef ISP_WB_H
#define ISP_WB_H

#include <systemc>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "blocks/wb/wb_metrics.h"

template<unsigned int BITS = 10, BayerPattern BAYER = BayerPattern::RGGB>
class isp_wb : public sc_module {
public:
    static constexpr unsigned DLY_CLK = 3;

    sc_in<bool> pclk{"pclk"};
    sc_in<bool> rst_n{"rst_n"};
    sc_in<bool> enable{"enable"};

    sc_in<bool> i_href{"i_href"};
    sc_in<bool> i_vsync{"i_vsync"};
    sc_in<uint16_t> i_data{"i_data"};

    sc_out<bool> o_href{"o_href"};
    sc_out<bool> o_vsync{"o_vsync"};
    sc_out<uint16_t> o_data{"o_data"};

    sc_in<uint16_t> i_gain_r{"i_gain_r"};
    sc_in<uint16_t> i_gain_b{"i_gain_b"};

    isp_wb(const sc_module_name& name)
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

        m_href_delay[0] = m_href_delay[1] = m_href_delay[2] = false;
        m_vsync_delay[0] = m_vsync_delay[1] = m_vsync_delay[2] = false;
        m_data_delay[0] = m_data_delay[1] = m_data_delay[2] = 0;
        m_x_delay[0] = m_x_delay[1] = m_x_delay[2] = 0;
        m_y_delay[0] = m_y_delay[1] = m_y_delay[2] = 0;
    }

    WbMetricsCollector& get_metrics() { return m_metrics; }
    const WbMetricsCollector& get_metrics() const { return m_metrics; }

    void set_image_size(unsigned w, unsigned h) { m_metrics.set_config(w, h); }

    unsigned get_pixel_count() const { return m_pixel_count >= 0 ? (unsigned)m_pixel_count : 0; }
    unsigned get_frame_count() const { return m_frame_count >= 0 ? (unsigned)m_frame_count : 0; }

private:
    static constexpr uint16_t MAX_VAL = (1 << BITS) - 1;
    static constexpr uint16_t GAIN_FRAC_BITS = 10;

    int m_pixel_count;
    int m_line_count;
    int m_frame_count;
    bool m_prev_vsync;
    bool m_prev_href;
    bool m_in_frame;

    bool m_href_delay[DLY_CLK];
    bool m_vsync_delay[DLY_CLK];
    uint16_t m_data_delay[DLY_CLK];
    unsigned m_x_delay[DLY_CLK];
    unsigned m_y_delay[DLY_CLK];

    WbMetricsCollector m_metrics;

    void process_thread() {
        o_href.write(false);
        o_vsync.write(false);
        o_data.write(0);

        while (true) {
            wait();

            if (!rst_n.read()) {
                m_pixel_count = 0;
                m_line_count = -1;
                m_in_frame = false;
                for (unsigned i = 0; i < DLY_CLK; i++) {
                    m_href_delay[i] = false;
                    m_vsync_delay[i] = false;
                    m_data_delay[i] = 0;
                    m_x_delay[i] = 0;
                    m_y_delay[i] = 0;
                }
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
                m_data_delay[i] = m_data_delay[i - 1];
                m_x_delay[i] = m_x_delay[i - 1];
                m_y_delay[i] = m_y_delay[i - 1];
            }
            m_href_delay[0] = curr_href && m_in_frame;
            m_vsync_delay[0] = curr_vsync;
            m_data_delay[0] = curr_data;
            m_x_delay[0] = (unsigned)m_pixel_count;
            m_y_delay[0] = (unsigned)m_line_count;

            uint16_t gain_r = i_gain_r.read();
            uint16_t gain_b = i_gain_b.read();

            uint16_t out_data = 0;
            bool out_href = m_href_delay[DLY_CLK - 1];
            bool out_vsync = m_vsync_delay[DLY_CLK - 1];

            if (enable.read() && m_in_frame && m_href_delay[DLY_CLK - 1]) {
                uint16_t in_pixel = m_data_delay[DLY_CLK - 1];
                unsigned x = m_x_delay[DLY_CLK - 1];
                unsigned y = m_y_delay[DLY_CLK - 1];

                unsigned channel = get_bayer_channel(x, y);

                uint32_t corrected;
                if (channel == 0) {
                    corrected = apply_gain(in_pixel, gain_r);
                    m_metrics.record_r_gain();
                } else if (channel == 3) {
                    corrected = apply_gain(in_pixel, gain_b);
                    m_metrics.record_b_gain();
                } else {
                    corrected = in_pixel;
                }

                if (corrected > MAX_VAL) {
                    corrected = MAX_VAL;
                    m_metrics.record_clip();
                }

                m_metrics.record_pixel();
                m_metrics.record_active_cycle();
                m_metrics.record_mul();

                out_data = (uint16_t)corrected;
            }

            o_data.write(out_data);
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
            m_data_delay[i] = 0;
            m_x_delay[i] = 0;
            m_y_delay[i] = 0;
        }
        m_metrics.reset();
    }

    unsigned get_bayer_channel(unsigned x, unsigned y) {
        bool odd_y = (y & 1);
        bool odd_x = (x & 1);

        switch (BAYER) {
            case BayerPattern::RGGB:
                return odd_y ? (odd_x ? 3 : 2) : (odd_x ? 1 : 0);
            case BayerPattern::GRBG:
                return odd_y ? (odd_x ? 3 : 0) : (odd_x ? 1 : 2);
            case BayerPattern::GBRG:
                return odd_y ? (odd_x ? 0 : 3) : (odd_x ? 1 : 2);
            case BayerPattern::BGGR:
                return odd_y ? (odd_x ? 1 : 0) : (odd_x ? 3 : 2);
            default:
                return odd_y ? (odd_x ? 3 : 2) : (odd_x ? 1 : 0);
        }
    }

    uint16_t apply_gain(uint16_t pixel, uint16_t gain) {
        uint32_t result = ((uint32_t)pixel * gain) >> GAIN_FRAC_BITS;
        return (uint16_t)result;
    }
};

using isp_wb_10b_rggb = isp_wb<10, BayerPattern::RGGB>;
using isp_wb_10b_grbg = isp_wb<10, BayerPattern::GRBG>;
using isp_wb_10b_gbrg = isp_wb<10, BayerPattern::GBRG>;
using isp_wb_10b_bggr = isp_wb<10, BayerPattern::BGGR>;
using isp_wb_8b_rggb = isp_wb<8, BayerPattern::RGGB>;

#endif // ISP_WB_H
