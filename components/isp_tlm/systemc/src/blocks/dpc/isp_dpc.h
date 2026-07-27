/*
 * DPC (Defective Pixel Correction) Block
 * Gradient-based defective pixel detection and correction
 * Pipeline latency: 10 cycles (RTL DLY_CLK = 10)
 * Matches RTL: isp_dpc.v
 *
 * Algorithm: Gradient-Based DPC
 * - Build 5x5 window using line buffer
 * - Compute 4 directional gradients
 * - Select minimum gradient direction
 * - Defect detection based on threshold
 * - Replace with gradient-based interpolation
 */

#ifndef ISP_DPC_H
#define ISP_DPC_H

#include <systemc>
#include <array>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "blocks/dpc/dpc_metrics.h"

template<unsigned int BITS = 10, BayerPattern BAYER = BayerPattern::RGGB>
class isp_dpc : public sc_module {
public:
    static constexpr unsigned DLY_CLK = 10;
    static constexpr unsigned WINDOW_SIZE = 5;
    static constexpr unsigned HALF_WIN = WINDOW_SIZE / 2;

    sc_in<bool> pclk{"pclk"};
    sc_in<bool> rst_n{"rst_n"};
    sc_in<bool> enable{"enable"};

    sc_in<bool> i_href{"i_href"};
    sc_in<bool> i_vsync{"i_vsync"};
    sc_in<uint16_t> i_raw{"i_raw"};

    sc_out<bool> o_href{"o_href"};
    sc_out<bool> o_vsync{"o_vsync"};
    sc_out<uint16_t> o_raw{"o_raw"};

    sc_in<uint16_t> i_threshold{"i_threshold"};

    isp_dpc(const sc_module_name& name)
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
            for (unsigned j = 0; j < WINDOW_SIZE; j++) {
                for (unsigned k = 0; k < WINDOW_SIZE; k++) {
                    m_window[i][j][k] = 0;
                }
            }
        }

        for (unsigned i = 0; i < DLY_CLK; i++) {
            m_x_delay[i] = 0;
            m_y_delay[i] = 0;
        }
    }

    DpcMetricsCollector& get_metrics() { return m_metrics; }
    const DpcMetricsCollector& get_metrics() const { return m_metrics; }

    void set_image_size(unsigned w, unsigned h) { m_metrics.set_config(w, h); }

