#pragma once

#include "hevc/bit_writer.hpp"

#include <cstdint>

namespace hevc {

// The MVP needs one regular bin (split_cu_flag) and termination bins for PCM.
// This is nevertheless a complete binary arithmetic engine so later stages can
// reuse it for intra prediction and transform coefficient syntax.
class CabacContext {
public:
    static CabacContext from_init_value(std::uint8_t init_value, int slice_qp);

    [[nodiscard]] unsigned state() const { return state_; }
    [[nodiscard]] unsigned mps() const { return mps_; }
    void update_mps();
    void update_lps();

private:
    CabacContext(unsigned state, unsigned mps) : state_(state), mps_(mps) {}
    unsigned state_ = 0;
    unsigned mps_ = 0;
};

class CabacEncoder {
public:
    explicit CabacEncoder(BitWriter& out) : out_(out) {}

    void encode_bin(unsigned bin, CabacContext& context);
    void encode_bypass(unsigned bin);
    void encode_terminate(unsigned bin);
    void finish();

private:
    void write_out();

    BitWriter& out_;
    std::uint32_t low_ = 0;
    std::uint32_t range_ = 510;
    int bits_left_ = 23;
    std::uint32_t buffered_byte_ = 0xff;
    unsigned num_buffered_bytes_ = 0;
    bool finished_ = false;
};

} // namespace hevc
