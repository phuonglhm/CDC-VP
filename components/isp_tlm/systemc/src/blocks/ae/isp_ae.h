/*
 * AE (Auto Exposure) Block
 * Computes exposure adjustment from frame statistics
 * Output: Statistics only (no pixel processing)
 * Matches RTL: isp_ae.v
 *
 * Algorithm: Moment-based exposure control
 * - Crop input region
 * - Compute statistics (sum, sum², sum³)
 * - Calculate moments
 * - Compute skewness
 * - Adjust exposure response
 */

#ifndef ISP_AE_H
#define ISP_AE_H

#include <systemc>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "blocks/ae/ae_metrics.h"

template<unsigned int BITS = 10>
class isp_ae : public sc_module {
public:
    SC_HAS_PROCESS(isp_ae);

    sc_in<bool> pclk{"pclk"};
    sc_in<bool> rst_n{"rst_n"};
    sc_in<bool> enable{"enable"};

    sc_in<bool> i_href{"i_href"};
    sc_in<bool> i_vsync{"i_vsync"};
    sc_in<uint16_t> i_data{"i_data"};

    sc_out<int8_t> o_ae_response{"o_ae_response"};
    sc_out<uint16_t> o_ae_result_skewness{"o_ae_result_skewness"};
    sc_out<bool> o_ae_done{"o_ae_done"};

    sc_in<uint8_t> i_center_illuminance{"i_center_illuminance"};
    sc_in<uint16_t> i_skewness{"i_skewness"};

    sc_in<uint16_t> i_ae_crop_left{"i_ae_crop_left"};
    sc_in<uint16_t> i_ae_crop_right{"i_ae_crop_right"};
    sc_in<uint16_t> i_ae_crop_top{"i_ae_crop_top"};
    sc_in<uint16_t> i_ae_crop_bottom{"i_ae_crop_bottom"};

    isp_ae(const sc_module_name& name)
        : sc_module(name)
        , m_pixel_count(0)
        , m_line_count(0)
        , m_frame_count(0)
        , m_prev_vsync(false)
        , m_in_frame(false)
        , m_sum1(0)
        , m_sum2(0)
        , m_sum3(0)
        , m_pixel_sum(0)
        , m_ae_done(false)
        , m_prev_frame_end(false)
    {
        SC_THREAD(process_thread);
        sensitive << pclk.pos();
        dont_initialize();

        SC_THREAD(reset_handler);
        sensitive << rst_n.neg();
    }

    AeMetricsCollector& get_metrics() { return m_metrics; }
    const AeMetricsCollector& get_metrics() const { return m_metrics; }

    void set_image_size(unsigned w, unsigned h) { m_metrics.set_config(w, h); }

private:
    int m_pixel_count;
    int m_line_count;
    int m_frame_count;
    bool m_prev_vsync;
    bool m_in_frame;

    uint64_t m_sum1;
    uint64_t m_sum2;
    uint64_t m_sum3;
    uint64_t m_pixel_sum;

    bool m_ae_done;
    bool m_prev_frame_end;

    AeMetricsCollector m_metrics;

    void process_thread() {
        o_ae_response.write(0);
        o_ae_result_skewness.write(0);
        o_ae_done.write(false);

        while (true) {
            wait();

            if (!rst_n.read()) {
                m_pixel_count = 0;
                m_line_count = -1;
                m_frame_count = 0;
                m_in_frame = false;
                m_sum1 = 0;
                m_sum2 = 0;
                m_sum3 = 0;
                m_pixel_sum = 0;
                m_ae_done = false;
                m_prev_frame_end = false;
                o_ae_response.write(0);
                o_ae_result_skewness.write(0);
                o_ae_done.write(false);
                continue;
            }

            bool curr_href = i_href.read();
            bool curr_vsync = i_vsync.read();
            uint16_t curr_pixel = i_data.read();

            bool vsync_fall = m_prev_vsync && !curr_vsync;
            bool vsync_rise = !m_prev_vsync && curr_vsync;

            if (vsync_fall) {
                m_frame_count++;
                m_line_count = -1;
                m_pixel_count = 0;
                m_in_frame = true;
                m_metrics.record_frame();
            }
            if (vsync_rise) {
                m_in_frame = false;
                m_prev_frame_end = true;
                compute_exposure();
            }
            if (curr_href && m_in_frame) {
                m_line_count++;
                m_pixel_count = 0;
            }

            m_metrics.record_total_cycle();

            if (enable.read() && m_in_frame && curr_href) {
                unsigned x = (unsigned)m_pixel_count;
                unsigned y = (unsigned)m_line_count;

                uint16_t crop_l = i_ae_crop_left.read();
                uint16_t crop_r = i_ae_crop_right.read();
                uint16_t crop_t = i_ae_crop_top.read();
                uint16_t crop_b = i_ae_crop_bottom.read();

                if (x >= crop_l && x < crop_r && y >= crop_t && y < crop_b) {
                    m_sum1 += curr_pixel;
                    m_sum2 += (uint64_t)curr_pixel * curr_pixel;
                    m_sum3 += (uint64_t)curr_pixel * curr_pixel * curr_pixel;
                    m_pixel_sum++;
                }

                m_metrics.record_pixel();
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
        m_sum1 = 0;
        m_sum2 = 0;
        m_sum3 = 0;
        m_pixel_sum = 0;
        m_ae_done = false;
        m_prev_frame_end = false;
        m_metrics.reset();
    }

    void compute_exposure() {
        if (m_pixel_sum == 0) {
            o_ae_response.write(0);
            o_ae_result_skewness.write(0);
            o_ae_done.write(true);
            return;
        }

        m_metrics.record_ae_iteration();

        double mean = (double)m_sum1 / m_pixel_sum;
        double mean2 = (double)m_sum2 / m_pixel_sum;
        double mean3 = (double)m_sum3 / m_pixel_sum;

        double variance = mean2 - mean * mean;
        double std_dev = (variance > 0) ? std::sqrt(variance) : 1.0;

        double skewness = 0.0;
        if (std_dev > 0.001) {
            double third_moment = mean3 - 3 * mean * mean2 + 2 * mean * mean * mean;
            skewness = third_moment / (std_dev * std_dev * std_dev);
        }

        const double scaled_skewness = skewness * 256.0;
        const uint16_t skewness_fixed =
            scaled_skewness <= 0.0
                ? 0
                : (scaled_skewness >= 65535.0
                       ? 65535
                       : static_cast<uint16_t>(scaled_skewness));

        o_ae_result_skewness.write(skewness_fixed);
        m_metrics.record_skewness(skewness);

        uint8_t center_target = i_center_illuminance.read();
        (void)i_skewness.read();

        int8_t response = 0;
        if (mean < center_target * 8) {
            response = 2;
        } else if (mean > center_target * 12) {
            response = -2;
        } else if (mean < center_target * 10) {
            response = 1;
        } else if (mean > center_target * 10) {
            response = -1;
        }

        o_ae_response.write(response);
        m_metrics.record_exposure_adjustment();

        m_sum1 = 0;
        m_sum2 = 0;
        m_sum3 = 0;
        m_pixel_sum = 0;

        o_ae_done.write(true);
        m_ae_done = true;
    }
};

using isp_ae_10b = isp_ae<10>;

#endif // ISP_AE_H
