#ifndef ISP_SYSTEMC_VIP_VIP_H
#define ISP_SYSTEMC_VIP_VIP_H

#include <systemc>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

// Single ISP video-output pipeline (VIP).
//
// The pin interface has one clock of latency. Y is valid for every active
// pixel. Cb/Cr are valid only for the upper-left pixel of each 2x2 block
// (even x, even y); the other active-pixel Cb/Cr outputs are zero.
//
// The buffer interfaces either convert interleaved RGB or pack interleaved
// YUV444 into planar I420 in Y, Cb, Cr order. Both deliberately use upper-left
// chroma decimation, rather than averaging, to match the approved
// architecture-exploration behavior.
template <unsigned BITS = 10>
class VIP : public sc_core::sc_module {
public:
    SC_HAS_PROCESS(VIP);

    static_assert(BITS > 0 && BITS <= 16,
                  "VIP supports RGB sample widths from 1 through 16 bits");

    static constexpr std::uint8_t CSC_BT709 = 1;
    static constexpr std::uint8_t CSC_BT601 = 2;

    struct YuvSample {
        std::uint8_t y{0};
        std::uint8_t cb{0};
        std::uint8_t cr{0};
    };

    sc_core::sc_in<bool> pclk{"pclk"};
    sc_core::sc_in<bool> rst_n{"rst_n"};

    sc_core::sc_in<bool> i_href{"i_href"};
    sc_core::sc_in<bool> i_vsync{"i_vsync"};
    sc_core::sc_in<sc_dt::sc_uint<BITS>> i_r{"i_r"};
    sc_core::sc_in<sc_dt::sc_uint<BITS>> i_g{"i_g"};
    sc_core::sc_in<sc_dt::sc_uint<BITS>> i_b{"i_b"};
    sc_core::sc_in<sc_dt::sc_uint<2>> i_csc_standard{"i_csc_standard"};
    sc_core::sc_in<bool> i_input_is_yuv{"i_input_is_yuv"};
    sc_core::sc_in<bool> i_enable{"i_enable"};

    sc_core::sc_out<bool> o_href{"o_href"};
    sc_core::sc_out<bool> o_vsync{"o_vsync"};
    sc_core::sc_out<std::uint8_t> o_y{"o_y"};
    sc_core::sc_out<std::uint8_t> o_u{"o_u"};
    sc_core::sc_out<std::uint8_t> o_v{"o_v"};
    sc_core::sc_out<bool> o_error{"o_error"};

    explicit VIP(const sc_core::sc_module_name& name)
        : sc_core::sc_module(name) {
        SC_METHOD(tick);
        sensitive << pclk.pos() << rst_n.neg();
        dont_initialize();
    }

    static constexpr bool is_supported_standard(std::uint8_t standard) {
        return standard == CSC_BT709 || standard == CSC_BT601;
    }

    // Convert already normalized, full-range 8-bit RGB to full-range YCbCr.
    // Returns false and clears the output for reserved standards 0 and 3.
    static bool convert_normalized(std::uint8_t r,
                                   std::uint8_t g,
                                   std::uint8_t b,
                                   std::uint8_t standard,
                                   YuvSample& output) noexcept {
        output = {};

        int y_sum = 0;
        int cb_sum = 0;
        int cr_sum = 0;
        if (standard == CSC_BT601) {
            y_sum = 77 * r + 150 * g + 29 * b;
            cb_sum = -43 * r - 85 * g + 128 * b;
            cr_sum = 128 * r - 107 * g - 21 * b;
        } else if (standard == CSC_BT709) {
            y_sum = 54 * r + 183 * g + 18 * b;
            cb_sum = -29 * r - 99 * g + 128 * b;
            cr_sum = 128 * r - 116 * g - 12 * b;
        } else {
            return false;
        }

        output.y = clamp_u8(round_div_256(y_sum));
        output.cb = clamp_u8(round_div_256(cb_sum) + 128);
        output.cr = clamp_u8(round_div_256(cr_sum) + 128);
        return true;
    }

    // Convert one BITS-wide RGB sample. Values outside the declared BITS
    // range, and reserved CSC standards, are rejected.
    static bool convert_bits(std::uint16_t r,
                             std::uint16_t g,
                             std::uint16_t b,
                             std::uint8_t standard,
                             YuvSample& output) noexcept {
        if (r > SAMPLE_MAX || g > SAMPLE_MAX || b > SAMPLE_MAX) {
            output = {};
            return false;
        }
        return convert_normalized(normalize(r), normalize(g), normalize(b),
                                  standard, output);
    }

