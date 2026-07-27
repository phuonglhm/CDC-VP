/*
 * DEMOSAIC (Demosaicing/Bayer CFA Interpolation) Block
 * Converts Bayer RAW to full RGB using bilinear interpolation
 * Pipeline latency: 9 cycles (RTL DLY_CLK = 9)
 * Matches RTL: isp_demosaic.v / isp_cfa.v
 *
 * Algorithm: Bilinear CFA Interpolation
 * - Build 5x5 window with line buffer
 * - Red extraction: filter based on pattern
 * - Green extraction: bilinear interpolation
 * - Blue extraction: filter based on pattern
 * - Handle all 4 Bayer patterns (RGGB, GRBG, GBRG, BGGR)
 */

#ifndef ISP_DEMOSAIC_H
#define ISP_DEMOSAIC_H

#include <systemc>
#include <array>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "blocks/demosaic/demosaic_metrics.h"

template<unsigned int BITS = 10, BayerPattern BAYER = BayerPattern::RGGB>
class isp_demosaic : public sc_module {
public:
    static constexpr unsigned DLY_CLK = 9;
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

    sc_out<uint16_t> o_r{"o_r"};
    sc_out<uint16_t> o_g{"o_g"};
    sc_out<uint16_t> o_b{"o_b"};

    isp_demosaic(const sc_module_name& name)
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

    DemosaicMetricsCollector& get_metrics() { return m_metrics; }
    const DemosaicMetricsCollector& get_metrics() const { return m_metrics; }

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

    DemosaicMetricsCollector m_metrics;

    void process_thread() {
        o_href.write(false);
        o_vsync.write(false);
        o_r.write(0);
        o_g.write(0);
        o_b.write(0);

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
                m_window[0][k][0] = m_window[0][k - 1][0];
            }
            m_window[0][0][0] = curr_pixel;

            uint16_t out_r = 0, out_g = 0, out_b = 0;
            bool out_href = m_href_delay[DLY_CLK - 1];
            bool out_vsync = m_vsync_delay[DLY_CLK - 1];

            if (enable.read() && m_in_frame && m_href_delay[DLY_CLK - 1]) {
                unsigned x = m_x_delay[DLY_CLK - 1];
                unsigned y = m_y_delay[DLY_CLK - 1];

                interpolate_rgb(x, y, out_r, out_g, out_b);

                m_metrics.record_pixel();
                m_metrics.record_r_pixel();
                m_metrics.record_g_pixel();
                m_metrics.record_b_pixel();
                m_metrics.record_active_cycle();
            }

            o_r.write(out_r);
            o_g.write(out_g);
            o_b.write(out_b);
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

