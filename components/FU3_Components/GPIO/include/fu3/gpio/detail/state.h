#pragma once
#include "fu3/gpio/registers.h"
#include <array>

namespace fu3::gpio::detail {
// One digital state machine shared by the TLM and signal-level APB interfaces.
class State {
  public:
    State() {
        reset();
    }
    void reset();
    void edge(std::uint32_t raw, bool read, bool write, unsigned index, std::uint32_t data);
    std::uint32_t read(unsigned index) const {
        return words_[index];
    }
    std::uint32_t synced() const {
        return sync1_;
    }
    bool irq() const {
        return irq_;
    }
    bool wake(std::uint32_t raw) const;
    unsigned padcfg(unsigned pin) const {
        return (words_[8 + pin / 4] >> (8 * (pin % 4))) & 63;
    }

  private:
    std::array<std::uint32_t, 16> words_{};
    std::uint32_t sync0_ = 0, sync1_ = 0, sample_ = 0;
    bool irq_ = false;
};
} // namespace fu3::gpio::detail