    // Convert interleaved BITS-wide RGB into explicitly described I420
    // planes. rgb_stride_samples and rgb_capacity_samples are measured in
    // uint16_t samples; output strides/capacities are measured in bytes.
    // Dimensions must be non-zero and even. Validation completes before any
    // output byte is written.
    static bool convert_i420(
        const std::uint16_t* rgb,
        std::size_t rgb_capacity_samples,
        std::size_t rgb_stride_samples,
        unsigned width,
        unsigned height,
        std::uint8_t* y_plane,
        std::size_t y_capacity_bytes,
        std::size_t y_stride_bytes,
        std::uint8_t* u_plane,
        std::size_t u_capacity_bytes,
        std::size_t u_stride_bytes,
        std::uint8_t* v_plane,
        std::size_t v_capacity_bytes,
        std::size_t v_stride_bytes,
        std::uint8_t standard = CSC_BT601,
        std::string* error = nullptr) {
        if (error != nullptr) {
            error->clear();
        }
        if (!is_supported_standard(standard)) {
            return fail(error, "CSC standard 0/3 is reserved; use 1 or 2");
        }
        if (width == 0 || height == 0 || (width & 1U) != 0 ||
            (height & 1U) != 0) {
            return fail(error, "I420 width and height must be non-zero and even");
        }
        if (rgb == nullptr || y_plane == nullptr || u_plane == nullptr ||
            v_plane == nullptr) {
            return fail(error, "RGB and all I420 plane pointers must be non-null");
        }

        const std::size_t width_sz = width;
        const std::size_t height_sz = height;
        if (width_sz > std::numeric_limits<std::size_t>::max() / 3U) {
            return fail(error, "RGB row size overflows size_t");
        }

        const std::size_t rgb_row_samples = width_sz * 3U;
        const std::size_t chroma_width = width_sz / 2U;
        const std::size_t chroma_height = height_sz / 2U;
        std::size_t rgb_required = 0;
        std::size_t y_required = 0;
        std::size_t u_required = 0;
        std::size_t v_required = 0;
        if (!required_span(height_sz, rgb_stride_samples, rgb_row_samples,
                           rgb_required) ||
            rgb_capacity_samples < rgb_required) {
            return fail(error, "RGB stride/capacity is smaller than the active frame");
        }
        if (!required_span(height_sz, y_stride_bytes, width_sz, y_required) ||
            y_capacity_bytes < y_required) {
            return fail(error, "Y stride/capacity is smaller than the active plane");
        }
        if (!required_span(chroma_height, u_stride_bytes, chroma_width,
                           u_required) ||
            u_capacity_bytes < u_required) {
            return fail(error, "U stride/capacity is smaller than the active plane");
        }
        if (!required_span(chroma_height, v_stride_bytes, chroma_width,
                           v_required) ||
            v_capacity_bytes < v_required) {
            return fail(error, "V stride/capacity is smaller than the active plane");
        }

        // Reject bad BITS-wide samples before modifying any output plane.
        for (std::size_t row = 0; row < height_sz; ++row) {
            const std::uint16_t* input_row = rgb + row * rgb_stride_samples;
            for (std::size_t column = 0; column < rgb_row_samples; ++column) {
                if (input_row[column] > SAMPLE_MAX) {
                    return fail(error, "RGB input contains a value wider than BITS");
                }
            }
        }

        for (std::size_t row = 0; row < height_sz; ++row) {
            const std::uint16_t* input_row = rgb + row * rgb_stride_samples;
            std::uint8_t* output_y = y_plane + row * y_stride_bytes;
            for (std::size_t column = 0; column < width_sz; ++column) {
                const std::size_t rgb_index = column * 3U;
                YuvSample sample{};
                (void)convert_bits(input_row[rgb_index],
                                   input_row[rgb_index + 1U],
                                   input_row[rgb_index + 2U], standard, sample);
                output_y[column] = sample.y;
                if ((row & 1U) == 0U && (column & 1U) == 0U) {
                    const std::size_t chroma_row = row / 2U;
                    const std::size_t chroma_column = column / 2U;
                    u_plane[chroma_row * u_stride_bytes + chroma_column] = sample.cb;
                    v_plane[chroma_row * v_stride_bytes + chroma_column] = sample.cr;
                }
            }
        }
        return true;
    }