    void interpolate_rgb(unsigned x, unsigned y, uint16_t& r, uint16_t& g, uint16_t& b) {
        unsigned ch = get_bayer_channel(x, y);

        r = g = b = 0;

        if (ch == 0) {
            r = m_window[DLY_CLK - 1][HALF_WIN][HALF_WIN];
            g = bilinear_green(x, y);
            b = bilinear_blue_rggb(x, y);
        } else if (ch == 1) {
            g = m_window[DLY_CLK - 1][HALF_WIN][HALF_WIN];
            r = bilinear_red_grbg(x, y);
            b = bilinear_blue_grbg(x, y);
        } else if (ch == 2) {
            g = m_window[DLY_CLK - 1][HALF_WIN][HALF_WIN];
            r = bilinear_red_gbrg(x, y);
            b = bilinear_blue_gbrg(x, y);
        } else {
            b = m_window[DLY_CLK - 1][HALF_WIN][HALF_WIN];
            r = bilinear_red_bggr(x, y);
            g = bilinear_green(x, y);
        }

        r = clamp(r);
        g = clamp(g);
        b = clamp(b);
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

    uint16_t bilinear_green(unsigned x, unsigned y) {
        unsigned h = HALF_WIN;
        uint16_t g00 = m_window[DLY_CLK - 1][h-1][h];
        uint16_t g10 = m_window[DLY_CLK - 1][h+1][h];
        uint16_t g01 = m_window[DLY_CLK - 1][h][h-1];
        uint16_t g11 = m_window[DLY_CLK - 1][h][h+1];

        uint16_t gh = (g00 + g10) >> 1;
        uint16_t gv = (g01 + g11) >> 1;

        return (gh + gv) >> 1;
    }

    uint16_t bilinear_red_rggb(unsigned x, unsigned y) {
        unsigned h = HALF_WIN;
        uint16_t r00 = m_window[DLY_CLK - 1][h-1][h-1];
        uint16_t r10 = m_window[DLY_CLK - 1][h-1][h+1];
        uint16_t r01 = m_window[DLY_CLK - 1][h+1][h-1];
        uint16_t r11 = m_window[DLY_CLK - 1][h+1][h+1];

        return (r00 + r10 + r01 + r11) >> 2;
    }

    uint16_t bilinear_blue_rggb(unsigned x, unsigned y) {
        unsigned h = HALF_WIN;
        uint16_t b00 = m_window[DLY_CLK - 1][h-1][h-1];
        uint16_t b10 = m_window[DLY_CLK - 1][h-1][h+1];
        uint16_t b01 = m_window[DLY_CLK - 1][h+1][h-1];
        uint16_t b11 = m_window[DLY_CLK - 1][h+1][h+1];

        return (b00 + b10 + b01 + b11) >> 2;
    }

    uint16_t bilinear_red_grbg(unsigned x, unsigned y) {
        unsigned h = HALF_WIN;
        uint16_t r00 = m_window[DLY_CLK - 1][h-1][h];
        uint16_t r10 = m_window[DLY_CLK - 1][h+1][h];

        return (r00 + r10) >> 1;
    }

    uint16_t bilinear_blue_grbg(unsigned x, unsigned y) {
        unsigned h = HALF_WIN;
        uint16_t b00 = m_window[DLY_CLK - 1][h][h-1];
        uint16_t b10 = m_window[DLY_CLK - 1][h][h+1];

        return (b00 + b10) >> 1;
    }

    uint16_t bilinear_red_gbrg(unsigned x, unsigned y) {
        unsigned h = HALF_WIN;
        uint16_t r00 = m_window[DLY_CLK - 1][h][h-1];
        uint16_t r10 = m_window[DLY_CLK - 1][h][h+1];

        return (r00 + r10) >> 1;
    }

    uint16_t bilinear_blue_gbrg(unsigned x, unsigned y) {
        unsigned h = HALF_WIN;
        uint16_t b00 = m_window[DLY_CLK - 1][h-1][h];
        uint16_t b10 = m_window[DLY_CLK - 1][h+1][h];

        return (b00 + b10) >> 1;
    }

    uint16_t bilinear_red_bggr(unsigned x, unsigned y) {
        unsigned h = HALF_WIN;
        uint16_t r00 = m_window[DLY_CLK - 1][h-1][h-1];
        uint16_t r10 = m_window[DLY_CLK - 1][h-1][h+1];
        uint16_t r01 = m_window[DLY_CLK - 1][h+1][h-1];
        uint16_t r11 = m_window[DLY_CLK - 1][h+1][h+1];

        return (r00 + r10 + r01 + r11) >> 2;
    }

    uint16_t bilinear_blue_bggr(unsigned x, unsigned y) {
        unsigned h = HALF_WIN;
        uint16_t b00 = m_window[DLY_CLK - 1][h-1][h-1];
        uint16_t b10 = m_window[DLY_CLK - 1][h-1][h+1];
        uint16_t b01 = m_window[DLY_CLK - 1][h+1][h-1];
        uint16_t b11 = m_window[DLY_CLK - 1][h+1][h+1];

        return (b00 + b10 + b01 + b11) >> 2;
    }

    inline uint16_t clamp(uint32_t val) {
        if (val > MAX_VAL) return MAX_VAL;
        return (uint16_t)val;
    }
};

using isp_demosaic_10b_rggb = isp_demosaic<10, BayerPattern::RGGB>;
using isp_demosaic_10b_grbg = isp_demosaic<10, BayerPattern::GRBG>;
using isp_demosaic_10b_gbrg = isp_demosaic<10, BayerPattern::GBRG>;
using isp_demosaic_10b_bggr = isp_demosaic<10, BayerPattern::BGGR>;
using isp_demosaic_8b_rggb = isp_demosaic<8, BayerPattern::RGGB>;

#endif // ISP_DEMOSAIC_H