private:
    static constexpr uint16_t MAX_VAL = (1 << BITS) - 1;

    int m_pixel_count;
    int m_line_count;
    int m_frame_count;
    bool m_prev_vsync;
    bool m_prev_href;
    bool m_in_frame;

    bool m_href_delay[DLY_CLK];
    bool m_vsync_delay[DLY_CLK];
    uint16_t m_window[DLY_CLK][WINDOW_SIZE][WINDOW_SIZE];
    unsigned m_x_delay[DLY_CLK];
    unsigned m_y_delay[DLY_CLK];

    DpcMetricsCollector m_metrics;

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
                for (unsigned i = 0; i < DLY_CLK; i++) {
                    m_href_delay[i] = false;
                    m_vsync_delay[i] = false;
                    m_x_delay[i] = 0;
                    m_y_delay[i] = 0;
                }
                continue;
            }

            bool curr_href = i_href.read();
            bool curr_vsync = i_vsync.read();
            uint16_t curr_pixel = i_raw.read();

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
                m_x_delay[i] = m_x_delay[i - 1];
                m_y_delay[i] = m_y_delay[i - 1];
                for (unsigned j = 0; j < WINDOW_SIZE; j++) {
                    for (unsigned k = 0; k < WINDOW_SIZE; k++) {
                        m_window[i][j][k] = m_window[i - 1][j][k];
                    }
                }
            }

            m_href_delay[0] = curr_href && m_in_frame;
            m_vsync_delay[0] = curr_vsync;
            m_x_delay[0] = (unsigned)m_pixel_count;
            m_y_delay[0] = (unsigned)m_line_count;

            for (unsigned k = WINDOW_SIZE - 1; k > 0; k--) {
                for (unsigned j = WINDOW_SIZE - 1; j > 0; j--) {
                    m_window[0][j][k] = m_window[0][j - 1][k - 1];
                }
            }
            m_window[0][0][0] = curr_pixel;

            uint16_t out_data = 0;
            bool out_href = m_href_delay[DLY_CLK - 1];
            bool out_vsync = m_vsync_delay[DLY_CLK - 1];

            if (enable.read() && m_in_frame && m_href_delay[DLY_CLK - 1]) {
                unsigned x = m_x_delay[DLY_CLK - 1];
                unsigned y = m_y_delay[DLY_CLK - 1];

                uint16_t center = m_window[DLY_CLK - 1][HALF_WIN][HALF_WIN];
                uint16_t threshold = i_threshold.read();

                bool is_defect = detect_defect(center, threshold);

                if (is_defect) {
                    out_data = interpolate_defect(x, y);
                    m_metrics.record_corrected();
                    m_metrics.record_active_cycle();
                } else {
                    out_data = center;
                }

                m_metrics.record_pixel();
            }

            o_raw.write(out_data);
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
            m_x_delay[i] = 0;
            m_y_delay[i] = 0;
        }
        m_metrics.reset();
    }

    bool detect_defect(uint16_t center, uint16_t threshold) {
        uint16_t p0 = m_window[DLY_CLK - 1][HALF_WIN - 2][HALF_WIN - 2];
        uint16_t p1 = m_window[DLY_CLK - 1][HALF_WIN - 2][HALF_WIN];
        uint16_t p2 = m_window[DLY_CLK - 1][HALF_WIN - 1][HALF_WIN - 1];
        uint16_t p3 = m_window[DLY_CLK - 1][HALF_WIN - 1][HALF_WIN + 1];
        uint16_t p4 = m_window[DLY_CLK - 1][HALF_WIN][HALF_WIN - 2];
        uint16_t p5 = center;
        uint16_t p6 = m_window[DLY_CLK - 1][HALF_WIN][HALF_WIN + 2];
        uint16_t p7 = m_window[DLY_CLK - 1][HALF_WIN + 1][HALF_WIN - 1];
        uint16_t p8 = m_window[DLY_CLK - 1][HALF_WIN + 1][HALF_WIN + 1];

        int32_t grad_v = 2 * p5 - p2 - p8;
        int32_t grad_h = 2 * p5 - p4 - p6;
        int32_t grad_ld = 2 * p5 - p0 - p8;
        int32_t grad_rd = 2 * p5 - p1 - p7;

        if (grad_v < 0) grad_v = -grad_v;
        if (grad_h < 0) grad_h = -grad_h;
        if (grad_ld < 0) grad_ld = -grad_ld;
        if (grad_rd < 0) grad_rd = -grad_rd;

        bool below_min = (p5 <= p2) && (p5 <= p8) && (p5 <= p4) &&
                         (p5 <= p6) && (p5 <= p1) && (p5 <= p7) &&
                         (p5 <= p0) && (p5 <= p3);

        bool above_max = (p5 >= p2) && (p5 >= p8) && (p5 >= p4) &&
                         (p5 >= p6) && (p5 >= p1) && (p5 >= p7) &&
                         (p5 >= p0) && (p5 >= p3);

        bool is_hot = above_max && (grad_v > (int32_t)threshold || grad_h > (int32_t)threshold ||
                                    grad_ld > (int32_t)threshold || grad_rd > (int32_t)threshold);

        bool is_dead = below_min && (grad_v > (int32_t)threshold || grad_h > (int32_t)threshold ||
                                     grad_ld > (int32_t)threshold || grad_rd > (int32_t)threshold);

        if (is_hot) m_metrics.record_hot_pixel();
        if (is_dead) m_metrics.record_dead_pixel();

        return is_hot || is_dead;
    }

    uint16_t interpolate_defect(unsigned x, unsigned y) {
        uint16_t p0 = m_window[DLY_CLK - 1][HALF_WIN - 2][HALF_WIN - 2];
        uint16_t p1 = m_window[DLY_CLK - 1][HALF_WIN - 2][HALF_WIN];
        uint16_t p2 = m_window[DLY_CLK - 1][HALF_WIN - 1][HALF_WIN - 1];
        uint16_t p3 = m_window[DLY_CLK - 1][HALF_WIN - 1][HALF_WIN + 1];
        uint16_t p4 = m_window[DLY_CLK - 1][HALF_WIN][HALF_WIN - 2];
        uint16_t p5 = m_window[DLY_CLK - 1][HALF_WIN][HALF_WIN];
        uint16_t p6 = m_window[DLY_CLK - 1][HALF_WIN][HALF_WIN + 2];
        uint16_t p7 = m_window[DLY_CLK - 1][HALF_WIN + 1][HALF_WIN - 1];
        uint16_t p8 = m_window[DLY_CLK - 1][HALF_WIN + 1][HALF_WIN + 1];

        int32_t grad_v = 2 * p5 - p2 - p8;
        int32_t grad_h = 2 * p5 - p4 - p6;
        int32_t grad_ld = 2 * p5 - p0 - p8;
        int32_t grad_rd = 2 * p5 - p1 - p7;

        if (grad_v < 0) grad_v = -grad_v;
        if (grad_h < 0) grad_h = -grad_h;
        if (grad_ld < 0) grad_ld = -grad_ld;
        if (grad_rd < 0) grad_rd = -grad_rd;

        int32_t min_grad = grad_v;
        uint16_t interpolated = (p2 + p8) >> 1;
        m_metrics.record_vertical_grad();

        if (grad_h < min_grad) {
            min_grad = grad_h;
            interpolated = (p4 + p6) >> 1;
            m_metrics.record_horizontal_grad();
        }
        if (grad_ld < min_grad) {
            min_grad = grad_ld;
            interpolated = (p0 + p8) >> 1;
            m_metrics.record_left_diag_grad();
        }
        if (grad_rd < min_grad) {
            interpolated = (p1 + p7) >> 1;
            m_metrics.record_right_diag_grad();
        }

        return clamp(interpolated);
    }

    inline uint16_t clamp(int32_t val) {
        if (val < 0) return 0;
        if (val > MAX_VAL) return MAX_VAL;
        return (uint16_t)val;
    }
};

using isp_dpc_10b_rggb = isp_dpc<10, BayerPattern::RGGB>;
using isp_dpc_10b_grbg = isp_dpc<10, BayerPattern::GRBG>;
using isp_dpc_10b_gbrg = isp_dpc<10, BayerPattern::GBRG>;
using isp_dpc_10b_bggr = isp_dpc<10, BayerPattern::BGGR>;

#endif // ISP_DPC_H