    // Pack interleaved 8-bit YUV444 into explicitly described I420 planes.
    // yuv444_stride_bytes and yuv444_capacity_bytes are measured in bytes.
    // Chroma is selected from the upper-left sample of each 2x2 block.
    // Dimensions must be non-zero and even. Validation completes before any
    // output byte is written.
    static bool pack_yuv444_i420(
        const std::uint8_t* yuv444,
        std::size_t yuv444_capacity_bytes,
        std::size_t yuv444_stride_bytes,
        unsigned width,
        unsigned height,
        std::uint8_t* y_plane,
        std::size_t y_capacity_bytes,
        std::size_t y_stride_bytes,
        std::uint8_t* u_plane,
        std::size_t u_capacity_bytes,
        std::size_t u_stride_bytes,
        std::uint8_t* v_plane,
        std::size_t v_capacity_bytes,
        std::size_t v_stride_bytes,
        std::string* error = nullptr) {
        if (error != nullptr) {
            error->clear();
        }
        if (width == 0 || height == 0 || (width & 1U) != 0 ||
            (height & 1U) != 0) {
            return fail(error, "I420 width and height must be non-zero and even");
        }
        if (yuv444 == nullptr || y_plane == nullptr || u_plane == nullptr ||
            v_plane == nullptr) {
            return fail(error,
                        "YUV444 and all I420 plane pointers must be non-null");
        }

        const std::size_t width_sz = width;
        const std::size_t height_sz = height;
        if (width_sz > std::numeric_limits<std::size_t>::max() / 3U) {
            return fail(error, "YUV444 row size overflows size_t");
        }

        const std::size_t yuv444_row_bytes = width_sz * 3U;
        const std::size_t chroma_width = width_sz / 2U;
        const std::size_t chroma_height = height_sz / 2U;
        std::size_t yuv444_required = 0;
        std::size_t y_required = 0;
        std::size_t u_required = 0;
        std::size_t v_required = 0;
        if (!required_span(height_sz, yuv444_stride_bytes, yuv444_row_bytes,
                           yuv444_required) ||
            yuv444_capacity_bytes < yuv444_required) {
            return fail(error,
                        "YUV444 stride/capacity is smaller than the active frame");
        }
        if (!required_span(height_sz, y_stride_bytes, width_sz, y_required) ||
            y_capacity_bytes < y_required) {
            return fail(error, "Y stride/capacity is smaller than the active plane");
        }
        if (!required_span(chroma_height, u_stride_bytes, chroma_width,
                           u_required) ||
            u_capacity_bytes < u_required) {
            return fail(error, "U stride/capacity is smaller than the active plane");
        }
        if (!required_span(chroma_height, v_stride_bytes, chroma_width,
                           v_required) ||
            v_capacity_bytes < v_required) {
            return fail(error, "V stride/capacity is smaller than the active plane");
        }

        for (std::size_t row = 0; row < height_sz; ++row) {
            const std::uint8_t* input_row =
                yuv444 + row * yuv444_stride_bytes;
            std::uint8_t* output_y = y_plane + row * y_stride_bytes;
            for (std::size_t column = 0; column < width_sz; ++column) {
                const std::size_t yuv_index = column * 3U;
                output_y[column] = input_row[yuv_index];
                if ((row & 1U) == 0U && (column & 1U) == 0U) {
                    const std::size_t chroma_row = row / 2U;
                    const std::size_t chroma_column = column / 2U;
                    u_plane[chroma_row * u_stride_bytes + chroma_column] =
                        input_row[yuv_index + 1U];
                    v_plane[chroma_row * v_stride_bytes + chroma_column] =
                        input_row[yuv_index + 2U];
                }
            }
        }
        return true;
    }

    // Convenience form for tightly packed I420. The output layout is exactly:
    // width*height Y bytes, then width*height/4 U bytes, then the same number
    // of V bytes.
    static bool convert_i420_contiguous(
        const std::uint16_t* rgb,
        std::size_t rgb_capacity_samples,
        std::size_t rgb_stride_samples,
        unsigned width,
        unsigned height,
        std::uint8_t* i420,
        std::size_t i420_capacity_bytes,
        std::uint8_t standard = CSC_BT601,
        std::string* error = nullptr) {
        if (error != nullptr) {
            error->clear();
        }
        if (width == 0 || height == 0 || (width & 1U) != 0 ||
            (height & 1U) != 0) {
            return fail(error, "I420 width and height must be non-zero and even");
        }
        if (i420 == nullptr) {
            return fail(error, "I420 output pointer must be non-null");
        }

        const std::size_t width_sz = width;
        const std::size_t height_sz = height;
        if (height_sz > std::numeric_limits<std::size_t>::max() / width_sz) {
            return fail(error, "I420 frame size overflows size_t");
        }
        const std::size_t y_size = width_sz * height_sz;
        const std::size_t chroma_size = y_size / 4U;
        if (chroma_size >
            (std::numeric_limits<std::size_t>::max() - y_size) / 2U) {
            return fail(error, "I420 frame size overflows size_t");
        }
        const std::size_t required = y_size + 2U * chroma_size;
        if (i420_capacity_bytes < required) {
            return fail(error, "I420 output capacity is smaller than width*height*3/2");
        }

        return convert_i420(
            rgb, rgb_capacity_samples, rgb_stride_samples, width, height,
            i420, y_size, width_sz,
            i420 + y_size, chroma_size, width_sz / 2U,
            i420 + y_size + chroma_size, chroma_size, width_sz / 2U,
            standard, error);
    }

private:
    static constexpr std::uint32_t SAMPLE_MAX =
        (std::uint32_t{1} << BITS) - std::uint32_t{1};

