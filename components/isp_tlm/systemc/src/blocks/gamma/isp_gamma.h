/*
 * GAMMA (Gamma Correction) Block
 * Applies gamma correction using LUT (Look-Up Table)
 * Pipeline latency: 2 cycles (RTL href_dly[1])
 * Matches RTL: isp_gamma.v
 *
 * Uses dual-port RAM for LUT: config port + processing port
 */

#ifndef ISP_GAMMA_H
#define ISP_GAMMA_H

#include <systemc>
#include <array>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "blocks/gamma/gamma_metrics.h"

template<unsigned int BITS = 10>
class isp_gamma : public sc_module {
public:
    SC_HAS_PROCESS(isp_gamma);

    static constexpr unsigned DLY_CLK = 2;
    static constexpr unsigned LUT_SIZE = 1 << BITS;

    sc_in<bool> pclk{"pclk"};
    sc_in<bool> rst_n{"rst_n"};
    sc_in<bool> enable{"enable"};

    sc_in<bool> i_href{"i_href"};
    sc_in<bool> i_vsync{"i_vsync"};

    sc_in<uint16_t> i_data_r{"i_data_r"};
    sc_in<uint16_t> i_data_g{"i_data_g"};
    sc_in<uint16_t> i_data_b{"i_data_b"};

    sc_out<bool> o_href{"o_href"};
    sc_out<bool> o_vsync{"o_vsync"};

    sc_out<uint16_t> o_data_r{"o_data_r"};
    sc_out<uint16_t> o_data_g{"o_data_g"};
    sc_out<uint16_t> o_data_b{"o_data_b"};

    isp_gamma(const sc_module_name& name)
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

        init_lut();

        for (unsigned i = 0; i < DLY_CLK; i++) {
            m_href_delay[i] = false;
            m_vsync_delay[i] = false;
            m_r_delay[i] = m_g_delay[i] = m_b_delay[i] = 0;
        }
    }

    GammaMetricsCollector& get_metrics() { return m_metrics; }
    const GammaMetricsCollector& get_metrics() const { return m_metrics; }

    void set_image_size(unsigned w, unsigned h) { m_metrics.set_config(w, h); }

    bool set_lut_entry(unsigned index, uint16_t value) {
        if (index >= LUT_SIZE || value > MAX_VAL) {
            return false;
        }
        m_lut_r[index] = value;
        m_lut_g[index] = value;
        m_lut_b[index] = value;
        return true;
    }

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
    uint16_t m_r_delay[DLY_CLK];
    uint16_t m_g_delay[DLY_CLK];
    uint16_t m_b_delay[DLY_CLK];

    std::array<uint16_t, LUT_SIZE> m_lut_r;
    std::array<uint16_t, LUT_SIZE> m_lut_g;
    std::array<uint16_t, LUT_SIZE> m_lut_b;

    GammaMetricsCollector m_metrics;

    void init_lut() {
        for (unsigned i = 0; i < LUT_SIZE; i++) {
            double normalized = (double)i / MAX_VAL;
            double gamma_corrected = std::pow(normalized, 1.0 / 2.2);
            uint16_t lut_val = (uint16_t)(gamma_corrected * MAX_VAL);
            m_lut_r[i] = m_lut_g[i] = m_lut_b[i] = lut_val;
        }
    }

    void process_thread() {
        o_href.write(false);
        o_vsync.write(false);
        o_data_r.write(0);
        o_data_g.write(0);
        o_data_b.write(0);

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
            uint16_t curr_r = i_data_r.read();
            uint16_t curr_g = i_data_g.read();
            uint16_t curr_b = i_data_b.read();

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

            uint16_t out_r = 0, out_g = 0, out_b = 0;
            bool out_href = m_href_delay[DLY_CLK - 1];
            bool out_vsync = m_vsync_delay[DLY_CLK - 1];

            if (enable.read() && m_href_delay[DLY_CLK - 1]) {
                uint16_t r_in = m_r_delay[DLY_CLK - 1];
                uint16_t g_in = m_g_delay[DLY_CLK - 1];
                uint16_t b_in = m_b_delay[DLY_CLK - 1];

                out_r = lut_lookup(r_in, m_lut_r);
                out_g = lut_lookup(g_in, m_lut_g);
                out_b = lut_lookup(b_in, m_lut_b);

                m_metrics.record_lut_lookup();
                m_metrics.record_lut_lookup();
                m_metrics.record_lut_lookup();
                m_metrics.record_r_lut();
                m_metrics.record_g_lut();
                m_metrics.record_b_lut();
                m_metrics.record_pixel();
                m_metrics.record_active_cycle();
                m_metrics.record_mem_read();
                m_metrics.record_mem_read();
                m_metrics.record_mem_read();
            } else if (!enable.read()) {
                out_r = m_r_delay[DLY_CLK - 1];
                out_g = m_g_delay[DLY_CLK - 1];
                out_b = m_b_delay[DLY_CLK - 1];
            }

            o_data_r.write(out_r);
            o_data_g.write(out_g);
            o_data_b.write(out_b);
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

    uint16_t lut_lookup(uint16_t addr, const std::array<uint16_t, LUT_SIZE>& lut) {
        if (addr >= LUT_SIZE) return MAX_VAL;
        return lut[addr];
    }
};

using isp_gamma_10b = isp_gamma<10>;
using isp_gamma_8b = isp_gamma<8>;

#endif // ISP_GAMMA_H
