/*
 * AWB (Auto White Balance) Block
 * Computes white balance gains from frame statistics
 * Output: Statistics only (no pixel processing)
 * Matches RTL: isp_awb.v
 *
 * Algorithm: Gray World Assumption
 * - Crop valid region (excluding edges)
 * - Filter by exposure limits
 * - Compute mean R, G, B per frame
 * - Calculate gains: R_gain = mean_G / mean_R, B_gain = mean_G / mean_B
 */

#ifndef ISP_AWB_H
#define ISP_AWB_H

#include <systemc>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "blocks/awb/awb_metrics.h"

template<unsigned int BITS = 10, BayerPattern BAYER = BayerPattern::RGGB>
class isp_awb : public sc_module {
public:
    sc_in<bool> pclk{"pclk"};
    sc_in<bool> rst_n{"rst_n"};
    sc_in<bool> enable{"enable"};

    sc_in<bool> i_href{"i_href"};
    sc_in<bool> i_vsync{"i_vsync"};
    sc_in<uint16_t> i_raw{"i_raw"};

    sc_out<uint16_t> o_r_gain{"o_r_gain"};
    sc_out<uint16_t> o_b_gain{"o_b_gain"};

    sc_in<uint16_t> i_underexposed_limit{"i_underexposed_limit"};
    sc_in<uint16_t> i_overexposed_limit{"i_overexposed_limit"};
    sc_in<uint8_t> i_frames{"i_frames"};

    isp_awb(const sc_module_name& name)
        : sc_module(name)
        , m_pixel_count(0)
        , m_line_count(0)
        , m_frame_count(0)
        , m_prev_vsync(false)
        , m_in_frame(false)
        , m_frame_r_sum(0)
        , m_frame_g_sum(0)
        , m_frame_b_sum(0)
        , m_frame_valid_count(0)
    {
        SC_THREAD(process_thread);
        sensitive << pclk.pos();
        dont_initialize();

        SC_THREAD(reset_handler);
        sensitive << rst_n.neg();
    }

    AwbMetricsCollector& get_metrics() { return m_metrics; }
    const AwbMetricsCollector& get_metrics() const { return m_metrics; }

    void set_image_size(unsigned w, unsigned h) { m_metrics.set_config(w, h); }

private:
    int m_pixel_count;
    int m_line_count;
    int m_frame_count;
    bool m_prev_vsync;
    bool m_in_frame;

    uint64_t m_frame_r_sum;
    uint64_t m_frame_g_sum;
    uint64_t m_frame_b_sum;
    uint64_t m_frame_valid_count;

    double m_accum_r_sum;
    double m_accum_g_sum;
    double m_accum_b_sum;
    unsigned m_accum_frames;

    AwbMetricsCollector m_metrics;

    void process_thread() {
        o_r_gain.write(1024);
        o_b_gain.write(1024);

        while (true) {
            wait();

            if (!rst_n.read()) {
                m_pixel_count = 0;
                m_line_count = -1;
                m_frame_count = 0;
                m_in_frame = false;
                m_frame_r_sum = 0;
                m_frame_g_sum = 0;
                m_frame_b_sum = 0;
                m_frame_valid_count = 0;
                m_accum_r_sum = 0;
                m_accum_g_sum = 0;
                m_accum_b_sum = 0;
                m_accum_frames = 0;
                o_r_gain.write(1024);
                o_b_gain.write(1024);
                continue;
            }

            bool curr_href = i_href.read();
            bool curr_vsync = i_vsync.read();
            uint16_t curr_pixel = i_raw.read();

            bool vsync_fall = m_prev_vsync && !curr_vsync;
            bool vsync_rise = !m_prev_vsync && curr_vsync;

            if (vsync_fall) {
                m_frame_count++;
                m_line_count = -1;
                m_pixel_count = 0;
                m_in_frame = true;
                m_metrics.record_frame();
                m_metrics.record_frame_processed();
            }
            if (vsync_rise) {
                m_in_frame = false;
                compute_gains();
            }
            if (curr_href && !m_prev_vsync && curr_vsync == false && m_in_frame) {
                m_line_count++;
                m_pixel_count = 0;
            }

            m_metrics.record_total_cycle();

            if (enable.read() && m_in_frame && curr_href) {
                unsigned ch = get_bayer_channel((unsigned)m_pixel_count, (unsigned)m_line_count);
                uint16_t underexp = i_underexposed_limit.read();
                uint16_t overexp = i_overexposed_limit.read();

                if (curr_pixel > underexp && curr_pixel < overexp) {
                    m_metrics.record_valid_pixel();

                    if (ch == 0) m_frame_r_sum += curr_pixel;
                    else if (ch == 1 || ch == 2) m_frame_g_sum += curr_pixel;
                    else m_frame_b_sum += curr_pixel;

                    m_frame_valid_count++;
                } else if (curr_pixel >= overexp) {
                    m_metrics.record_overexposed();
                } else {
                    m_metrics.record_underexposed();
                }

                m_pixel_count++;
            }

            m_prev_vsync = curr_vsync;
        }
    }

    void reset_handler() {
        wait();
        m_pixel_count = 0;
        m_line_count = -1;
        m_frame_count = 0;
        m_in_frame = false;
        m_frame_r_sum = 0;
        m_frame_g_sum = 0;
        m_frame_b_sum = 0;
        m_frame_valid_count = 0;
        m_accum_r_sum = 0;
        m_accum_g_sum = 0;
        m_accum_b_sum = 0;
        m_accum_frames = 0;
        m_metrics.reset();
    }

    void compute_gains() {
        if (m_frame_valid_count == 0) return;

        double mean_r = (double)m_frame_r_sum / m_frame_valid_count;
        double mean_g = (double)m_frame_g_sum / m_frame_valid_count;
        double mean_b = (double)m_frame_b_sum / m_frame_valid_count;

        m_accum_r_sum += mean_r;
        m_accum_g_sum += mean_g;
        m_accum_b_sum += mean_b;
        m_accum_frames++;

        uint8_t target_frames = i_frames.read();
        if (target_frames == 0) target_frames = 1;

        if (m_accum_frames >= target_frames) {
            double avg_r = m_accum_r_sum / m_accum_frames;
            double avg_g = m_accum_g_sum / m_accum_frames;
            double avg_b = m_accum_b_sum / m_accum_frames;

            double r_gain = (avg_r > 0) ? (avg_g / avg_r) : 1.0;
            double b_gain = (avg_b > 0) ? (avg_g / avg_b) : 1.0;

            r_gain = std::min(std::max(r_gain, 0.5), 4.0);
            b_gain = std::min(std::max(b_gain, 0.5), 4.0);

            uint16_t r_gain_fixed = (uint16_t)(r_gain * 1024.0);
            uint16_t b_gain_fixed = (uint16_t)(b_gain * 1024.0);

            o_r_gain.write(r_gain_fixed);
            o_b_gain.write(b_gain_fixed);

            m_metrics.record_r_gain(r_gain);
            m_metrics.record_b_gain(b_gain);

            m_accum_r_sum = 0;
            m_accum_g_sum = 0;
            m_accum_b_sum = 0;
            m_accum_frames = 0;
        }

        m_frame_r_sum = 0;
        m_frame_g_sum = 0;
        m_frame_b_sum = 0;
        m_frame_valid_count = 0;
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
};

using isp_awb_10b_rggb = isp_awb<10, BayerPattern::RGGB>;

#endif // ISP_AWB_H
