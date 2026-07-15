#include "hevc/cabac.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace hevc {
namespace {

constexpr std::array<std::array<std::uint8_t, 4>, 64> kLpsTable{{
    {{128,176,208,240}}, {{128,167,197,227}}, {{128,158,187,216}}, {{123,150,178,205}},
    {{116,142,169,195}}, {{111,135,160,185}}, {{105,128,152,175}}, {{100,122,144,166}},
    {{ 95,116,137,158}}, {{ 90,110,130,150}}, {{ 85,104,123,142}}, {{ 81, 99,117,135}},
    {{ 77, 94,111,128}}, {{ 73, 89,105,122}}, {{ 69, 85,100,116}}, {{ 66, 80, 95,110}},
    {{ 62, 76, 90,104}}, {{ 59, 72, 86, 99}}, {{ 56, 69, 81, 94}}, {{ 53, 65, 77, 89}},
    {{ 51, 62, 73, 85}}, {{ 48, 59, 69, 80}}, {{ 46, 56, 66, 76}}, {{ 43, 53, 63, 72}},
    {{ 41, 50, 59, 69}}, {{ 39, 48, 56, 65}}, {{ 37, 45, 54, 62}}, {{ 35, 43, 51, 59}},
    {{ 33, 41, 48, 56}}, {{ 32, 39, 46, 53}}, {{ 30, 37, 43, 50}}, {{ 29, 35, 41, 48}},
    {{ 27, 33, 39, 45}}, {{ 26, 31, 37, 43}}, {{ 24, 30, 35, 41}}, {{ 23, 28, 33, 39}},
    {{ 22, 27, 32, 37}}, {{ 21, 26, 30, 35}}, {{ 20, 24, 29, 33}}, {{ 19, 23, 27, 31}},
    {{ 18, 22, 26, 30}}, {{ 17, 21, 25, 28}}, {{ 16, 20, 23, 27}}, {{ 15, 19, 22, 25}},
    {{ 14, 18, 21, 24}}, {{ 14, 17, 20, 23}}, {{ 13, 16, 19, 22}}, {{ 12, 15, 18, 21}},
    {{ 12, 14, 17, 20}}, {{ 11, 14, 16, 19}}, {{ 11, 13, 15, 18}}, {{ 10, 12, 15, 17}},
    {{ 10, 12, 14, 16}}, {{  9, 11, 13, 15}}, {{  9, 11, 12, 14}}, {{  8, 10, 12, 14}},
    {{  8,  9, 11, 13}}, {{  7,  9, 11, 12}}, {{  7,  9, 10, 12}}, {{  7,  8, 10, 11}},
    {{  6,  8,  9, 11}}, {{  6,  7,  9, 10}}, {{  6,  7,  8,  9}}, {{  2,  2,  2,  2}},
}};

constexpr std::array<std::uint8_t, 64> kNextMps{{
     1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,
    17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
    33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
    49,50,51,52,53,54,55,56,57,58,59,60,61,62,62,63,
}};

constexpr std::array<std::uint8_t, 64> kNextLps{{
     0, 0, 1, 2, 2, 4, 4, 5, 6, 7, 8, 9, 9,11,11,12,
    13,13,15,15,16,16,18,18,19,19,21,21,22,22,23,24,
    24,25,26,26,27,27,28,29,29,30,30,30,31,32,32,33,
    33,33,34,34,35,35,35,36,36,36,37,37,37,38,38,63,
}};

constexpr std::array<std::uint8_t, 32> kRenormTable{{
    6,5,4,4,3,3,3,3,2,2,2,2,2,2,2,2,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
}};

} // namespace

