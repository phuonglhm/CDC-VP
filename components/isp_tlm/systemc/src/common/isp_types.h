/*
 * ISP Types and Data Structures
 * Matched with Infinite-ISP RTL
 */

#ifndef ISP_TYPES_H
#define ISP_TYPES_H

#include <systemc>
#include <tlm>
#include <cstdint>
#include <array>

//=============================================================================
// Bayer Pattern Definitions
//=============================================================================
enum class BayerPattern : uint8_t {
    RGGB = 0,  // Standard RGGB
    GRBG = 1,  // GRBG
    GBRG = 2,  // GBRG
    BGGR = 3   // BGGR
};

//=============================================================================
// Pixel Data Types
//=============================================================================
template<unsigned int BITS>
struct pixel_t {
    uint16_t value;

    pixel_t() : value(0) {}
    explicit pixel_t(uint16_t v) : value(v) {}

    operator uint16_t() const { return value; }
    pixel_t& operator=(uint16_t v) { value = v; return *this; }
    pixel_t& operator=(const pixel_t& p) { value = p.value; return *this; }

    // Arithmetic operators
    pixel_t operator+(const pixel_t& p) const { return pixel_t(value + p.value); }
    pixel_t operator-(const pixel_t& p) const { return pixel_t(value - p.value); }
    pixel_t operator*(int multiplier) const { return pixel_t(value * multiplier); }
    pixel_t operator/(int divisor) const { return pixel_t(value / divisor); }
};

// Specialization for common bit widths
using pixel_10b_t = pixel_t<10>;
using pixel_8b_t = pixel_t<8>;

//=============================================================================
// RGB Pixel (after demosaic)
//=============================================================================
template<unsigned int BITS>
struct rgb_pixel_t {
    pixel_t<BITS> r;
    pixel_t<BITS> g;
    pixel_t<BITS> b;

    rgb_pixel_t() : r(0), g(0), b(0) {}
    rgb_pixel_t(pixel_t<BITS> _r, pixel_t<BITS> _g, pixel_t<BITS> _b)
        : r(_r), g(_g), b(_b) {}
};

//=============================================================================
// YUV Pixel (after CSC)
//=============================================================================
template<unsigned int BITS>
struct yuv_pixel_t {
    pixel_t<BITS> y;
    pixel_t<BITS> u;
    pixel_t<BITS> v;

    yuv_pixel_t() : y(0), u(0), v(0) {}
    yuv_pixel_t(pixel_t<BITS> _y, pixel_t<BITS> _u, pixel_t<BITS> _v)
        : y(_y), u(_u), v(_v) {}
};

//=============================================================================
// Video Timing Signals
//=============================================================================
struct video_if {
    sc_core::sc_signal<bool> href;      // Horizontal valid
    sc_core::sc_signal<bool> vsync;      // Vertical sync
    sc_core::sc_signal<bool> href_deassert;  // For edge detection

    video_if(const char* name)
        : href((std::string(name) + "_href").c_str())
        , vsync((std::string(name) + "_vsync").c_str())
        , href_deassert((std::string(name) + "_href_d").c_str())
    {}
};

//=============================================================================
// ISP Configuration Structures
//=============================================================================

// Black Level Correction
struct BlcConfig {
    uint16_t r;
    uint16_t gr;
    uint16_t gb;
    uint16_t b;
    bool linear_en;
    uint16_t linear_r;
    uint16_t linear_gr;
    uint16_t linear_gb;
    uint16_t linear_b;
};

// Digital Gain
struct DgainConfig {
    bool is_manual;
    uint8_t man_index;
    std::array<uint8_t, 100> gain_array;  // 100 entries
};

// White Balance
struct WbConfig {
    uint16_t r_gain;
    uint16_t b_gain;
};

// Color Correction Matrix
struct CcmConfig {
    uint16_t rr, rg, rb;
    uint16_t gr, gg, gb;
    uint16_t br, bg, bb;
};

// BNR Configuration
struct BnrConfig {
    std::array<int16_t, 25> space_kernel_r;
    std::array<int16_t, 25> space_kernel_g;
    std::array<int16_t, 25> space_kernel_b;
    std::array<uint16_t, 9> color_curve_x_r;
    std::array<int16_t, 9>  color_curve_y_r;
    std::array<uint16_t, 9> color_curve_x_g;
    std::array<int16_t, 9>  color_curve_y_g;
    std::array<uint16_t, 9> color_curve_x_b;
    std::array<int16_t, 9>  color_curve_y_b;
};

// Sharpening
struct SharpenConfig {
    int16_t luma_kernel[9][9];  // 9x9 kernel
    uint16_t strength;
};

// 2DNR Configuration
struct Nr2dConfig {
    std::array<uint8_t, 32> diff_lut;
    std::array<uint8_t, 32> weight_lut;
};

// AWB Statistics
struct AwbStats {
    uint16_t underexposed_limit;
    uint16_t overexposed_limit;
    uint8_t frames;
};

// AE Configuration
struct AeConfig {
    uint8_t center_illuminance;
    uint16_t skewness;
    uint16_t crop_left;
    uint16_t crop_right;
    uint16_t crop_top;
    uint16_t crop_bottom;
};

// CSC Standard
enum class CscStandard : uint8_t {
    BT_601 = 0,
    BT_709 = 1,
    BT_2020 = 2
};

// Crop Window
struct CropWindow {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
};

//=============================================================================
// Utility Functions
//=============================================================================

// Clamp value to bit range
template<unsigned int BITS>
inline uint16_t clamp_to_bits(uint32_t val) {
    constexpr uint16_t MAX = (1 << BITS) - 1;
    if (val > MAX) return MAX;
    return static_cast<uint16_t>(val);
}

// Absolute difference
template<typename T>
inline T abs_diff(T a, T b) {
    return (a > b) ? (a - b) : (b - a);
}

// Min of 3 values
template<typename T>
inline T min3(T a, T b, T c) {
    return std::min(std::min(a, b), c);
}

// Max of 3 values
template<typename T>
inline T max3(T a, T b, T c) {
    return std::max(std::max(a, b), c);
}

// Median of 3 values
template<typename T>
inline T med3(T a, T b, T c) {
    if ((a <= b) == (a >= c)) return a;
    if ((b <= a) == (b >= c)) return b;
    return c;
}

#endif // ISP_TYPES_H
