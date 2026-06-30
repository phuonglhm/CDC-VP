#include "cabac.h"

namespace cdc::components {

std::vector<std::uint8_t> cabac::encode(const frame& reconstructed,
                                        const prediction_result& result) const
{
    std::vector<std::uint8_t> bitstream;
    bitstream.reserve(6);
    bitstream.push_back(static_cast<std::uint8_t>(reconstructed.width & 0xffu));
    bitstream.push_back(static_cast<std::uint8_t>(reconstructed.height & 0xffu));
    bitstream.push_back(static_cast<std::uint8_t>(result.cost & 0xffu));
    bitstream.push_back(static_cast<std::uint8_t>((result.cost >> 8) & 0xffu));
    bitstream.push_back(static_cast<std::uint8_t>(result.mode == prediction_mode::inter ? 1u : 0u));
    bitstream.push_back(static_cast<std::uint8_t>(reconstructed.pixel_count() & 0xffu));
    return bitstream;
}

} // namespace cdc::components
