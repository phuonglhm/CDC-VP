/*
 * BNR (Bayer Noise Reduction) Block
 * Joint Bilateral Filter for noise reduction
 * Pipeline latency: ~50 cycles
 * Matches RTL: isp_bnr.v
 *
 * Algorithm: Joint Bilateral Filter
 * - Green interpolation first
 * - Apply JBF on interpolated green
 * - 5x5 spatial kernel + range kernel from color curve
 */

#ifndef ISP_BNR_H
#define ISP_BNR_H

#include <systemc>
#include <array>
#include "common/common_defs.h"
#include "common/isp_types.h"
#include "blocks/bnr/bnr_metrics.h"

template<unsigned int BITS = 10, BayerPattern BAYER = BayerPattern::RGGB>
class isp_bnr : public sc_module {
public:
    SC_HAS_PROCESS(isp_bnr);

    static constexpr unsigned DLY_CLK = 50;
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

    isp_bnr(const sc_module_name& name)
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
            m_raw_delay[i] = 0;
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

    BnrMetricsCollector& get_metrics() { return m_metrics; }
    const BnrMetricsCollector& get_metrics() const { return m_metrics; }

    void set_image_size(unsigned w, unsigned h) { m_metrics.set_config(w, h); }

    bool set_spatial_entry(unsigned channel,
                           unsigned row,
                           unsigned column,
                           uint8_t value) {
        if (channel >= 3 || row >= WINDOW_SIZE || column >= WINDOW_SIZE) {
            return false;
        }
        m_kernel[channel][row][column] = value;
        return true;
    }

    bool set_color_entry(unsigned channel,
                         unsigned index,
                         uint16_t difference,
                         uint8_t weight) {
        if (channel >= 3 || index >= COLOR_CURVE_SIZE ||
            difference > MAX_VAL) {
            return false;
        }
        m_color_x[channel][index] = difference;
        m_color_y[channel][index] = weight;
        return true;
    }

private:
    static constexpr uint16_t MAX_VAL = (1 << BITS) - 1;
    static constexpr unsigned COLOR_CURVE_SIZE = 9;

    int m_pixel_count;
    int m_line_count;
    int m_frame_count;
    bool m_prev_vsync;
    bool m_prev_href;
    bool m_in_frame;

    bool m_href_delay[DLY_CLK];
    bool m_vsync_delay[DLY_CLK];
    uint16_t m_raw_delay[DLY_CLK];
    uint16_t m_window[DLY_CLK][WINDOW_SIZE][WINDOW_SIZE];
    unsigned m_x_delay[DLY_CLK];
    unsigned m_y_delay[DLY_CLK];

    int m_kernel[3][WINDOW_SIZE][WINDOW_SIZE];
    uint16_t m_color_x[3][COLOR_CURVE_SIZE];
    uint8_t m_color_y[3][COLOR_CURVE_SIZE];

    BnrMetricsCollector m_metrics;

    void init_default_kernel() {
        int k[WINDOW_SIZE][WINDOW_SIZE] = {
            {1, 4, 7, 4, 1},
            {4, 16, 26, 16, 4},
            {7, 26, 41, 26, 7},
            {4, 16, 26, 16, 4},
            {1, 4, 7, 4, 1}
        };
        for (unsigned channel = 0; channel < 3; ++channel) {
            for (unsigned i = 0; i < WINDOW_SIZE; i++) {
                for (unsigned j = 0; j < WINDOW_SIZE; j++) {
                    m_kernel[channel][i][j] = k[i][j];
                }
            }
            for (unsigned index = 0; index < COLOR_CURVE_SIZE; ++index) {
                m_color_x[channel][index] = static_cast<uint16_t>(
                    (static_cast<uint32_t>(MAX_VAL) * index) /
                    (COLOR_CURVE_SIZE - 1));
                m_color_y[channel][index] =
                    static_cast<uint8_t>(255U -
                                         index * (255U /
                                                  (COLOR_CURVE_SIZE - 1)));
            }
        }
    }

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
            m_raw_delay[i] = 0;
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
                m_raw_delay[i] = m_raw_delay[i - 1];
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
            m_raw_delay[0] = curr_pixel;
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

