#pragma once

#include <cstdint>

#include "block.h"
#include "prediction_result.h"
#include "rec_mv.h"
#include "rec_packet.h"

namespace cdc::components {

struct prediction_to_rec_result {
    bool valid = false;

    RecPacket packet;

    bool has_mv = false;
    MotionVector mv {};
    // Extended MV-memory address derived from block-tag high bits plus packet
    // x/y low bits so larger frames do not alias on the backend.
    std::uint32_t mv_addr = 0;

    static prediction_to_rec_result invalid()
    {
        return prediction_to_rec_result {};
    }
};

class prediction_to_rec_packet {
public:
    prediction_to_rec_packet() = default;

    prediction_to_rec_result run(const block& region,
                                 const prediction_result& prediction) const;

private:
    std::uint8_t encode_block_size(const block& region) const;
    std::uint8_t encode_intra_mode(const prediction_result& prediction) const;
    std::uint32_t derive_block_index(const block& region) const;
};

} // namespace cdc::components