CabacContext CabacContext::from_init_value(std::uint8_t init_value, int slice_qp) {
    slice_qp = std::clamp(slice_qp, 0, 51);
    const int slope = static_cast<int>(init_value >> 4) * 5 - 45;
    const int offset = static_cast<int>(init_value & 15) * 8 - 16;
    const int init_state = std::clamp(((slope * slice_qp) >> 4) + offset, 1, 126);
    if (init_state >= 64) {
        return CabacContext(static_cast<unsigned>(init_state - 64), 1);
    }
    return CabacContext(static_cast<unsigned>(63 - init_state), 0);
}

void CabacContext::update_mps() {
    state_ = kNextMps[state_];
}

void CabacContext::update_lps() {
    if (state_ == 0) {
        mps_ ^= 1U;
    }
    state_ = kNextLps[state_];
}

void CabacEncoder::encode_bin(unsigned bin, CabacContext& context) {
    if (finished_ || bin > 1) {
        throw std::logic_error("invalid CABAC regular bin");
    }
    const auto lps = kLpsTable[context.state()][(range_ >> 6) & 3U];
    range_ -= lps;
    if (bin != context.mps()) {
        const auto shift = kRenormTable[lps >> 3];
        low_ = (low_ + range_) << shift;
        range_ = static_cast<std::uint32_t>(lps) << shift;
        bits_left_ -= shift;
        context.update_lps();
    } else {
        context.update_mps();
        if (range_ >= 256) {
            return;
        }
        low_ <<= 1;
        range_ <<= 1;
        --bits_left_;
    }
    if (bits_left_ < 12) {
        write_out();
    }
}

void CabacEncoder::encode_bypass(unsigned bin) {
    if (finished_ || bin > 1) {
        throw std::logic_error("invalid CABAC bypass bin");
    }
    low_ <<= 1;
    if (bin) {
        low_ += range_;
    }
    --bits_left_;
    if (bits_left_ < 12) {
        write_out();
    }
}

void CabacEncoder::encode_terminate(unsigned bin) {
    if (finished_ || bin > 1) {
        throw std::logic_error("invalid CABAC termination bin");
    }
    range_ -= 2;
    if (bin) {
        low_ += range_;
        low_ <<= 7;
        range_ = 2U << 7;
        bits_left_ -= 7;
    } else if (range_ < 256) {
        low_ <<= 1;
        range_ <<= 1;
        --bits_left_;
    } else {
        return;
    }
    if (bits_left_ < 12) {
        write_out();
    }
}

void CabacEncoder::write_out() {
    const std::uint32_t lead_byte = low_ >> (24 - bits_left_);
    bits_left_ += 8;
    low_ &= 0xffffffffU >> bits_left_;

    if (lead_byte == 0xff) {
        ++num_buffered_bytes_;
        return;
    }
    if (num_buffered_bytes_ > 0) {
        const std::uint32_t carry = lead_byte >> 8;
        out_.put_bits(buffered_byte_ + carry, 8);
        const std::uint32_t fill = (0xffU + carry) & 0xffU;
        while (num_buffered_bytes_ > 1) {
            out_.put_bits(fill, 8);
            --num_buffered_bytes_;
        }
        buffered_byte_ = lead_byte & 0xffU;
    } else {
        num_buffered_bytes_ = 1;
        buffered_byte_ = lead_byte;
    }
}

void CabacEncoder::finish() {
    if (finished_) {
        throw std::logic_error("CABAC encoder already finished");
    }
    if ((low_ >> (32 - bits_left_)) != 0) {
        out_.put_bits(buffered_byte_ + 1, 8);
        while (num_buffered_bytes_ > 1) {
            out_.put_bits(0x00, 8);
            --num_buffered_bytes_;
        }
        low_ -= 1U << (32 - bits_left_);
    } else {
        if (num_buffered_bytes_ > 0) {
            out_.put_bits(buffered_byte_, 8);
        }
        while (num_buffered_bytes_ > 1) {
            out_.put_bits(0xff, 8);
            --num_buffered_bytes_;
        }
    }
    out_.put_bits(low_ >> 8, static_cast<unsigned>(24 - bits_left_));
    finished_ = true;
}

} // namespace hevc
