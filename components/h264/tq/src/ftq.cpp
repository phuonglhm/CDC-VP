#include "ftq.h"
#include <algorithm>
#include <cmath>
 
namespace h264::tq {

Ftq::Ftq(TransposeRam& transpose)
    : transpose_(transpose) {}

    std::array<std::int32_t,16> Ftq::transform(const std::array<std::int16_t,16>& residual, BlockClass block_class) {
    std::array<std::int32_t,16> col_pass{};

        // Rẽ nhánh kiến trúc theo Spec: Luma16x16Dc hoặc ChromaDc
        if (block_class == BlockClass::Luma16x16Dc || block_class == BlockClass::ChromaDc) {
            for (unsigned int i = 0; i < 16; ++i) // Tạm bypass Hadamard DC => cần tối ưu sau.
               col_pass[i] = residual[i];
            return col_pass;
        }

        std::array<std::int32_t, 16> row{};
        for (unsigned y = 0; y < 4; ++y) {
            const int x0 = residual[y*4 + 0];
            const int x1 = residual[y*4 + 1];
            const int x2 = residual[y*4 + 2];
            const int x3 = residual[y*4 + 3];

            row[y*4 + 0] = x0 + x1 + x2 + x3;
            row[y*4 + 1] = 2*x0 + x1 - x2 - 2*x3;
            row[y*4 + 2] = x0 - x1 - x2 + x3;
            row[y*4 + 3] = x0 - 2*x1 + 2*x2 - x3;

            std::array<std::int32_t, 16> tmp = row;
            transpose_.write_row(y, tmp);
        }

        for (unsigned x = 0; x < 4; ++x) {
            const int x0 = row[0*4 + x];
            const int x1 = row[1*4 + x];
            const int x2 = row[2*4 + x];
            const int x3 = row[3*4 + x];

            col_pass[0*4 + x] = x0 + x1 + x2 + x3;
            col_pass[1*4 + x] = 2*x0 + x1 - x2 - 2*x3;
            col_pass[2*4 + x] = x0 - x1 - x2 + x3;
            col_pass[3*4 + x] = x0 - 2*x1 + 2*x2 - x3;
        }
        return col_pass;
    }

    std::array<std::int16_t, 16> Ftq::quantize(const std::array<std::int32_t, 16>& c,
                                          std::uint8_t qp,
                                          BlockClass) const {
        if (qp > 51) qp = 51;
        std::array<std::int16_t, 16> out{};
        const unsigned qbits = 15 + (qp / 6);

        for (unsigned i = 0; i < 16; ++i) {
            const std::int32_t f = 1 << (qbits - 1);
            const std::int32_t a = std::abs(c[i]);
            std::int32_t level = (a + f) >> qbits;
            if (c[i] < 0) level = -level;
            out[i] = static_cast<std::int16_t>(
                std::max<std::int32_t>(-32768, std::min<std::int32_t>(32767, level)));
        }
        return out;
    }

} // namespace h264::tq

