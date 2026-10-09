#include "itq.h"
#include <algorithm>

namespace h264::tq {

std::array<std::int16_t, 16> Itq::inverse_transform(const std::array<std::int16_t,16>& levels,
                                                   std::uint8_t) const {
    std::array<std::int32_t, 16> tmp{};
    for (unsigned y = 0; y < 4; ++y) {
        const int a0 = levels[y*4+0];
        const int a1 = levels[y*4+1];
        const int a2 = levels[y*4+2];
        const int a3 = levels[y*4+3];

        const int e0 = a0 + a2;
        const int e1 = a0 - a2;
        const int e2 = (a1 >> 1) - a3;
        const int e3 = a1 + (a3 >> 1);

        tmp[y*4 + 0] = e0 + e3;
        tmp[y*4 + 1] = e1 + e2;
        tmp[y*4 + 2] = e1 - e2;
        tmp[y*4 + 3] = e0 - e3;
    }

    std::array<std::int16_t, 16> out{};
    for (unsigned x = 0; x < 4; ++x) {
        const int a0 = tmp[0*4 + x];
        const int a1 = tmp[1*4 + x];
        const int a2 = tmp[2*4 + x];
        const int a3 = tmp[3*4 + x];

        const int e0 = a0 + a2;
        const int e1 = a0 - a2;
        const int e2 = (a1 >> 1) - a3;
        const int e3 = a1 + (a3 >> 1);

        out[0*4 + x] = static_cast<std::int16_t>((e0 + e3 + 32) >> 6);
        out[1*4 + x] = static_cast<std::int16_t>((e1 + e2 + 32) >> 6);
        out[2*4 + x] = static_cast<std::int16_t>((e1 - e2 + 32) >> 6);
        out[3*4 + x] = static_cast<std::int16_t>((e0 - e3 + 32) >> 6);
    }
    return out;
}

std::uint8_t Itq::clip1(std::int32_t value) {
    value = std::max<std::int32_t>(0, std::min<std::int32_t>(255, value));
    return static_cast<std::uint8_t>(value);
}

std::array<std::uint8_t, 16> Itq::reconstruct(const std::array<std::int16_t,16>& residual,
                                             const std::array<std::uint8_t,16>& predictor) const {
    std::array<std::uint8_t, 16> out{};
    for (unsigned i = 0; i < 16; ++i) {
        out[i] = clip1(static_cast<std::int32_t>(predictor[i]) + residual[i]);
    }
    return out;
}

} // namespace h264::tq