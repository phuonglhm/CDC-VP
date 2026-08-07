/*
 * CFA demosaic model.
 *
 * This implementation is deliberately streaming: it keeps the current and
 * previous RAW rows, uses all causally available same-colour neighbours, and
 * emits one RGB sample for every input sample. A one-pixel look-ahead within
 * each row plus a fixed token pipeline preserves href/vsync and drains the
 * final pixels after input VSYNC rises. At the top/left image boundaries,
 * missing neighbours fall back to the centre sample.
 */

#ifndef ISP_DEMOSAIC_H
#define ISP_DEMOSAIC_H

#include <systemc>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "blocks/demosaic/demosaic_metrics.h"
#include "common/common_defs.h"
#include "common/isp_types.h"

template<unsigned int BITS = 10,
         BayerPattern BAYER = BayerPattern::RGGB>
class isp_demosaic : public sc_module {
public:
    SC_HAS_PROCESS(isp_demosaic);

    static_assert(BITS > 0 && BITS <= 16,
                  "demosaic supports 1-16 bit RAW samples");

    static constexpr unsigned DLY_CLK = 9;

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

    explicit isp_demosaic(const sc_module_name& name)
        : sc_module(name)
    {
        SC_THREAD(process_thread);
        sensitive << pclk.pos();
        dont_initialize();

        SC_THREAD(reset_handler);
        sensitive << rst_n.neg();

        clear_pipeline();
    }

    DemosaicMetricsCollector& get_metrics() { return m_metrics; }
    const DemosaicMetricsCollector& get_metrics() const {
        return m_metrics;
    }

    // Kept public for compatibility with the existing block-level diagnostic
    // code. It counts valid RGB samples emitted by the delayed interface.
    unsigned m_pixels_output = 0;

    // Kept for compatibility with earlier diagnostics. The array is a simple
    // input-sample delay and is not used as a substitute for line storage.
    uint16_t m_raw_delay[DLY_CLK]{};

    void set_image_size(unsigned width, unsigned height)
    {
        image_width_ = width;
        image_height_ = height;
        previous_row_.assign(image_width_, 0);
        current_row_.assign(image_width_, 0);
        m_metrics.set_config(width, height);
    }

private:
    static constexpr uint16_t MAX_VAL =
        static_cast<uint16_t>((uint32_t{1} << BITS) - 1U);

    struct RgbToken {
        bool href = false;
        bool vsync = false;
        uint16_t r = 0;
        uint16_t g = 0;
        uint16_t b = 0;
    };

    unsigned image_width_ = 0;
    unsigned image_height_ = 0;
    unsigned pixel_count_ = 0;
    unsigned line_count_ = 0;
    unsigned frame_count_ = 0;
    bool previous_vsync_ = false;
    bool previous_href_ = false;
    bool in_frame_ = false;
    std::vector<uint16_t> previous_row_;
    std::vector<uint16_t> current_row_;
    RgbToken token_delay_[DLY_CLK];
    DemosaicMetricsCollector m_metrics;

    void clear_pipeline()
    {
        for (unsigned index = 0; index < DLY_CLK; ++index) {
            token_delay_[index] = {};
            m_raw_delay[index] = 0;
        }
    }

    void clear_outputs()
    {
        o_href.write(false);
        o_vsync.write(false);
        o_r.write(0);
        o_g.write(0);
        o_b.write(0);
    }

    void reset_state()
    {
        pixel_count_ = 0;
        line_count_ = 0;
        frame_count_ = 0;
        previous_vsync_ = false;
        previous_href_ = false;
        in_frame_ = false;
        m_pixels_output = 0;
        std::fill(previous_row_.begin(), previous_row_.end(), 0);
        std::fill(current_row_.begin(), current_row_.end(), 0);
        clear_pipeline();
    }

    static unsigned bayer_colour(unsigned x, unsigned y)
    {
        const bool odd_x = (x & 1U) != 0;
        const bool odd_y = (y & 1U) != 0;
        switch (BAYER) {
            case BayerPattern::RGGB:
                return odd_y ? (odd_x ? 2U : 1U)
                             : (odd_x ? 1U : 0U);
            case BayerPattern::GRBG:
                return odd_y ? (odd_x ? 1U : 2U)
                             : (odd_x ? 0U : 1U);
            case BayerPattern::GBRG:
                return odd_y ? (odd_x ? 1U : 0U)
                             : (odd_x ? 2U : 1U);
            case BayerPattern::BGGR:
                return odd_y ? (odd_x ? 0U : 1U)
                             : (odd_x ? 1U : 2U);
        }
        return 1U;
    }

    uint16_t row_sample(unsigned row,
                        unsigned column,
                        unsigned current_y) const
    {
        if (row == current_y) {
            return current_row_[column];
        }
        return previous_row_[column];
    }

