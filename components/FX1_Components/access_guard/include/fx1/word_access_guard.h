#pragma once
#include <cstdint>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace fx1 {
// Pass-through that admits only one naturally aligned 32-bit access with all
// bytes enabled, in front of a register model that always transfers 4 bytes
// (e.g. components/uart2_tlm, which copies 4 bytes whatever the payload
// length). Anything else is answered with a slave error (GENERIC), which a
// RISC-V hart reports as a load/store access fault, and never reaches the
// model, so a short guest access cannot overrun the host buffer.
// Debug accesses obey the same rule (they return 0 bytes otherwise).
class WordAccessGuard : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<WordAccessGuard> target{"target"};
    tlm_utils::simple_initiator_socket<WordAccessGuard> out{"out"};

    explicit WordAccessGuard(sc_core::sc_module_name name);

    std::uint64_t rejected() const noexcept { return rejected_; }

private:
    static bool admissible(const tlm::tlm_generic_payload& tx);
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);

    std::uint64_t rejected_ = 0;
};
} // namespace fx1
