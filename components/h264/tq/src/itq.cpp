#include "itq.h"

#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <limits>

namespace h264::tq {

// Báº£ng há»‡ sá»‘ Inverse Scaling V[QP % 6][pos_type]
static const int V_TABLE[6][3] = {
    {10, 16, 13},
    {11, 18, 14},
    {13, 20, 16},
    {14, 23, 18},
    {16, 25, 20},
    {18, 29, 23}
};

std::array<std::int16_t, 16>
Itq::inverse_transform(const std::array<std::int16_t, 16>& levels, std::uint8_t qp, std::optional<std::int32_t> scaled_dc) const {
    if(qp>51) throw std::invalid_argument("ITQ QP");
    std::array<std::int16_t, 16> residual;
    int q_rem = qp % 6;
    int q_per = qp / 6;

    std::int64_t d[16];
    // Inverse Quantization / Scaling
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            int idx = r * 4 + c;
            int pos_type = ((r % 2 == 0) && (c % 2 == 0)) ? 0 :
                           (((r % 2 == 1) && (c % 2 == 1)) ? 1 : 2);
            d[idx] = (std::int64_t(levels[idx]) * V_TABLE[q_rem][pos_type]) * (1 << q_per);
        }
    }

    if(scaled_dc) d[0]=*scaled_dc;
    std::int64_t m[16];
    // Horizontal pass
    for (int i = 0; i < 4; ++i) {
        std::int64_t e0 = d[i * 4 + 0] + d[i * 4 + 2];
        std::int64_t e1 = d[i * 4 + 0] - d[i * 4 + 2];
        std::int64_t e2 = (d[i * 4 + 1] >> 1) - d[i * 4 + 3];
        std::int64_t e3 = d[i * 4 + 1] + (d[i * 4 + 3] >> 1);
        m[i * 4 + 0] = e0 + e3;
        m[i * 4 + 1] = e1 + e2;
        m[i * 4 + 2] = e1 - e2;
        m[i * 4 + 3] = e0 - e3;
    }

    auto narrow = [](std::int64_t value) {
        if(value<std::numeric_limits<std::int16_t>::min() || value>std::numeric_limits<std::int16_t>::max())
            throw std::overflow_error("ITQ residual range");
        return static_cast<std::int16_t>(value);
    };
    // Vertical pas + Predictor Addition + Clip1
    for (int j = 0; j < 4; ++j) {
        std::int64_t e0 = m[0 * 4 + j] + m[2 * 4 + j];
        std::int64_t e1 = m[0 * 4 + j] - m[2 * 4 + j];
        std::int64_t e2 = (m[1 * 4 + j] >> 1) - m[3 * 4 + j];
        std::int64_t e3 = m[1 * 4 + j] + (m[3 * 4 + j] >> 1);

        residual[0 * 4 + j] = narrow((e0 + e3 + 32) >> 6);
        residual[1 * 4 + j] = narrow((e1 + e2 + 32) >> 6);
        residual[2 * 4 + j] = narrow((e1 - e2 + 32) >> 6);
        residual[3 * 4 + j] = narrow((e0 - e3 + 32) >> 6);
    }
    return residual;
}

std::array<std::int32_t,4>
Itq::inverse_chroma_dc(const std::array<std::int16_t,4>& levels,std::uint8_t qp) const {
    if(qp>51) throw std::invalid_argument("chroma DC QP");
    const int h[2][2]={{1,1},{1,-1}};std::array<std::int32_t,4> out{};
    for(int y=0;y<2;++y) for(int x=0;x<2;++x) {
        std::int64_t sum=0;
        for(int j=0;j<2;++j) for(int i=0;i<2;++i) sum+=h[y][j]*levels[j*2+i]*h[x][i];
        out[y*2+x]=(sum*V_TABLE[qp%6][0]*(1<<(qp/6)))>>1;
    }
    return out;
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
