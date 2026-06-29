#include "ime.h"

#include "encoder_defs.h"

namespace cdc::components {

prediction_result ime::run(const frame& input, const block& region) const
{
    const std::uint32_t motion_span = SEARCH_RANGE;
    const std::uint32_t frame_span = input.empty() ? 0u : ((input.width + input.height) & 0xFFu);
    const std::uint32_t cost = motion_span + frame_span + region.area();

    return prediction_result::make_inter(
        cost,
        motion_vector{0, 0},
        partition_mode::part_2nx2n,
        INIT_QP
    );
}

} // namespace cdc::components
