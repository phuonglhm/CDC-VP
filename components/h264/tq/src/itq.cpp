#include "itq.h"

#include <cmath>
#include <algorithm>

namespace h264::tq {

// Bảng hệ số Inverse Scaling V[QP % 6][pos_type]
static const int V_TABLE[6][3] = {
    {10, 16, 13},
    {11, 18, 14},
    {13, 20, 16},
    {14, 23, 18},
    {16, 25, 20},
    {18, 29, 23}
};

std::array<std::int16_t, 16>
Itq::inverse_transform(const std::array<std::int16_t, 16>& levels, std::uint8_t qp) const {
    std::array<std::int16_t, 16> residual;
    int q_rem = qp % 6;
    int q_per = qp / 6;

    std::int32_t d[16];
    // Inverse Quantization / Scaling
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            int idx = r * 4 + c;
            int pos_type = ((r % 2 == 0) && (c % 2 == 0)) ? 0 : 
                           (((r % 2 == 1) && (c % 2 == 1)) ? 1 : 2);
            d[idx] = (levels[idx] * V_TABLE[q_rem][pos_type]) << q_per;
        }
    }

    std::int32_t m[16];
    // Horizontal pass
    for (int i = 0; i < 4; ++i) {
        int e0 = d[i * 4 + 0] + d[i * 4 + 2];
        int e1 = d[i * 4 + 0] - d[i * 4 + 2];
        int e2 = (d[i * 4 + 1] >> 1) - d[i * 4 + 3];
        int e3 = d[i * 4 + 1] + (d[i * 4 + 3] >> 1);
        m[i * 4 + 0] = e0 + e3;
        m[i * 4 + 1] = e1 + e2;
        m[i * 4 + 2] = e1 - e2;
        m[i * 4 + 3] = e0 - e3;
    }

    // Vertical pas + Predictor Addition + Clip1
    for (int j = 0; j < 4; ++j) {
        int e0 = m[0 * 4 + j] + m[2 * 4 + j];
        int e1 = m[0 * 4 + j] - m[2 * 4 + j];
        int e2 = (m[1 * 4 + j] >> 1) - m[3 * 4 + j];
        int e3 = m[1 * 4 + j] + (m[3 * 4 + j] >> 1);

        residual[0 * 4 + j] = static_cast<std::int16_t>((e0 + e3 + 32) >> 6);
        residual[1 * 4 + j] = static_cast<std::int16_t>((e1 + e2 + 32) >> 6);
        residual[2 * 4 + j] = static_cast<std::int16_t>((e1 - e2 + 32) >> 6);
        residual[3 * 4 + j] = static_cast<std::int16_t>((e0 - e3 + 32) >> 6);
    }
    return residual;
}

std::array<std::uint8_t, 16>
Itq::reconstruct(const std::array<std::int16_t, 16>& residual, 
                 const std::array<std::uint8_t, 16>& predictor) const {
    std::array<std::uint8_t, 16> recon;
    for (int i = 0; i < 16; ++i) {
        recon[i] = clip1(residual[i] + predictor[i]);
    }
    return recon;
}

std::uint8_t Itq::clip1(std::int32_t value) {
    return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

} // namespace h264::tq
