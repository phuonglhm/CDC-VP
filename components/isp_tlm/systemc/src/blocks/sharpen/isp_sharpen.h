/*
 * SHARP (Sharpening/Unsharp Masking) Block
 * Applies sharpening using 9x9 convolution kernel
 * Pipeline latency: 15 cycles (RTL DLY_CLK = 15)
 * Matches RTL: isp_sharpen.v
 *
 * Algorithm: Unsharp Masking
 * - 9x9 convolution on Y channel
 * - Subtract smoothed from original
 * - Scale by strength
 * - Add back to original
 * - Clip to 0-255
 */

#ifndef ISP_SHARPEN_H
#define ISP_SHARPEN_H

#include <systemc>
#include <array>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "blocks/sharpen/sharpen_metrics.h"

class isp_sharpen : public sc_module {
public:
    SC_HAS_PROCESS(isp_sharpen);

    static constexpr unsigned DLY_CLK = 15;
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

    sc_in<uint16_t> i_sharpen_strength{"i_sharpen_strength"};

    isp_sharpen(const sc_module_name& name)
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

        init_default_kernel();

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

    SharpenMetricsCollector& get_metrics() { return m_metrics; }
    const SharpenMetricsCollector& get_metrics() const { return m_metrics; }

    void set_image_size(unsigned w, unsigned h) { m_metrics.set_config(w, h); }

    bool set_kernel_entry(unsigned row, unsigned column, int value) {
        if (row >= WINDOW_SIZE || column >= WINDOW_SIZE) {
            return false;
        }
        m_kernel[row][column] = value;
        return true;
    }

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

    int m_kernel[WINDOW_SIZE][WINDOW_SIZE];

    SharpenMetricsCollector m_metrics;

    void init_default_kernel() {
        int k[WINDOW_SIZE][WINDOW_SIZE] = {
            {0, 0, -1, -1, -1, -1, -1, 0, 0},
            {0, -1, -2, -3, -3, -3, -2, -1, 0},
            {-1, -2, -3, 0, 6, 0, -3, -2, -1},
            {-1, -3, 0, 6, 20, 6, 0, -3, -1},
            {-1, -3, 6, 20, 36, 20, 6, -3, -1},
            {-1, -3, 0, 6, 20, 6, 0, -3, -1},
            {-1, -2, -3, 0, 6, 0, -3, -2, -1},
            {0, -1, -2, -3, -3, -3, -2, -1, 0},
            {0, 0, -1, -1, -1, -1, -1, 0, 0}
        };
        for (unsigned i = 0; i < WINDOW_SIZE; i++) {
            for (unsigned j = 0; j < WINDOW_SIZE; j++) {
                m_kernel[i][j] = k[i][j];
            }
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

            if (enable.read() && m_href_delay[DLY_CLK - 1]) {
                uint8_t y_center = m_y_delay[DLY_CLK - 1];

                int32_t sum = apply_9x9_kernel(y_center);

                int32_t diff = (int32_t)y_center - sum;
                uint16_t strength = i_sharpen_strength.read();
                int32_t scaled_diff = (diff * strength) >> 10;

                int32_t sharpened = (int32_t)y_center + scaled_diff;
                out_y = clamp_8bit(sharpened);

                out_u = m_u_delay[DLY_CLK - 1];
                out_v = m_v_delay[DLY_CLK - 1];

                m_metrics.record_sharpened_pixel();
                m_metrics.record_kernel_app();
                m_metrics.record_pixel();
                m_metrics.record_active_cycle();

                if (diff > 10 || diff < -10) {
                    m_metrics.record_edge_enhancement();
                }
            } else if (!enable.read()) {
                out_y = m_y_delay[DLY_CLK - 1];
                out_u = m_u_delay[DLY_CLK - 1];
                out_v = m_v_delay[DLY_CLK - 1];
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

    int32_t apply_9x9_kernel(uint8_t center) {
        int32_t sum = 0;
        int32_t center_contrib = 0;

        for (unsigned i = 0; i < WINDOW_SIZE; i++) {
            for (unsigned j = 0; j < WINDOW_SIZE; j++) {
                unsigned h = HALF_WIN;
                int offset_y = (int)i - (int)h;
                int offset_x = (int)j - (int)h;

                int32_t neighbor_val = center;

                int dy = (int)m_y_pos_delay[DLY_CLK - 1] + offset_y;
                int dx = (int)m_x_delay[DLY_CLK - 1] + offset_x;

                if (dy >= 0 && dx >= 0) {
                    unsigned idx = (offset_y + 1) * DLY_CLK + (offset_x + 1);
                    if (idx < DLY_CLK * WINDOW_SIZE) {
                        neighbor_val = m_y_delay[0];
                    }
                }

                int kernel_val = m_kernel[i][j];
                sum += neighbor_val * kernel_val;

                if (i == h && j == h) {
                    center_contrib = neighbor_val * kernel_val;
                }
            }
        }

        if (center_contrib != 0) {
            sum = sum / 256;
        }

        return sum;
    }

    inline uint8_t clamp_8bit(int32_t val) {
        if (val < 0) return 0;
        if (val > 255) return 255;
        return (uint8_t)val;
    }
};

#endif // ISP_SHARPEN_H
