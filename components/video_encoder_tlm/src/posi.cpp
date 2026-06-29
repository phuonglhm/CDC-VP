#include "posi.h"

#include "encoder_defs.h"

namespace cdc::components {

prediction_result posi::run(const frame& input, const block& region) const
{
    const std::uint32_t center_x = region.x + region.width / 2u;
    const std::uint32_t center_y = region.y + region.height / 2u;

    const std::uint32_t center =
        input.in_luma_bounds(center_x, center_y)
            ? input.get_luma(center_x, center_y)
            : 128u;

    const std::uint32_t cost = center + region.area();

    return prediction_result::make_intra(
        cost,
        intra_prediction_mode::dc,
        partition_mode::part_2nx2n,
        INIT_QP
    );
}

} // namespace cdc::components
