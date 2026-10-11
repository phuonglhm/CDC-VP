#include "df_filter.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace h264::df {

static const std::uint8_t ALPHA_TABLE[52] = {
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,4,5,6,
     7,8,9,10,12,13,15,17,20,22,25,28,32,36,40,45,50,56,63,71,
     80,90,101,113,127,144,162,182,203,226,255,255
};

static const std::uint8_t BETA_TABLE[52] = {
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,3,
     3,3,3,4,4,4,6,6,7,7,8,8,9,9,10,10,11,11,12,12,
     13,13,14,14,15,15,16,16,17,17,18,18
};

// AVC Table 8-17: rows indexed by clipped indexA, columns bS=1..3.
static const std::uint8_t TC0_TABLE[52][3] = {
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,1},
    {0,0,1},
    {0,0,1},
    {0,0,1},
    {0,1,1},
    {0,1,1},
    {1,1,1},
    {1,1,1},
    {1,1,1},
    {1,1,1},
    {1,1,2},
    {1,1,2},
    {1,1,2},
    {1,1,2},
    {1,2,3},
    {1,2,3},
    {2,2,3},
    {2,2,4},
    {2,3,4},
    {2,3,4},
    {3,3,5},
    {3,4,6},
    {3,4,6},
    {4,5,7},
    {4,5,8},
    {4,6,9},
    {5,7,10},
    {6,8,11},
    {6,8,13},
    {7,10,14},
    {8,11,16},
    {9,12,18},
    {10,13,20},
    {11,15,23},
    {13,17,25}
};

unsigned boundary_strength(const EdgeSide& p,const EdgeSide& q,bool external) {
    if(p.intra || q.intra) return external ? 4 : 3;
    if(p.nonzero || q.nonzero) return 2;
    const auto dx=std::int64_t(p.mv_x)-q.mv_x,dy=std::int64_t(p.mv_y)-q.mv_y;
    if(p.reference!=q.reference || dx<=-4 || dx>=4 || dy<=-4 || dy>=4) return 1;
    return 0;
}
unsigned chroma_qp(unsigned qp) {
    if(qp>51) throw std::invalid_argument("chroma QP");
    static constexpr unsigned char table[22]={29,30,31,32,32,33,34,34,35,35,36,36,37,37,37,38,38,38,39,39,39,39};
    return qp<30 ? qp : table[qp-30];
}

DfFilter::DfFilter(sc_core::sc_module_name name) : sc_core::sc_module(name) {}

void DfFilter::apply_filter(std::array<std::uint8_t, 16>& left_blk,
                            std::array<std::uint8_t, 16>& right_blk,
                            std::uint8_t bs, std::uint8_t qp, bool chroma,
                            int alpha_offset, int beta_offset) {
    if (bs > 4 || qp > 51 || alpha_offset < -16 || alpha_offset > 15 ||
        beta_offset < -16 || beta_offset > 15) throw std::invalid_argument("DF parameters");
    if (bs == 0) return;

    int indexA = std::clamp(static_cast<int>(qp) + alpha_offset, 0, 51);
    int indexB = std::clamp(static_cast<int>(qp) + beta_offset, 0, 51);

    int alpha = ALPHA_TABLE[indexA];
    int beta  = BETA_TABLE[indexB];

    for (int row = 0; row < 4; ++row) {
        int p0 = left_blk[row * 4 + 3], p1 = left_blk[row * 4 + 2];
        int p2 = left_blk[row * 4 + 1], p3 = left_blk[row * 4 + 0];
        int q0 = right_blk[row * 4 + 0], q1 = right_blk[row * 4 + 1];
        int q2 = right_blk[row * 4 + 2], q3 = right_blk[row * 4 + 3];

        if (!((std::abs(p0 - q0) < alpha) && (std::abs(p1 - p0) < beta) && (std::abs(q1 - q0) < beta)))
            continue;

        if (chroma) {
            // Chroma modifies only the two samples adjacent to the edge.
            if (bs == 4) {
                left_blk[row*4+3] = (2*p1+p0+q1+2)/4;
                right_blk[row*4] = (2*q1+q0+p1+2)/4;
            } else {
                int tc = TC0_TABLE[indexA][bs-1]+1;
                int delta = std::clamp(((q0-p0)*4+p1-q1+4)>>3, -tc, tc);
                left_blk[row*4+3] = std::clamp(p0+delta,0,255);
                right_blk[row*4] = std::clamp(q0-delta,0,255);
            }
        } else if (bs == 4) {
            bool filter_p = (std::abs(p2 - p0) < beta) && (std::abs(p0 - q0) < ((alpha >> 2) + 2));
            bool filter_q = (std::abs(q2 - q0) < beta) && (std::abs(p0 - q0) < ((alpha >> 2) + 2));

            if (filter_p) {
                left_blk[row * 4 + 3] = static_cast<std::uint8_t>((p2 + 2 * p1 + 2 * p0 + 2 * q0 + q1 + 4) >> 3);
                left_blk[row * 4 + 2] = static_cast<std::uint8_t>((p2 + p1 + p0 + q0 + 2) >> 2);
                left_blk[row * 4 + 1] = static_cast<std::uint8_t>((2 * p3 + 3 * p2 + p1 + p0 + q0 + 4) >> 3);
            } else {
                left_blk[row * 4 + 3] = static_cast<std::uint8_t>((2 * p1 + p0 + q1 + 2) >> 2);
            }

            if (filter_q) {
                right_blk[row * 4 + 0] = static_cast<std::uint8_t>((q2 + 2 * q1 + 2 * q0 + 2 * p0 + p1 + 4) >> 3);
                right_blk[row * 4 + 1] = static_cast<std::uint8_t>((q2 + q1 + q0 + p0 + 2) >> 2);
                right_blk[row * 4 + 2] = static_cast<std::uint8_t>((2 * q3 + 3 * q2 + q1 + q0 + p0 + 4) >> 3);
            } else {
                right_blk[row * 4 + 0] = static_cast<std::uint8_t>((2 * q1 + q0 + p1 + 2) >> 2);
            }
        } else {
            int tc0 = TC0_TABLE[indexA][bs - 1];
            int tc = tc0;
            if (std::abs(p2 - p0) < beta) {
                left_blk[row * 4 + 2] = static_cast<std::uint8_t>(p1 + std::clamp((p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1, -tc0, tc0));
                tc++;
            }
            if (std::abs(q2 - q0) < beta) {
                right_blk[row * 4 + 1] = static_cast<std::uint8_t>(q1 + std::clamp((q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1, -tc0, tc0));
                tc++;
            }
            int delta = std::clamp((((q0 - p0) * 4) + (p1 - q1) + 4) >> 3, -tc, tc);
            left_blk[row * 4 + 3]  = static_cast<std::uint8_t>(std::clamp(p0 + delta, 0, 255));
            right_blk[row * 4 + 0] = static_cast<std::uint8_t>(std::clamp(q0 - delta, 0, 255));
        }
    }
}

} // namespace h264::df
