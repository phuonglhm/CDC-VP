/*
 * DGAIN (Digital Gain) Block
 * Applies digital gain from array lookup
 * Pipeline latency: 2 cycles (RTL DLY_CLK = 2)
 * Matches RTL: isp_dgain.v
 */

#ifndef ISP_DGAIN_H
#define ISP_DGAIN_H

#include <systemc>
#include <array>
#include "common/common_defs.h"
#include "common/isp_types.h"

template<unsigned int BITS = 10>
class isp_dgain : public sc_module {
public:
    SC_HAS_PROCESS(isp_dgain);

    static constexpr unsigned DLY_CLK = 2;
    static constexpr unsigned GAIN_ARRAY_SIZE = 100;
    static constexpr unsigned GAIN_ARRAY_BITS = 7;

    sc_in<bool> pclk{"pclk"};
    sc_in<bool> rst_n{"rst_n"};
    sc_in<bool> enable{"enable"};

    sc_in<bool> i_href{"i_href"};
    sc_in<bool> i_vsync{"i_vsync"};
    sc_in<uint16_t> i_raw{"i_raw"};

    sc_out<bool> o_href{"o_href"};
    sc_out<bool> o_vsync{"o_vsync"};
    sc_out<uint16_t> o_raw{"o_raw"};

    sc_out<uint8_t> o_applied_index{"o_applied_index"};

    sc_in<bool> i_is_manual{"i_is_manual"};
    sc_in<uint8_t> i_manual_index{"i_manual_index"};
    sc_in<uint8_t> i_ae_feedback_index{"i_ae_feedback_index"};
    sc_in<uint8_t> i_dgain_array[GAIN_ARRAY_SIZE];

    isp_dgain(const sc_module_name& name)
        : sc_module(name)
        , m_pixel_count(0)
        , m_line_count(0)
        , m_frame_count(0)
        , m_prev_vsync(false)
        , m_prev_href(false)
        , m_in_frame(false)
        , m_applied_index(0)
    {
        SC_THREAD(process_thread);
        sensitive << pclk.pos();
        dont_initialize();

        SC_THREAD(reset_handler);
        sensitive << rst_n.neg();

        for (unsigned i = 0; i < DLY_CLK; i++) {
            m_href_delay[i] = false;
            m_vsync_delay[i] = false;
            m_data_delay[i] = 0;
        }
    }

private:
    static constexpr uint16_t MAX_VAL = (1 << BITS) - 1;

    int m_pixel_count;
    int m_line_count;
    int m_frame_count;
    bool m_prev_vsync;
    bool m_prev_href;
    bool m_in_frame;
    uint8_t m_applied_index;

    bool m_href_delay[DLY_CLK];
    bool m_vsync_delay[DLY_CLK];
    uint16_t m_data_delay[DLY_CLK];

    void process_thread() {
        o_href.write(false);
        o_vsync.write(false);
        o_raw.write(0);
        o_applied_index.write(0);

        while (true) {
            wait();

            if (!rst_n.read()) {
                m_pixel_count = 0;
                m_line_count = -1;
                m_in_frame = false;
                m_applied_index = 0;
                for (unsigned i = 0; i < DLY_CLK; i++) {
                    m_href_delay[i] = false;
                    m_vsync_delay[i] = false;
                    m_data_delay[i] = 0;
                }
                continue;
            }

            bool curr_href = i_href.read();
            bool curr_vsync = i_vsync.read();
            uint16_t curr_data = i_raw.read();

            bool vsync_fall = m_prev_vsync && !curr_vsync;
            bool vsync_rise = !m_prev_vsync && curr_vsync;
            bool href_rise = !m_prev_href && curr_href;

            if (vsync_fall) {
                m_frame_count++;
                m_line_count = -1;
                m_pixel_count = 0;
                m_in_frame = true;
            }
            if (vsync_rise) {
                m_in_frame = false;
            }
            if (href_rise && m_in_frame) {
                m_line_count++;
                m_pixel_count = 0;
            }

            for (unsigned i = DLY_CLK - 1; i > 0; i--) {
                m_href_delay[i] = m_href_delay[i - 1];
                m_vsync_delay[i] = m_vsync_delay[i - 1];
                m_data_delay[i] = m_data_delay[i - 1];
            }
            m_href_delay[0] = curr_href && m_in_frame;
            m_vsync_delay[0] = curr_vsync;
            m_data_delay[0] = curr_data;

            uint16_t out_data = 0;
            bool out_href = m_href_delay[DLY_CLK - 1];
            bool out_vsync = m_vsync_delay[DLY_CLK - 1];

            uint8_t gain_idx = 0;
            if (i_is_manual.read()) {
                gain_idx = i_manual_index.read();
            } else {
                gain_idx = i_ae_feedback_index.read();
            }
            gain_idx = (gain_idx >= GAIN_ARRAY_SIZE) ? (GAIN_ARRAY_SIZE - 1) : gain_idx;
            m_applied_index = gain_idx;

            if (enable.read() && m_href_delay[DLY_CLK - 1]) {
                const uint16_t gain = i_dgain_array[gain_idx].read();
                const uint32_t result =
                    static_cast<uint32_t>(
                        m_data_delay[DLY_CLK - 1]) * gain;
                out_data = (result > MAX_VAL) ? MAX_VAL : (uint16_t)result;
            } else if (!enable.read()) {
                out_data = m_data_delay[DLY_CLK - 1];
            }

            o_raw.write(out_data);
            o_href.write(out_href);
            o_vsync.write(out_vsync);
            o_applied_index.write(m_applied_index);

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
        m_applied_index = 0;
        for (unsigned i = 0; i < DLY_CLK; i++) {
            m_href_delay[i] = false;
            m_vsync_delay[i] = false;
            m_data_delay[i] = 0;
        }
    }
};

using isp_dgain_10b = isp_dgain<10>;

#endif // ISP_DGAIN_H
