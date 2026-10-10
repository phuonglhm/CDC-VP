#include "ftq.h"

#include <algorithm>
#include <cmath>
 
namespace h264::tq {

// Bảng hệ số lượng tử hóa MF[QP % 6][pos_type] theo ITU-T H.264
// pos_type: 0: (0,0),(0,2),(2,0),(2,2); 1: (1,1),(1,3),(3,1),(3,3); 2: còn lại
static const int MF_TABLE[6][3] = {
    {13107, 5243, 8066},
    {11916, 4660, 7490},
    {10082, 4194, 6554},
    {9362,  3647, 5825},
    {8192,  3355, 5243},
    {7282,  2893, 4559}
};

Ftq::Ftq(TransposeRam& transpose) : transpose_(transpose) {}

std::array<std::int32_t, 16>
Ftq::transform(const std::array<std::int16_t, 16>& residual, BlockClass block_class) {
    std::array<std::int32_t, 16> d;
    std::array<std::int32_t, 16> c;
    
    // Core Transform 4x4 (Row pass)
    for (int i = 0; i < 4; ++i) {
        int p0 = residual[i * 4 + 0] + residual[i * 4 + 3];
        int p1 = residual[i * 4 + 1] + residual[i * 4 + 2];
        int p2 = residual[i * 4 + 1] - residual[i * 4 + 2];
        int p3 = residual[i * 4 + 0] - residual[i * 4 + 3];

        d[i * 4 + 0] = p0 + p1;
        d[i * 4 + 1] = (p3 << 1) + p2;
        d[i * 4 + 2] = p0 - p1;
        d[i * 4 + 3] = p3 - (p2 << 1);

        transpose_.write_row(i, d); // Ghi từng hàng vào khối Transpose RAM
    }

    // Core Transform 4x4 (Column pass)
    for (int j = 0; j < 4; ++j) {
        
        auto col_data = transpose_.read_column(j); // Lấy dữ liệu đã xoay chiều từ Transpose RAM

        int p0 = col_data[0 * 4 + j] + col_data[3 * 4 + j];
        int p1 = col_data[1 * 4 + j] + col_data[2 * 4 + j];
        int p2 = col_data[1 * 4 + j] - col_data[2 * 4 + j];
        int p3 = col_data[0 * 4 + j] - col_data[3 * 4 + j];

        c[0 * 4 + j] = p0 + p1;
        c[1 * 4 + j] = (p3 << 1) + p2;
        c[2 * 4 + j] = p0 - p1;
        c[3 * 4 + j] = p3 - (p2 << 1);
    }

    // Hadamard DC (Bật tùy theo loại BlockClass của H.264)
    
    bool is_dc = (block_class == BlockClass::Luma16x16Dc || block_class == BlockClass::ChromaDc);
    
    if (is_dc) { 
        std::array<std::int32_t, 16> m;
        for (int i = 0; i < 4; ++i) {
            m[i * 4 + 0] = c[i * 4 + 0] + c[i * 4 + 3];
            m[i * 4 + 1] = c[i * 4 + 1] + c[i * 4 + 2];
            m[i * 4 + 2] = c[i * 4 + 1] - c[i * 4 + 2];
            m[i * 4 + 3] = c[i * 4 + 0] - c[i * 4 + 3];
        }
        for (int j = 0; j < 4; ++j) {
            c[0 * 4 + j] = (m[0 * 4 + j] + m[3 * 4 + j]) >> 1;
            c[1 * 4 + j] = (m[0 * 4 + j] - m[3 * 4 + j]) >> 1;
            c[2 * 4 + j] = (m[1 * 4 + j] - m[2 * 4 + j]) >> 1;
            c[3 * 4 + j] = (m[1 * 4 + j] + m[2 * 4 + j]) >> 1;
        }
    }
    return c;
}

std::array<std::int16_t, 16>
Ftq::quantize(const std::array<std::int32_t, 16>& coeffs, std::uint8_t qp, BlockClass block_class, bool is_intra) const {
    std::array<std::int16_t, 16> levels;
    int q_rem = qp % 6;
    int q_per = qp / 6;
    int qbits = 15 + q_per;
    int f = (1 << qbits) / (is_intra ? 3 : 6); // f = (1 << qbits) / 3 cho Intra, / 6 cho Inter

    for (int r = 0; r < 4; ++r) {
        for (int col = 0; col < 4; ++col) {
            int idx = r * 4 + col;
            int pos_type = ((r % 2 == 0) && (col % 2 == 0)) ? 0 : 
                           (((r % 2 == 1) && (col % 2 == 1)) ? 1 : 2);
            int mf = MF_TABLE[q_rem][pos_type];
            int sign = (coeffs[idx] < 0) ? -1 : 1;
            int level = (std::abs(coeffs[idx]) * mf + f) >> qbits;
            levels[idx] = static_cast<std::int16_t>(sign * level);
        }
    }
    return levels;
}

} // namespace h264::tq
