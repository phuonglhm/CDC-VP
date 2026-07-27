/*
 * 2DNR (2D Noise Reduction) Block
 * Weighted averaging filter with difference-based weights
 * Pipeline latency: ~30 cycles
 * Matches RTL: isp_2dnr.v
 *
 * Algorithm:
 * - 9x9 window on Y channel
 * - Calculate difference with center
 * - Assign weights based on difference LUT
 * - Weighted average filter
 * - Pass U/V through with delay
 */

#ifndef ISP_NR2D_H
#define ISP_NR2D_H

#include <systemc>
#include <array>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "blocks/2dnr/nr2d_metrics.h"

class isp_2dnr : public sc_module {
public:
    static constexpr unsigned DLY_CLK = 30;
    static constexpr unsigned WINDOW_SIZE = 9;
    static constexpr unsigned HALF_WIN = WINDOW_SIZE / 2;

    sc_in<bool> pclk{"pclk"};
    sc_in<bool> rst_n{"rst_n"};
    sc_in<bool> enable{"enable"};

    sc_in<bool> i_href{"i_href"};
    sc_in<bool> i_vsync{"i_vsync"};

    sc_in<uint8_t> i_data_y{"i_data_y"};
    sc_in<uint8_t> i_data_u{"i_data_u"};
    sc_in<uint8_t> i_data_v{"i_data_v"};

    sc_out<bool> o_href{"o_href"};
    sc_out<bool> o_vsync{"o_vsync"};

    sc_out<uint8_t> o_data_y{"o_data_y"};
    sc_out<uint8_t> o_data_u{"o_data_u"};
    sc_out<uint8_t> o_data_v{"o_data_v"};

    isp_2dnr(const sc_module_name& name)
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

        init_default_lut();

        for (unsigned i = 0; i < DLY_CLK; i++) {
            m_href_delay[i] = false;
            m_vsync_delay[i] = false;
            m_y_delay[i] = 0;
            m_u_delay[i] = 128;
            m_v_delay[i] = 128;
            m_x_delay[i] = 0;
            m_y_pos_delay[i] = 0;
        }
    }

    Nr2dMetricsCollector& get_metrics() { return m_metrics; }
    const Nr2dMetricsCollector& get_metrics() const { return m_metrics; }

    void set_image_size(unsigned w, unsigned h) { m_metrics.set_config(w, h); }

private:
    int m_pixel_count;
    int m_line_count;
    int m_frame_count;
    bool m_prev_vsync;
    bool m_prev_href;
    bool m_in_frame;

    bool m_href_delay[DLY_CLK];
    bool m_vsync_delay[DLY_CLK];
    uint8_t m_y_delay[DLY_CLK];
    uint8_t m_u_delay[DLY_CLK];
    uint8_t m_v_delay[DLY_CLK];
    unsigned m_x_delay[DLY_CLK];
    unsigned m_y_pos_delay[DLY_CLK];

    std::array<uint8_t, 32> m_diff_lut;
    std::array<uint8_t, 32> m_weight_lut;

    Nr2dMetricsCollector m_metrics;

    void init_default_lut() {
        for (unsigned i = 0; i < 32; i++) {
            m_diff_lut[i] = i * 8;
            m_weight_lut[i] = (31 - i);
        }
    }

    void process_thread() {
        o_href.write(false);
        o_vsync.write(false);
        o_data_y.write(0);
        o_data_u.write(128);
        o_data_v.write(128);

        while (true) {
            wait();

            if (!rst_n.read()) {
                m_pixel_count = 0;
                m_line_count = -1;
                m_in_frame = false;
                for (unsigned i = 0; i < DLY_CLK; i++) {
                    m_href_delay[i] = false;
                    m_vsync_delay[i] = false;
                    m_y_delay[i] = 0;
                    m_u_delay[i] = 128;
                    m_v_delay[i] = 128;
                    m_x_delay[i] = 0;
                    m_y_pos_delay[i] = 0;
                }
                continue;
            }

            bool curr_href = i_href.read();
            bool curr_vsync = i_vsync.read();
            uint8_t curr_y = i_data_y.read();
            uint8_t curr_u = i_data_u.read();
            uint8_t curr_v = i_data_v.read();

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
                m_y_delay[i] = m_y_delay[i - 1];
                m_u_delay[i] = m_u_delay[i - 1];
                m_v_delay[i] = m_v_delay[i - 1];
                m_x_delay[i] = m_x_delay[i - 1];
                m_y_pos_delay[i] = m_y_pos_delay[i - 1];
            }
            m_href_delay[0] = curr_href && m_in_frame;
            m_vsync_delay[0] = curr_vsync;
            m_y_delay[0] = curr_y;
            m_u_delay[0] = curr_u;
            m_v_delay[0] = curr_v;
            m_x_delay[0] = (unsigned)m_pixel_count;
            m_y_pos_delay[0] = (unsigned)m_line_count;

            uint8_t out_y = 0, out_u = 128, out_v = 128;
            bool out_href = m_href_delay[DLY_CLK - 1];
            bool out_vsync = m_vsync_delay[DLY_CLK - 1];

            if (enable.read() && m_in_frame && m_href_delay[DLY_CLK - 1]) {
                uint8_t center = m_y_delay[DLY_CLK - 1];

                uint8_t filtered = apply_weighted_filter(center);

                out_y = filtered;
                out_u = m_u_delay[DLY_CLK - 1];
                out_v = m_v_delay[DLY_CLK - 1];

                m_metrics.record_filtered_pixel();
                m_metrics.record_pixel();
                m_metrics.record_active_cycle();

                int diff = (int)center - (int)filtered;
                m_metrics.record_noise_reduction(diff);
            }

            o_data_y.write(out_y);
            o_data_u.write(out_u);
            o_data_v.write(out_v);
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
            m_y_delay[i] = 0;
            m_u_delay[i] = 128;
            m_v_delay[i] = 128;
            m_x_delay[i] = 0;
            m_y_pos_delay[i] = 0;
        }
        m_metrics.reset();
    }

    uint8_t apply_weighted_filter(uint8_t center) {
        int32_t sum = 0;
        int32_t weight_sum = 0;

        for (unsigned i = 0; i < WINDOW_SIZE; i++) {
            for (unsigned j = 0; j < WINDOW_SIZE; j++) {
                uint8_t neighbor = m_y_delay[0];

                int diff = (int)center - (int)neighbor;
                if (diff < 0) diff = -diff;

                unsigned lut_idx = (diff > 248) ? 31 : diff / 8;
                uint8_t weight = m_weight_lut[lut_idx];

                sum += neighbor * weight;
                weight_sum += weight;

                m_metrics.record_weight_bin(lut_idx);
            }
        }

        if (weight_sum == 0) return center;

        return (uint8_t)(sum / weight_sum);
    }
};

#endif // ISP_NR2D_H
