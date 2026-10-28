#include "fu3_plic/intmap.h"

namespace fu3_plic {

Intmap::Intmap(sc_core::sc_module_name name) : sc_core::sc_module(name) {
    SC_METHOD(update);
    sensitive << changed_;
    for (auto& s : sources) sensitive << s;
}

void Intmap::set_enable(unsigned source, bool on) {
    if (source >= kIntmapSources) return;
    const std::uint64_t bit = std::uint64_t(1) << source;
    enables_ = on ? (enables_ | bit) : (enables_ & ~bit);
    changed_.notify(sc_core::SC_ZERO_TIME);
}

void Intmap::enable_all(bool on) {
    enables_ = on ? (std::uint64_t(1) << kIntmapSources) - 1 : 0;
    changed_.notify(sc_core::SC_ZERO_TIME);
}

void Intmap::update() {
    bool out[kIntmapOutputs] = {};
    for (unsigned s = 0; s < kIntmapSources; ++s)
        if (enabled(s) && sources[s].read()) out[kIntmapRoute[s]] = true;
    for (unsigned o = 0; o < kIntmapOutputs; ++o) irq[o].write(out[o]);   // NMI stays low
}

}  // namespace fu3_plic