    unsigned x_{0};
    unsigned y_{0};
    bool previous_href_{false};

    static constexpr std::uint8_t clamp_u8(int value) noexcept {
        return value < 0 ? std::uint8_t{0}
                         : (value > 255 ? std::uint8_t{255}
                                        : static_cast<std::uint8_t>(value));
    }

    // Symmetric round-to-nearest, with exact half values rounded away from 0.
    static constexpr int round_div_256(int value) noexcept {
        return value >= 0 ? (value + 128) / 256
                          : -((-value + 128) / 256);
    }

    static constexpr std::uint8_t normalize(std::uint16_t value) noexcept {
        return static_cast<std::uint8_t>(
            (static_cast<std::uint32_t>(value) * 255U + SAMPLE_MAX / 2U) /
            SAMPLE_MAX);
    }

    static bool required_span(std::size_t rows,
                              std::size_t stride,
                              std::size_t active_row,
                              std::size_t& span) noexcept {
        if (rows == 0 || active_row == 0 || stride < active_row) {
            return false;
        }
        const std::size_t preceding_rows = rows - 1U;
        if (preceding_rows != 0U &&
            stride > (std::numeric_limits<std::size_t>::max() - active_row) /
                         preceding_rows) {
            return false;
        }
        span = preceding_rows * stride + active_row;
        return true;
    }

    static bool fail(std::string* error, const char* message) {
        if (error != nullptr) {
            *error = message;
        }
        return false;
    }

    void clear_outputs() {
        o_href.write(false);
        o_vsync.write(false);
        o_y.write(0);
        o_u.write(0);
        o_v.write(0);
        o_error.write(false);
    }

    void tick() {
        if (!rst_n.read()) {
            x_ = 0;
            y_ = 0;
            previous_href_ = false;
            clear_outputs();
            return;
        }

        const bool href = i_href.read();
        const bool vsync = i_vsync.read();
        const bool enabled = i_enable.read();
        o_href.write(enabled && href);
        o_vsync.write(vsync);
        o_y.write(0);
        o_u.write(0);
        o_v.write(0);
        o_error.write(false);

        if (vsync) {
            x_ = 0;
            y_ = 0;
        } else if (href) {
            if (!previous_href_) {
                x_ = 0;
            }
            if (enabled) {
                YuvSample sample{};
                bool valid = true;
                if (i_input_is_yuv.read()) {
                    sample.y = static_cast<std::uint8_t>(
                        i_r.read().to_uint() & 0xFFU);
                    sample.cb = static_cast<std::uint8_t>(
                        i_g.read().to_uint() & 0xFFU);
                    sample.cr = static_cast<std::uint8_t>(
                        i_b.read().to_uint() & 0xFFU);
                } else {
                    valid = convert_bits(
                        static_cast<std::uint16_t>(i_r.read().to_uint()),
                        static_cast<std::uint16_t>(i_g.read().to_uint()),
                        static_cast<std::uint16_t>(i_b.read().to_uint()),
                        static_cast<std::uint8_t>(
                            i_csc_standard.read().to_uint()),
                        sample);
                }
                if (valid) {
                    o_y.write(sample.y);
                    if ((x_ & 1U) == 0U && (y_ & 1U) == 0U) {
                        o_u.write(sample.cb);
                        o_v.write(sample.cr);
                    }
                } else {
                    o_error.write(true);
                }
            }
            ++x_;
        } else if (previous_href_) {
            x_ = 0;
            ++y_;
        }

        previous_href_ = href;
    }
};

#endif  // ISP_SYSTEMC_VIP_VIP_H
