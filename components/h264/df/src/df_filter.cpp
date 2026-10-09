#include "df_filter.h"

namespace h264::df {
DfFilter::DfFilter(sc_core::sc_module_name name) : sc_core::sc_module(name) {}

void DfFilter::apply_filter(std::array<std::uint8_t, 16>& left_blk, 
                            std::array<std::uint8_t, 16>& right_blk, 
                            std::uint8_t bs, std::uint8_t qp) {
    if (bs == 0) 
    return; // Không cần làm mịn

    // Mô phỏng bảng Threshold (Alpha, Beta) của H.264 dựa trên QP
    int alpha = qp + 10;
    int beta  = (qp / 2) + 4;
    
    // Giá trị clip giới hạn cho bộ lọc yếu
    int t_c = (bs < 4 && qp > 20) ? bs : 0; // Khi QP thấp và bS < 4 => t_c = 0 (Mạch tự động ngắt lọc để giữ chi tiết cạnh)

    for (int row = 0; row < 4; ++row) {
        int p0 = left_blk[row * 4 + 3];
        int p1 = left_blk[row * 4 + 2];
        int q0 = right_blk[row * 4 + 0];
        int q1 = right_blk[row * 4 + 1];
        
        // Điều kiện lọc cơ bản của H.264
        bool filter_condition = (std::abs(p0 - q0) < alpha || bs == 4) && 
                                (std::abs(p1 - p0) < beta  || bs == 4) && 
                                (std::abs(q1 - q0) < beta  || bs == 4);

        if (filter_condition) {
            if (bs == 4) {
                // Strong Filter: Giao thoa triệt để 
                int filter_val = (p0 + q0 + 1) >> 1; 
                left_blk[row * 4 + 3] = static_cast<std::uint8_t>(filter_val);
                right_blk[row * 4 + 0] = static_cast<std::uint8_t>(filter_val);
            } else {
                // Weak Filter: Giới hạn biến thiên 
                int delta = (q0 - p0 + 2) >> 2;
                
                // Hàm Clipping: Cắt biên
                if (delta > t_c) delta = t_c;
                else if (delta < -t_c) delta = -t_c;

                left_blk[row * 4 + 3] = static_cast<std::uint8_t>(p0 + delta);
                right_blk[row * 4 + 0] = static_cast<std::uint8_t>(q0 - delta);
            }
        }
    }
}
} // namespace h264::df