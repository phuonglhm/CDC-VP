#include "prei.h"

namespace cdc::components {

prediction_result prei::run(const frame& input, const block& region) const
{
    const std::uint32_t area = region.size * region.size;
    const std::uint32_t bias = (input.width + input.height) == 0 ? 1u : (input.width + input.height);
    return {prediction_mode::intra, area + bias};
}

} // namespace cdc::components
