#include "prei.h"

#include "encoder_defs.h"

namespace cdc::components {

prediction_result prei::run(const frame& input, const block& region) const
{
    const std::uint32_t area = region.area();
    const std::uint32_t frame_bias = input.empty() ? 0u : ((input.width + input.height) & 0xFFu);
    const std::uint32_t cost = area + frame_bias;

    return prediction_result::make_intra(
        cost,
        intra_prediction_mode::dc,
        partition_mode::part_2nx2n,
        INIT_QP
    );
}

} // namespace cdc::components