            if (enable.read() && m_href_delay[DLY_CLK - 1]) {
                uint16_t center = m_window[DLY_CLK - 1][HALF_WIN][HALF_WIN];

                const unsigned channel = bayer_channel(
                    m_x_delay[DLY_CLK - 1],
                    m_y_delay[DLY_CLK - 1]);
                uint16_t filtered =
                    apply_bilateral_filter(center, channel);

                out_data = filtered;

                m_metrics.record_filtered_pixel();
                m_metrics.record_kernel_app();
                m_metrics.record_pixel();
                m_metrics.record_active_cycle();
            } else if (!enable.read()) {
                out_data = m_raw_delay[DLY_CLK - 1];
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
            m_raw_delay[i] = 0;
            m_x_delay[i] = 0;
            m_y_delay[i] = 0;
        }
        m_metrics.reset();
    }

    unsigned bayer_channel(unsigned x, unsigned y) const {
        const bool even_x = (x & 1U) == 0;
        const bool even_y = (y & 1U) == 0;
        switch (BAYER) {
            case BayerPattern::RGGB:
                return even_y ? (even_x ? 0U : 1U)
                              : (even_x ? 1U : 2U);
            case BayerPattern::GRBG:
                return even_y ? (even_x ? 1U : 0U)
                              : (even_x ? 2U : 1U);
            case BayerPattern::GBRG:
                return even_y ? (even_x ? 1U : 2U)
                              : (even_x ? 0U : 1U);
            case BayerPattern::BGGR:
                return even_y ? (even_x ? 2U : 1U)
                              : (even_x ? 1U : 0U);
        }
        return 1U;
    }

    int range_weight(unsigned channel, unsigned difference) const {
        if (difference <= m_color_x[channel][0]) {
            return m_color_y[channel][0];
        }
        for (unsigned index = 1; index < COLOR_CURVE_SIZE; ++index) {
            const unsigned high_x = m_color_x[channel][index];
            if (difference <= high_x) {
                const unsigned low_x = m_color_x[channel][index - 1];
                const int low_y = m_color_y[channel][index - 1];
                const int high_y = m_color_y[channel][index];
                if (high_x <= low_x) {
                    return high_y;
                }
                const int delta_y = high_y - low_y;
                return low_y +
                       delta_y * static_cast<int>(difference - low_x) /
                           static_cast<int>(high_x - low_x);
            }
        }
        return m_color_y[channel][COLOR_CURVE_SIZE - 1];
    }

    uint16_t apply_bilateral_filter(uint16_t center, unsigned channel) {
        int64_t sum = 0;
        int64_t weight_sum = 0;

        for (unsigned i = 0; i < WINDOW_SIZE; i++) {
            for (unsigned j = 0; j < WINDOW_SIZE; j++) {
                uint16_t neighbor = m_window[DLY_CLK - 1][i][j];

                int spatial_weight = m_kernel[channel][i][j];

                int range_diff = (int)center - (int)neighbor;
                if (range_diff < 0) range_diff = -range_diff;

                const int range = range_weight(
                    channel, static_cast<unsigned>(range_diff));

                int weight = spatial_weight * range;

                sum += (int64_t)neighbor * weight;
                weight_sum += weight;
            }
        }

        if (weight_sum == 0) return center;

        uint16_t result = (uint16_t)(sum / weight_sum);
        return clamp(result);
    }

    inline uint16_t clamp(int32_t val) {
        if (val < 0) return 0;
        if (val > MAX_VAL) return MAX_VAL;
        return (uint16_t)val;
    }
};

using isp_bnr_10b_rggb = isp_bnr<10, BayerPattern::RGGB>;

#endif // ISP_BNR_H
