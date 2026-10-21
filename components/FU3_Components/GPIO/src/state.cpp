#include "fu3/gpio/detail/state.h"

namespace fu3::gpio::detail {
void State::reset() {
    words_.fill(0);
    for (unsigned i = 8; i < 16; ++i)
        words_[i] = reg::PADCFG_RESET;
    sync0_ = sync1_ = sample_ = 0;
    irq_ = false;
}

void State::edge(std::uint32_t raw, bool read, bool write, unsigned index, std::uint32_t data) {
    // Event conditions observe pre-edge samples and configuration, like RTL
    // nonblocking assignments. A same-edge configuration write applies later.
    const auto t0 = words_[4], t1 = words_[5];
    const std::uint32_t active =
        words_[3] & ((~t1 & ~t0 & sample_) | (~t1 & t0 & ~sample_) |
                     (t1 & ~t0 & sync1_ & ~sample_) | (t1 & t0 & ~sync1_ & sample_));
    const bool was_pending = irq_;
    if (write && index != 1 && index != 6)
        words_[index] = data & (index >= 8 ? reg::PADCFG_MASK : 0xffffffffu);
    if (read && index == 6 && was_pending) {
        irq_ = false;
        words_[6] = 0;
    } else if (!was_pending && active) {
        irq_ = true;
        words_[6] = active;
    }
    sample_ = sync1_;
    sync1_ = sync0_;
    sync0_ = raw;
    words_[1] = sample_;
}

bool State::wake(std::uint32_t raw) const {
    bool result = false;
    for (unsigned i = 0; i < 32; ++i)
        if (words_[7] & (std::uint32_t{1} << i))
            result = (raw >> i) & 1;
    return result;
}
} // namespace fu3::gpio::detail