    uint16_t interpolate_colour(unsigned x,
                                unsigned y,
                                unsigned target_colour,
                                uint16_t centre) const
    {
        if (bayer_colour(x, y) == target_colour ||
            image_width_ == 0) {
            return centre;
        }

        uint32_t sum = 0;
        unsigned samples = 0;
        for (int row_offset = -1; row_offset <= 0; ++row_offset) {
            if (row_offset < 0 && y == 0) {
                continue;
            }
            const unsigned row =
                row_offset < 0 ? y - 1U : y;
            for (int column_offset = -1;
                 column_offset <= 1; ++column_offset) {
                int candidate_x =
                    static_cast<int>(x) + column_offset;
                candidate_x = std::max(
                    0,
                    std::min(candidate_x,
                             static_cast<int>(image_width_ - 1U)));
                const unsigned column =
                    static_cast<unsigned>(candidate_x);
                if (row == y && column == x) {
                    continue;
                }
                if (bayer_colour(column, row) == target_colour) {
                    sum += row_sample(row, column, y);
                    ++samples;
                }
            }
        }
        return samples == 0
                   ? centre
                   : static_cast<uint16_t>(sum / samples);
    }

    RgbToken make_pixel_token(unsigned x, unsigned y) const
    {
        RgbToken token;
        token.href = true;
        token.vsync = false;
        const uint16_t centre = current_row_[x] & MAX_VAL;
        if (!enable.read()) {
            token.r = centre;
            token.g = centre;
            token.b = centre;
            return token;
        }
        token.r = interpolate_colour(x, y, 0U, centre);
        token.g = interpolate_colour(x, y, 1U, centre);
        token.b = interpolate_colour(x, y, 2U, centre);
        return token;
    }

    void shift_pipeline(const RgbToken& input)
    {
        for (unsigned index = DLY_CLK - 1; index > 0; --index) {
            token_delay_[index] = token_delay_[index - 1U];
            m_raw_delay[index] = m_raw_delay[index - 1U];
        }
        token_delay_[0] = input;
        const RgbToken& output = token_delay_[DLY_CLK - 1U];
        o_href.write(output.href);
        o_vsync.write(output.vsync);
        o_r.write(output.href ? output.r : 0);
        o_g.write(output.href ? output.g : 0);
        o_b.write(output.href ? output.b : 0);
        if (output.href) {
            ++m_pixels_output;
            m_metrics.record_pixel();
            m_metrics.record_r_pixel();
            m_metrics.record_g_pixel();
            m_metrics.record_b_pixel();
            m_metrics.record_active_cycle();
        }
    }

    void process_thread()
    {
        clear_outputs();
        while (true) {
            wait();
            if (!rst_n.read()) {
                reset_state();
                clear_outputs();
                continue;
            }

            const bool href = i_href.read();
            const bool vsync = i_vsync.read();
            const bool vsync_fall = previous_vsync_ && !vsync;
            const bool vsync_rise = !previous_vsync_ && vsync;
            const bool href_rise = !previous_href_ && href;
            const bool href_fall = previous_href_ && !href;

            if (vsync_fall) {
                ++frame_count_;
                line_count_ = 0;
                pixel_count_ = 0;
                in_frame_ = true;
                m_pixels_output = 0;
                std::fill(previous_row_.begin(),
                          previous_row_.end(), 0);
                std::fill(current_row_.begin(),
                          current_row_.end(), 0);
                m_metrics.record_frame();
            }

            if (href_rise && in_frame_) {
                if (previous_href_) {
                    ++line_count_;
                } else if (pixel_count_ != 0) {
                    ++line_count_;
                }
                pixel_count_ = 0;
                if (image_width_ == 0) {
                    current_row_.clear();
                } else {
                    current_row_.assign(image_width_, 0);
                }
                m_metrics.record_line();
            }

            RgbToken token;
            token.vsync = vsync;
            if (href && in_frame_) {
                const uint16_t raw = i_raw.read() & MAX_VAL;
                if (image_width_ == 0) {
                    current_row_.push_back(raw);
                } else if (pixel_count_ < image_width_) {
                    current_row_[pixel_count_] = raw;
                }
                m_raw_delay[0] = raw;
                if (pixel_count_ > 0 &&
                    pixel_count_ - 1U < current_row_.size()) {
                    token = make_pixel_token(
                        pixel_count_ - 1U, line_count_);
                }
                ++pixel_count_;
            } else if (href_fall && in_frame_ &&
                       pixel_count_ != 0 &&
                       !current_row_.empty()) {
                const unsigned last =
                    std::min<unsigned>(
                        pixel_count_,
                        static_cast<unsigned>(current_row_.size())) -
                    1U;
                token = make_pixel_token(last, line_count_);
                previous_row_ = current_row_;
            }

            if (vsync_rise) {
                in_frame_ = false;
            }

            m_metrics.record_total_cycle();
            shift_pipeline(token);
            previous_vsync_ = vsync;
            previous_href_ = href;
        }
    }

    void reset_handler()
    {
        wait();
        reset_state();
        m_metrics.reset();
    }
};

using isp_demosaic_10b_rggb =
    isp_demosaic<10, BayerPattern::RGGB>;
using isp_demosaic_10b_grbg =
    isp_demosaic<10, BayerPattern::GRBG>;
using isp_demosaic_10b_gbrg =
    isp_demosaic<10, BayerPattern::GBRG>;
using isp_demosaic_10b_bggr =
    isp_demosaic<10, BayerPattern::BGGR>;

#endif  // ISP_DEMOSAIC_H
