#pragma once
#include <h264/types.h>

namespace h264 {
// Persistent state is authoritative; an event is only a wake-up notification.
class ResetDomain {
public:
    uint64_t generation = 0;
    bool active = true;
    sc_core::sc_event changed;
    void assert_reset() {
        active = true;
        ++generation;
        changed.notify(sc_core::SC_ZERO_TIME);
    }
    void release() { active = false; changed.notify(sc_core::SC_ZERO_TIME); }
    bool valid(uint64_t token) const { return !active && token == generation; }
};
struct EpochExtension : tlm::tlm_extension<EpochExtension> {
    ResetDomain* domain = nullptr;
    uint64_t generation = 0;
    explicit EpochExtension(ResetDomain& d) : domain(&d), generation(d.generation) {}
    EpochExtension() = default;
    bool valid() const { return !domain || domain->valid(generation); }
    tlm_extension_base* clone() const override { return new EpochExtension(*this); }
    void copy_from(const tlm_extension_base& other) override {
        *this = static_cast<const EpochExtension&>(other);
    }
};
inline bool epoch_valid(tlm::tlm_generic_payload& tx) {
    auto* epoch = tx.get_extension<EpochExtension>();
    return !epoch || epoch->valid();
}
class StickyCompletion {
public:
    sc_core::sc_event event;
    void set(uint64_t generation) {
        generation_ = generation; pending_ = true;
        event.notify(sc_core::SC_ZERO_TIME);
    }
    bool take(uint64_t generation) {
        if (!pending_ || generation_ != generation) return false;
        pending_ = false;
        return true;
    }
    void clear() { pending_ = false; }
private:
    bool pending_ = false;
    uint64_t generation_ = 0;
};
}
