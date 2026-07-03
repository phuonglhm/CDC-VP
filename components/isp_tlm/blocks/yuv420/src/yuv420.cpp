#include "yuv420.h"

#include <algorithm>
#include <cstdint>

std::size_t yuv420_block::get_yuv420_size(std::uint32_t width, std::uint32_t height) const
{
    return static_cast<std::size_t>(width) * height +
           2 * ((width + 1) / 2) * ((height + 1) / 2);
}

void yuv420_block::process(const std::uint8_t* in,
                           std::uint8_t* out,
                           std::uint32_t width,
                           std::uint32_t height,
                           const yuv420_config& cfg) const
{
    if (in == nullptr || out == nullptr) {
        return;
    }

    if (!cfg.is_enable) {
        const std::size_t size = static_cast<std::size_t>(width) * height * 3u;
        std::copy(in, in + size, out);
        return;
    }

    const std::uint32_t half_width = (width + 1) / 2;
    const std::uint32_t half_height = (height + 1) / 2;

    std::size_t out_offset = 0;

    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            out[out_offset++] = in[(y * width + x) * 3u];
        }
    }

    for (std::uint32_t y = 0; y < half_height; ++y) {
        for (std::uint32_t x = 0; x < half_width; ++x) {
            std::uint32_t src_y = y * 2;
            std::uint32_t src_x = x * 2;

            std::uint32_t sum_u = 0;
            std::uint32_t sum_v = 0;
            std::uint32_t count = 0;

            for (std::uint32_t dy = 0; dy < 2 && src_y + dy < height; ++dy) {
                for (std::uint32_t dx = 0; dx < 2 && src_x + dx < width; ++dx) {
                    std::size_t idx = ((src_y + dy) * width + (src_x + dx)) * 3u;
                    sum_u += in[idx + 1u];
                    sum_v += in[idx + 2u];
                    ++count;
                }
            }

            out[out_offset++] = static_cast<std::uint8_t>(sum_u / count);
            out[out_offset++] = static_cast<std::uint8_t>(sum_v / count);
        }
    }
}

void yuv420_block::process(const std::vector<std::uint8_t>& in,
                           std::vector<std::uint8_t>& out,
                           std::uint32_t width,
                           std::uint32_t height,
                           const yuv420_config& cfg) const
{
    const std::size_t out_size = get_yuv420_size(width, height);
    out.resize(out_size);

    const std::size_t required_in = static_cast<std::size_t>(width) * height * 3u;
    if (in.size() < required_in) {
        std::fill(out.begin(), out.end(), 0u);
        return;
    }
    process(in.data(), out.data(), width, height, cfg);
}
