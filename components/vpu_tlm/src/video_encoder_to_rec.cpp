#include "video_encoder_to_rec.h"

namespace cdc::components {

bool video_encoder_to_rec::write_mv_if_needed(
    const prediction_to_rec_result& request,
    RecMvIf& mv_mem) const
{
    if (!request.valid) {
        return false;
    }

    if (!request.has_mv) {
        return true;
    }

    return mv_mem.writeMV(request.mv_addr, request.mv);
}

std::vector<std::uint8_t> video_encoder_to_rec::pack_request(
    const prediction_to_rec_result& request) const
{
    if (!request.valid) {
        return {};
    }

    return packRecPacket(request.packet);
}

bool video_encoder_to_rec::targets_mc(
    const prediction_to_rec_result& request) const
{
    if (!request.valid) {
        return false;
    }

    return request.packet.pred_type == static_cast<std::uint8_t>(PredType::MC);
}

void video_encoder_to_rec::make_transaction(
    const prediction_to_rec_result& request,
    tlm::tlm_generic_payload& trans,
    std::vector<std::uint8_t>& storage) const
{
    storage = pack_request(request);

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(storage.empty() ? nullptr : storage.data());
    trans.set_data_length(storage.size());
}

} // namespace cdc::components
