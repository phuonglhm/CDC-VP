#pragma once
#include <tlm>
#include "dma/registers.h"

namespace fx1::dma {
// Optional metadata; ordinary TLM targets may ignore it. FIXED bursts are
// transported beat by beat so existing CDC-VP buses need no streaming support.
struct AxiExtension : tlm::tlm_extension<AxiExtension> {
    unsigned channel = 0;
    unsigned id = reg::AXI_MASTER_ID;
    unsigned beat_bytes = 4;
    unsigned beats = 1;
    unsigned beat_index = 0;
    unsigned cache = 0;
    unsigned prot = 0;
    bool increment = true;
    bool fragmented = false;
    bool command_fetch = false;
    tlm::tlm_extension_base* clone() const override { return new AxiExtension(*this); }
    void copy_from(const tlm::tlm_extension_base& other) override {
        *this = static_cast<const AxiExtension&>(other);
    }
};
} // namespace fx1::dma
