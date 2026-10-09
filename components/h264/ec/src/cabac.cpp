#include "cabac.h"
#include <cmath>

namespace h264::ec {

Cabac::Cabac(sc_core::sc_module_name name) : sc_core::sc_module(name) {}

void Cabac::encode(const EcRequest& req, BitWriter& bw) {
    std::uint32_t range = 0x01FE, low = 0x0000;
    for(int i = 0; i < 16; ++i) {
        std::int16_t val = req.levels[i];
        if (val == 0) continue;
        for (int b = 0; b < 4; ++b) { 
            std::uint8_t bit = (std::abs(val) >> b) & 1;
            low <<= 1;

            if (bit) low += range;

            if (low >= 0x0200) { bw.write_bits(1, 1); low &= 0x01FF; }
            else { bw.write_bits(0, 1); }
        }
        bw.write_bits(val > 0 ? 0 : 1, 1);
    }
}

} // namespace h264::ec