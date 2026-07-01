#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "encoder_defs.h"

namespace cdc::components {

// -----------------------------------------------------------------------------
// Frame container for H.265 TLM model.
//
// Current model:
// - 8-bit YUV 4:2:0 frame
// - luma plane:       width x height
// - chroma planes:    ceil(width/2) x ceil(height/2)
//
// Most blocks can start with luma only.
// Chroma is included so the frame structure is aligned with H.265 Main Profile.
// -----------------------------------------------------------------------------

struct frame {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    chroma_format format = chroma_format::yuv420;

    std::vector<std::uint8_t> luma;
    std::vector<std::uint8_t> chroma_cb;
    std::vector<std::uint8_t> chroma_cr;

    frame() = default;

    frame(std::uint32_t width_,
          std::uint32_t height_,
          chroma_format format_ = chroma_format::yuv420);

    void resize(std::uint32_t width_,
                std::uint32_t height_,
                chroma_format format_ = chroma_format::yuv420);

    bool empty() const;

    std::size_t pixel_count() const;
    std::size_t luma_size() const;
    std::size_t chroma_width() const;
    std::size_t chroma_height() const;
    std::size_t chroma_size() const;

    bool in_luma_bounds(std::uint32_t x, std::uint32_t y) const;
    bool in_chroma_bounds(std::uint32_t x, std::uint32_t y) const;

    std::uint8_t get_luma(std::uint32_t x, std::uint32_t y) const;
    void set_luma(std::uint32_t x, std::uint32_t y, std::uint8_t value);

    std::uint8_t get_cb(std::uint32_t x, std::uint32_t y) const;
    void set_cb(std::uint32_t x, std::uint32_t y, std::uint8_t value);

    std::uint8_t get_cr(std::uint32_t x, std::uint32_t y) const;
    void set_cr(std::uint32_t x, std::uint32_t y, std::uint8_t value);

    void fill_luma(std::uint8_t value);
    void fill_chroma(std::uint8_t cb_value, std::uint8_t cr_value);
    void fill(std::uint8_t y_value, std::uint8_t cb_value, std::uint8_t cr_value);
};

} // namespace cdc::components
