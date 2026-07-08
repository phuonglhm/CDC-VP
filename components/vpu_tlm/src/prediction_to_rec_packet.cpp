#include "prediction_to_rec_packet.h"

#include <algorithm>

namespace cdc::components {

namespace {

std::uint8_t clamp_u8(std::uint32_t value)
{
    return static_cast<std::uint8_t>(
        std::min<std::uint32_t>(value, static_cast<std::uint32_t>(0xffu)));
}

std::uint8_t encode_coord_u8(std::uint32_t value)
{
    return static_cast<std::uint8_t>(value & 0xffu);
}

std::uint32_t make_block_address(std::uint32_t block_tag,
                                 std::uint32_t block_x_4x4,
                                 std::uint32_t block_y_4x4)
{
    return ((block_tag & 0xffu) << 16u) |
           ((block_y_4x4 & 0xffu) << 8u) |
           (block_x_4x4 & 0xffu);
}

} // namespace

prediction_to_rec_result prediction_to_rec_packet::run(
    const block& region,
    const prediction_result& prediction) const
{
    if (!prediction.valid || region.width == 0 || region.height == 0) {
        return prediction_to_rec_result::invalid();
    }

    const std::uint32_t block_x_4x4 = region.x / 4u;
    const std::uint32_t block_y_4x4 = region.y / 4u;

    prediction_to_rec_result result;
    result.valid = true;

    result.packet.cmd = RecCmd::READ_REQ;
    result.packet.block_idx = clamp_u8(derive_block_index(region));
    result.packet.x = encode_coord_u8(block_x_4x4);
    result.packet.y = encode_coord_u8(block_y_4x4);
    result.packet.size = encode_block_size(region);
    result.packet.sel = 0;
    result.packet.qp = clamp_u8(prediction.qp);
    result.packet.pred_type =
        static_cast<std::uint8_t>(prediction.mode == prediction_mode::intra
                                      ? PredType::INTRA
                                      : PredType::MC);
    result.packet.mode = encode_intra_mode(prediction);
    result.packet.pre_sel = 0;
    result.packet.i4x4_x = 0;
    result.packet.i4x4_y = 0;
    result.packet.data.clear();

    result.mv_addr =
        make_block_address(result.packet.block_idx, block_x_4x4, block_y_4x4);

    if (prediction.mode == prediction_mode::inter) {
        result.has_mv = true;
        result.mv = MotionVector(prediction.mv.integer_x(),
                                 prediction.mv.integer_y(),
                                 false);
    }

    return result;
}

std::uint8_t prediction_to_rec_packet::encode_block_size(const block& region) const
{
    const std::uint32_t edge = std::min(region.width, region.height);

    if (edge <= 4u) {
        return 0;
    }
    if (edge <= 8u) {
        return 1;
    }
    if (edge <= 16u) {
        return 2;
    }
    return 3;
}

std::uint8_t prediction_to_rec_packet::encode_intra_mode(
    const prediction_result& prediction) const
{
    if (prediction.mode == prediction_mode::inter) {
        return 0;
    }

    switch (prediction.intra_mode) {
        case intra_prediction_mode::planar:
            return 0;
        case intra_prediction_mode::dc:
            return 1;
        case intra_prediction_mode::angular_2:
            return 2;
        case intra_prediction_mode::angular_10:
            return 10;
        case intra_prediction_mode::angular_18:
            return 18;
        case intra_prediction_mode::angular_26:
            return 26;
        case intra_prediction_mode::angular_34:
            return 34;
    }

    return 1;
}

std::uint32_t prediction_to_rec_packet::derive_block_index(const block& region) const
{
    const std::uint32_t bx = region.x / 4u;
    const std::uint32_t by = region.y / 4u;
    return (((by >> 8u) & 0x0fu) << 4u) |
           ((bx >> 8u) & 0x0fu);
}

} // namespace cdc::components
