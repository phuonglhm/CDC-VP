#pragma once

#include <vector>

#include "prediction_to_rec_packet.h"
#include "rec_mv.h"
#include "tlm.h"

namespace cdc::components {

class video_encoder_to_rec {
public:
    video_encoder_to_rec() = default;

    bool write_mv_if_needed(const prediction_to_rec_result& request,
                            RecMvIf& mv_mem) const;

    std::vector<std::uint8_t> pack_request(
        const prediction_to_rec_result& request) const;

    bool targets_mc(const prediction_to_rec_result& request) const;

    void make_transaction(const prediction_to_rec_result& request,
                          tlm::tlm_generic_payload& trans,
                          std::vector<std::uint8_t>& storage) const;
};

} // namespace cdc::components
