#include "exp_golomb.h"
#include <limits>
namespace h264::ec {
void ExpGolomb::encode_ue(std::uint32_t val, BitWriter& bw) {
    // Use 64 bits so UINT32_MAX + 1 does not wrap.
    std::uint64_t code = std::uint64_t(val) + 1;
    int order=0;
    for(auto n=code; n>1; n>>=1) ++order;
    if(std::uint64_t(bw.byte_count)*8+bw.bit_offset+2*order+1>bw.stream.size()*8)
        throw std::overflow_error("Exp-Golomb reservoir full");
    bw.write_bits(0,order);
    if(order==32) { bw.write_bits(1,1); bw.write_bits(0,32); }
    else bw.write_bits(static_cast<std::uint32_t>(code),order+1);
}
void ExpGolomb::encode_se(std::int16_t val, BitWriter& bw) {
    const int wide=val;
    encode_ue(wide<=0 ? std::uint32_t(-2*wide) : std::uint32_t(2*wide-1),bw);
}
} // namespace h264::ec
