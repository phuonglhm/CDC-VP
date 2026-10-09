#include "exp_golomb.h"

namespace h264::ec {

void ExpGolomb::encode_se(std::int16_t val, BitWriter& bw) {
    std::uint32_t code_num = (val <= 0) ? (-val * 2) : (val * 2 - 1);
    std::uint32_t m = code_num + 1;

    int len = 0;
    while ((m >> len) > 0) len++;

    len--;

    bw.write_bits(0, len);
    bw.write_bits(m, len + 1);

}

} // namespace h264::ec