#pragma once

#include <cstdint>

namespace cdc::components {

// -----------------------------------------------------------------------------
// H.265 / HEVC encoder common definitions
// TLM equivalent of xk265 rtl/enc_defines.v
// -----------------------------------------------------------------------------

constexpr std::uint32_t PIXEL_WIDTH = 8;
constexpr std::uint32_t MAX_PIXEL_VALUE = 255;

// HEVC Main Profile commonly uses YUV 4:2:0 8-bit.
enum class chroma_format : std::uint32_t {
    yuv400 = 0,
    yuv420 = 1,
    yuv422 = 2,
    yuv444 = 3,
};

// Coding tree structure
constexpr std::uint32_t CTU_SIZE = 64;
constexpr std::uint32_t LCU_SIZE = 64;

constexpr std::uint32_t MIN_CU_SIZE = 8;
constexpr std::uint32_t MAX_CU_SIZE = 64;

constexpr std::uint32_t MIN_TU_SIZE = 4;
constexpr std::uint32_t MAX_TU_SIZE = 32;

constexpr std::uint32_t TU_SIZE_4  = 4;
constexpr std::uint32_t TU_SIZE_8  = 8;
constexpr std::uint32_t TU_SIZE_16 = 16;
constexpr std::uint32_t TU_SIZE_32 = 32;

// xk265 feature: search range 64
constexpr std::uint32_t IME_SEARCH_RANGE = 64;
constexpr std::uint32_t SEARCH_RANGE = IME_SEARCH_RANGE;

// HEVC QP range
constexpr std::uint32_t INIT_QP = 22;
constexpr std::uint32_t MIN_QP = 0;
constexpr std::uint32_t MAX_QP = 51;

// HEVC intra prediction modes
constexpr std::uint32_t NUM_INTRA_MODES = 35;
constexpr std::uint32_t INTRA_PLANAR_MODE = 0;
constexpr std::uint32_t INTRA_DC_MODE = 1;
constexpr std::uint32_t INTRA_ANGULAR_START = 2;
constexpr std::uint32_t INTRA_ANGULAR_END = 34;

// Simplified cost widths for TLM model
constexpr std::uint32_t INTRA_COST_WIDTH = 20;
constexpr std::uint32_t INTER_COST_WIDTH = 20;
constexpr std::uint32_t IME_COST_WIDTH = 28;

// -----------------------------------------------------------------------------
// TLM register map
// -----------------------------------------------------------------------------

constexpr std::uint64_t REG_CONTROL     = 0x00;
constexpr std::uint64_t REG_STATUS      = 0x04;
constexpr std::uint64_t REG_OUTPUT_SIZE = 0x08;
constexpr std::uint64_t REG_FRAME_TYPE  = 0x0C;
constexpr std::uint64_t REG_QP          = 0x10;
constexpr std::uint64_t REG_WIDTH       = 0x14;
constexpr std::uint64_t REG_HEIGHT      = 0x18;

constexpr std::uint32_t CONTROL_START = 1u << 0;
constexpr std::uint32_t CONTROL_CLEAR = 1u << 1;

constexpr std::uint32_t STATUS_BUSY  = 1u << 0;
constexpr std::uint32_t STATUS_DONE  = 1u << 1;
constexpr std::uint32_t STATUS_ERROR = 1u << 2;

// -----------------------------------------------------------------------------
// Common encoder enums
// -----------------------------------------------------------------------------

enum class frame_type : std::uint32_t {
    intra = 0,
    inter = 1,
};

enum class slice_type : std::uint32_t {
    i_slice = 0,
    p_slice = 1,
};

enum class block_type : std::uint32_t {
    ctu = 0,
    cu  = 1,
    pu  = 2,
    tu  = 3,
};

} // namespace cdc::components
