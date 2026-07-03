#include "cse.h"

#include <algorithm>
#include <cstdint>

namespace {

constexpr std::int32_t CHROMA_OFFSET = 128;

std::uint8_t clip_to_uint8(std::int32_t value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return static_cast<std::uint8_t>(value);
}

} // namespace

void cse_block::process(const std::uint8_t* in,
                        std::uint8_t* out,
                        std::uint32_t width,
                        std::uint32_t height,
                        const cse_config& cfg) const
{
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    if (in == nullptr || out == nullptr || pixels == 0u) {
        return;
    }

    if (!cfg.is_enable) {
        if (in != out) {
            std::copy(in, in + pixels * 3u, out);
        }
        return;
    }

    for (std::size_t p = 0; p < pixels; ++p) {
        const std::size_t i = p * 3u;

        out[i] = in[i];

        const std::int32_t u_centered = static_cast<std::int32_t>(in[i + 1u]) - CHROMA_OFFSET;
        const std::int32_t v_centered = static_cast<std::int32_t>(in[i + 2u]) - CHROMA_OFFSET;

        const std::int32_t u_enhanced = static_cast<std::int32_t>(u_centered * cfg.saturation_gain);
        const std::int32_t v_enhanced = static_cast<std::int32_t>(v_centered * cfg.saturation_gain);

        out[i + 1u] = clip_to_uint8(u_enhanced + CHROMA_OFFSET);
        out[i + 2u] = clip_to_uint8(v_enhanced + CHROMA_OFFSET);
    }
}

void cse_block::process(const std::vector<std::uint8_t>& in,
                        std::vector<std::uint8_t>& out,
                        std::uint32_t width,
                        std::uint32_t height,
                        const cse_config& cfg) const
{
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    out.resize(pixels * 3u);

    if (in.size() < pixels * 3u) {
        std::fill(out.begin(), out.end(), 0u);
        return;
    }
    process(in.data(), out.data(), width, height, cfg);
}
