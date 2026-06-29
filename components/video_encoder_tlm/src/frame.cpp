#include "frame.h"

namespace cdc::components {

frame::frame(std::uint32_t width_value, std::uint32_t height_value)
    : width(width_value)
    , height(height_value)
    , luma(static_cast<std::size_t>(width_value) * static_cast<std::size_t>(height_value), 0)
{
}

std::size_t frame::pixel_count() const
{
    return luma.size();
}

} // namespace cdc::components
