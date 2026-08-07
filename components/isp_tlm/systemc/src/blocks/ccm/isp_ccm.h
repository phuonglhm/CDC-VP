/*
 * CCM (Color Correction Matrix) Block
 * Applies 3x3 color correction matrix to RGB pixels
 * Pipeline latency: 4 cycles (RTL DLY_CLK = 4)
 * Matches RTL: isp_ccm.v
 *
 * Algorithm:
 *   Rout = (Mrr*Rin + Mrg*Gin + Mrb*Bin) >> 10
 *   Gout = (Mgr*Rin + Mgg*Gin + Mgb*Bin) >> 10
 *   Bout = (Mbr*Rin + Mbg*Gin + Mbb*Bin) >> 10
 *
 * Coefficients are in S8.8 fixed-point format
 */

#ifndef ISP_CCM_H
#define ISP_CCM_H

#include <systemc>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "blocks/ccm/ccm_metrics.h"

template<unsigned int BITS = 10>
class isp_ccm : public sc_module {
public:
    SC_HAS_PROCESS(isp_ccm);

    static constexpr unsigned DLY_CLK = 4;

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

    sc_out<uint16_t> o_r{"o_r"};
    sc_out<uint16_t> o_g{"o_g"};
    sc_out<uint16_t> o_b{"o_b"};

    // CCM coefficients (S8.8 fixed-point format)
    sc_in<int16_t> i_m_rr{"i_m_rr"}, i_m_rg{"i_m_rg"}, i_m_rb{"i_m_rb"};
    sc_in<int16_t> i_m_gr{"i_m_gr"}, i_m_gg{"i_m_gg"}, i_m_gb{"i_m_gb"};
    sc_in<int16_t> i_m_br{"i_m_br"}, i_m_bg{"i_m_bg"}, i_m_bb{"i_m_bb"};

    isp_ccm(const sc_module_name& name)
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

    CcmMetricsCollector& get_metrics() { return m_metrics; }
    const CcmMetricsCollector& get_metrics() const { return m_metrics; }

    void set_image_size(unsigned w, unsigned h) { m_metrics.set_config(w, h); }

    unsigned get_pixel_count() const { return m_pixel_count >= 0 ? (unsigned)m_pixel_count : 0; }
    unsigned get_frame_count() const { return m_frame_count >= 0 ? (unsigned)m_frame_count : 0; }

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

    CcmMetricsCollector m_metrics;

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

            uint16_t out_r = 0, out_g = 0, out_b = 0;
            bool out_href = m_href_delay[DLY_CLK - 1];
            bool out_vsync = m_vsync_delay[DLY_CLK - 1];

            if (enable.read() && m_href_delay[DLY_CLK - 1]) {
                uint16_t r_in = m_r_delay[DLY_CLK - 1];
                uint16_t g_in = m_g_delay[DLY_CLK - 1];
                uint16_t b_in = m_b_delay[DLY_CLK - 1];

                int16_t m_rr = i_m_rr.read();
                int16_t m_rg = i_m_rg.read();
                int16_t m_rb = i_m_rb.read();
                int16_t m_gr = i_m_gr.read();
                int16_t m_gg = i_m_gg.read();
                int16_t m_gb = i_m_gb.read();
                int16_t m_br = i_m_br.read();
                int16_t m_bg = i_m_bg.read();
                int16_t m_bb = i_m_bb.read();

                int32_t r_result = (int32_t)r_in * m_rr +
                                   (int32_t)g_in * m_rg +
                                   (int32_t)b_in * m_rb;
                int32_t g_result = (int32_t)r_in * m_gr +
                                   (int32_t)g_in * m_gg +
                                   (int32_t)b_in * m_gb;
                int32_t b_result = (int32_t)r_in * m_br +
                                   (int32_t)g_in * m_bg +
                                   (int32_t)b_in * m_bb;

                r_result = (r_result + 512) >> 10;
                g_result = (g_result + 512) >> 10;
                b_result = (b_result + 512) >> 10;

                bool saturated = false;
                if (r_result < 0) {
                    r_result = 0;
                    saturated = true;
                } else if (r_result > MAX_VAL) {
                    r_result = MAX_VAL;
                    saturated = true;
                }
                if (g_result < 0) {
                    g_result = 0;
                    saturated = true;
                } else if (g_result > MAX_VAL) {
                    g_result = MAX_VAL;
                    saturated = true;
                }
                if (b_result < 0) {
                    b_result = 0;
                    saturated = true;
                } else if (b_result > MAX_VAL) {
                    b_result = MAX_VAL;
                    saturated = true;
                }

                out_r = (uint16_t)r_result;
                out_g = (uint16_t)g_result;
                out_b = (uint16_t)b_result;

                m_metrics.record_ccm_multiplication();
                m_metrics.record_ccm_multiplication();
                m_metrics.record_ccm_multiplication();
                m_metrics.record_pixel();
                m_metrics.record_active_cycle();
                m_metrics.record_mul();
                m_metrics.record_mul();
                m_metrics.record_mul();

                if (saturated) {
                    m_metrics.record_saturation();
                }
            } else if (!enable.read()) {
                out_r = m_r_delay[DLY_CLK - 1];
                out_g = m_g_delay[DLY_CLK - 1];
                out_b = m_b_delay[DLY_CLK - 1];
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
            m_r_delay[i] = m_g_delay[i] = m_b_delay[i] = 0;
        }
        m_metrics.reset();
    }
};

using isp_ccm_10b = isp_ccm<10>;
using isp_ccm_8b = isp_ccm<8>;

#endif // ISP_CCM_H
