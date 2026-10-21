#pragma once
#include <tlm>
namespace fu3_bus {
// Functional address progression, NOT raw AXI AxBURST encoding.
// Fixed is decomposed into repeated single transfers by the ingress adapter.
enum class Burst { Increment, Fixed };
// Explicit functional request contract. All default FU3 masters require this.
// length = beat_bytes * number of beats; byte enables repeat over payload bytes.
struct Access : tlm::tlm_extension<Access> {
    unsigned ordering_id = 0; // Adapter-defined independent ordering stream.
    bool instruction = false;
    bool debug = false; // Trusted CPU debug-mode indication supplied by the platform.
    unsigned beat_bytes = 4;
    Burst burst = Burst::Increment;
    tlm_extension_base* clone() const override { return new Access(*this); }
    void copy_from(const tlm_extension_base& other) override {
        ordering_id = static_cast<const Access&>(other).ordering_id;
        instruction = static_cast<const Access&>(other).instruction;
        debug = static_cast<const Access&>(other).debug;
        beat_bytes = static_cast<const Access&>(other).beat_bytes;
        burst = static_cast<const Access&>(other).burst;
    }
};
// Stack-safe attachment for CPU/DMA adapters; restores any previous extension.
class ScopedAccess {
    tlm::tlm_generic_payload& tx_;
    Access access_;
    Access* previous_;
public:
    ScopedAccess(tlm::tlm_generic_payload& tx, unsigned bytes, Burst burst = Burst::Increment,
                 bool instruction = false, unsigned ordering_id = 0, bool debug = false) : tx_(tx), previous_(tx.get_extension<Access>()) {
        access_.ordering_id=ordering_id; access_.beat_bytes=bytes; access_.burst=burst; access_.instruction=instruction;
        access_.debug=debug;
        tx_.set_extension(&access_);
    }
    ~ScopedAccess() { tx_.clear_extension<Access>(); if(previous_) tx_.set_extension(previous_); }
    ScopedAccess(const ScopedAccess&) = delete;
    ScopedAccess& operator=(const ScopedAccess&) = delete;
};
}
