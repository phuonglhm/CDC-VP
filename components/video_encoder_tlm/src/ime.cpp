#include "ime.h"

namespace cdc::components {

prediction_result ime::run(const frame& input, const block& region) const
{
    const std::uint32_t motion_span = region.x + region.y + region.size;
    const std::uint32_t frame_span = input.width + input.height + 1u;
    return {prediction_mode::inter, motion_span + frame_span};
}

} // namespace cdc::components
