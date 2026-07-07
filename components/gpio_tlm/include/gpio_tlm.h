#pragma once

#include <cstdint>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::components {

// Minimal memory-mapped GPIO block (32 pins, single port).
//
// Register window: 32-bit aligned accesses, offsets 0x00..0x08. Addresses are
// region-local because bus_router subtracts the peripheral base before
// forwarding.
//
//   0x00 VALUE R   pin levels: input pins reflect the external stimulus set
//                  via set_pin(); output pins read back OUT.
//   0x04 OUT   R/W output latch (takes effect on pins whose DIR bit is 1).
//   0x08 DIR   R/W direction, bit=1 output, bit=0 input. Reset: all inputs.
//
// No interrupt output in this revision (PLIC slot stays reserved), matching
// the platform's PWM/CMU precedent. External pin stimulus (board straps,
// testbenches, CLI) is injected with set_pin().
class gpio_tlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<gpio_tlm> socket;

    static constexpr std::uint64_t kValueOffset = 0x00;
    static constexpr std::uint64_t kOutOffset   = 0x04;
    static constexpr std::uint64_t kDirOffset   = 0x08;

    explicit gpio_tlm(sc_core::sc_module_name name,
                      sc_core::sc_time access_latency = sc_core::sc_time(10, sc_core::SC_NS));

    // External stimulus for an input pin (board strap / testbench / CLI).
    void set_pin(unsigned pin, bool level);

    bool pin(unsigned pin) const;

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    unsigned int transport_dbg(tlm::tlm_generic_payload& trans);

    bool access(tlm::tlm_generic_payload& trans);  // shared reg read/write
    std::uint32_t value_reg() const;

    std::uint32_t ext_in_ = 0;   // external levels driven onto input pins
    std::uint32_t out_ = 0;
    std::uint32_t dir_ = 0;      // reset: all inputs
    sc_core::sc_time access_latency_;
};

} // namespace cdc::components
