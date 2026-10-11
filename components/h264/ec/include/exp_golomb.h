#pragma once
#include "ec_types.h"

namespace h264::ec {

struct ExpGolomb {
    static void encode_ue(std::uint32_t val, BitWriter& bw);
    static void encode_se(std::int16_t val, BitWriter& bw);
};

} // namespace h264::ec
