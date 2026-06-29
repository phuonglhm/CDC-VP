#include "frame.h"

#include <algorithm>

namespace cdc::components {

namespace {

std::size_t ceil_div2(std::uint32_t value)
{
    return static_cast<std::size_t>((value + 1u) / 2u);
}

} // namespace

frame::frame(std::uint32_t width_,
             std::uint32_t height_,
             chroma_format format_)
{
    resize(width_, height_, format_);
}

void frame::resize(std::uint32_t width_,
                   std::uint32_t height_,
                   chroma_format format_)
{
    width = width_;
    height = height_;
    format = format_;

    luma.assign(luma_size(), 0);

    if (format == chroma_format::yuv420) {
        chroma_cb.assign(chroma_size(), 128);
        chroma_cr.assign(chroma_size(), 128);
    } else {
        chroma_cb.clear();
        chroma_cr.clear();
    }
}

bool frame::empty() const
{
    return width == 0 || height == 0 || luma.empty();
}

std::size_t frame::pixel_count() const
{
    return luma_size();
}

std::size_t frame::luma_size() const
{
    return static_cast<std::size_t>(width) * height;
}

std::size_t frame::chroma_width() const
{
    if (format == chroma_format::yuv420) {
        return ceil_div2(width);
    }

    return 0;
}

std::size_t frame::chroma_height() const
{
    if (format == chroma_format::yuv420) {
        return ceil_div2(height);
    }

    return 0;
}

std::size_t frame::chroma_size() const
{
    return chroma_width() * chroma_height();
}

bool frame::in_luma_bounds(std::uint32_t x, std::uint32_t y) const
{
    return x < width && y < height;
}

bool frame::in_chroma_bounds(std::uint32_t x, std::uint32_t y) const
{
    return x < chroma_width() && y < chroma_height();
}

std::uint8_t frame::get_luma(std::uint32_t x, std::uint32_t y) const
{
    if (!in_luma_bounds(x, y)) {
        return 0;
    }

    const std::size_t index = static_cast<std::size_t>(y) * width + x;
    return luma[index];
}

void frame::set_luma(std::uint32_t x, std::uint32_t y, std::uint8_t value)
{
    if (!in_luma_bounds(x, y)) {
        return;
    }

    const std::size_t index = static_cast<std::size_t>(y) * width + x;
    luma[index] = value;
}

std::uint8_t frame::get_cb(std::uint32_t x, std::uint32_t y) const
{
    if (!in_chroma_bounds(x, y)) {
        return 128;
    }

    const std::size_t index = static_cast<std::size_t>(y) * chroma_width() + x;
    return chroma_cb[index];
}

void frame::set_cb(std::uint32_t x, std::uint32_t y, std::uint8_t value)
{
    if (!in_chroma_bounds(x, y)) {
        return;
    }

    const std::size_t index = static_cast<std::size_t>(y) * chroma_width() + x;
    chroma_cb[index] = value;
}

std::uint8_t frame::get_cr(std::uint32_t x, std::uint32_t y) const
{
    if (!in_chroma_bounds(x, y)) {
        return 128;
    }

    const std::size_t index = static_cast<std::size_t>(y) * chroma_width() + x;
    return chroma_cr[index];
}

void frame::set_cr(std::uint32_t x, std::uint32_t y, std::uint8_t value)
{
    if (!in_chroma_bounds(x, y)) {
        return;
    }

    const std::size_t index = static_cast<std::size_t>(y) * chroma_width() + x;
    chroma_cr[index] = value;
}

void frame::fill_luma(std::uint8_t value)
{
    std::fill(luma.begin(), luma.end(), value);
}

void frame::fill_chroma(std::uint8_t cb_value, std::uint8_t cr_value)
{
    std::fill(chroma_cb.begin(), chroma_cb.end(), cb_value);
    std::fill(chroma_cr.begin(), chroma_cr.end(), cr_value);
}

void frame::fill(std::uint8_t y_value, std::uint8_t cb_value, std::uint8_t cr_value)
{
    fill_luma(y_value);
    fill_chroma(cb_value, cr_value);
}

} // namespace cdc::components
