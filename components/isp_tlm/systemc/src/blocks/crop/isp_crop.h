/*
 * CROP (Cropping) Block
 * Zero latency - combinational gating of pixels based on crop window
 * Matches RTL: isp_crop.v
 */

#ifndef ISP_CROP_H
#define ISP_CROP_H

#include <systemc>
#include <cmath>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "blocks/crop/crop_metrics.h"

template<unsigned int BITS = 10, unsigned int WIDTH = 2048, unsigned int HEIGHT = 1536>
class isp_crop : public sc_module {
public:
    SC_HAS_PROCESS(isp_crop);

    sc_in<bool> pclk{"pclk"};
    sc_in<bool> rst_n{"rst_n"};
    sc_in<bool> enable{"enable"};

    // Input video interface
    sc_in<bool> i_href{"i_href"};
    sc_in<bool> i_vsync{"i_vsync"};
    sc_in<uint16_t> i_data{"i_data"};

    // Output video interface
    sc_out<bool> o_href{"o_href"};
    sc_out<bool> o_vsync{"o_vsync"};
    sc_out<uint16_t> o_data{"o_data"};

    // Crop parameters (matching RTL)
    sc_in<uint16_t> i_crop_w{"i_crop_w"};   // Output width
    sc_in<uint16_t> i_crop_h{"i_crop_h"};   // Output height
    sc_in<uint16_t> i_crop_x{"i_crop_x"};   // Start X (default: centered)
    sc_in<uint16_t> i_crop_y{"i_crop_y"};   // Start Y (default: centered)

    isp_crop(const sc_module_name& name)
        : sc_module(name)
        , m_pixel_count(0)
        , m_line_count(0)
        , m_frame_count(0)
        , m_prev_vsync(false)
        , m_prev_href(false)
        , m_in_frame(false)
        , m_in_crop_window(false)
    {
        SC_THREAD(process_thread);
        sensitive << pclk.pos();
        dont_initialize();

        SC_THREAD(reset_handler);
        sensitive << rst_n.neg();
    }

    CropMetricsCollector& get_metrics() { return m_metrics; }
    const CropMetricsCollector& get_metrics() const { return m_metrics; }

    void set_image_size(unsigned w, unsigned h) { m_metrics.set_config(w, h); }

    unsigned get_pixel_count() const { return m_pixel_count >= 0 ? (unsigned)m_pixel_count : 0; }
    unsigned get_frame_count() const { return m_frame_count >= 0 ? (unsigned)m_frame_count : 0; }
    unsigned get_width() const { return m_metrics.get_width(); }
    unsigned get_height() const { return m_metrics.get_height(); }
    bool in_frame() const { return m_in_frame; }
    bool in_crop_window() const { return m_in_crop_window; }

private:
    static constexpr uint16_t MAX_VAL = (1 << BITS) - 1;

    int m_pixel_count;
    int m_line_count;
    int m_frame_count;
    bool m_prev_vsync;
    bool m_prev_href;
    bool m_in_frame;
    bool m_in_crop_window;

    CropMetricsCollector m_metrics;

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
                m_in_crop_window = false;
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
                m_in_crop_window = false;
                m_metrics.record_frame();
            }
            if (vsync_rise) {
                m_in_frame = false;
                m_in_crop_window = false;
            }
            if (href_rise && m_in_frame) {
                m_line_count++;
                m_pixel_count = 0;
                m_metrics.record_line();
            }

            m_metrics.record_total_cycle();

            const bool input_valid = curr_href && m_in_frame;
            const bool block_enabled = enable.read();
            uint16_t out_data = block_enabled ? 0 : curr_data;
            bool out_href = !block_enabled && input_valid;
            bool out_vsync = curr_vsync;

            if (block_enabled && input_valid) {
                m_metrics.record_input_pixel();

                unsigned x = (unsigned)m_pixel_count;
                unsigned y = (unsigned)m_line_count;

                uint16_t crop_x = i_crop_x.read();
                uint16_t crop_y = i_crop_y.read();
                uint16_t crop_w = i_crop_w.read();
                uint16_t crop_h = i_crop_h.read();

                if (crop_w == 0) crop_w = WIDTH;
                if (crop_h == 0) crop_h = HEIGHT;

                bool in_window = (x >= crop_x) && (x < crop_x + crop_w) &&
                                 (y >= crop_y) && (y < crop_y + crop_h);

                m_in_crop_window = in_window;

                if (in_window) {
                    m_metrics.record_output_pixel();
                    out_data = curr_data;
                    out_href = true;
                    m_metrics.record_active_cycle();
                } else {
                    m_metrics.record_cropped_pixel();
                    m_metrics.record_stall_cycle();
                    if (x >= crop_x + crop_w) {
                        m_metrics.record_crop_right();
                    }
                }
            } else {
                m_in_crop_window = false;
            }

            if (input_valid) {
                m_pixel_count++;
            }

            o_data.write(out_data);
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
        m_in_crop_window = false;
        m_prev_vsync = false;
        m_prev_href = false;
        m_metrics.reset();
    }
};

using isp_crop_10b = isp_crop<10, 2048, 1536>;
using isp_crop_8b = isp_crop<8, 2048, 1536>;

#endif // ISP_CROP_H
