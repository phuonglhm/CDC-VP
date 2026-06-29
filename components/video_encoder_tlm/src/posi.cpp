#include "posi.h"

namespace cdc::components {

prediction_result posi::run(const frame& input, const block& region) const
{
    const std::uint32_t center = region.x + region.y + region.size;
    const std::uint32_t extent = input.width > input.height ? input.width : input.height;
    return {prediction_mode::intra, center + extent};
}

} // namespace cdc::components
