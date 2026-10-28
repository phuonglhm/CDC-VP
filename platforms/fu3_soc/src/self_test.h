#pragma once

#include <cstdint>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

#include "fu3_soc_top.h"

namespace cdc::platforms::fu3_soc {

// Platform self-check (`vp_fu3_soc --self-test`, ctest fu3_soc_self_test).
//
// Takes the IBEX initiator port, which stays free while no CPU is integrated,
// and checks the assembled platform rather than any model:
//   - every target decodes to its own memory, model (a register with a known
//     reset value) or responder at the right local offset (debug path, so the
//     access policy does not hide a mis-route);
//   - a functional access on each of SYSBUS_0/1 and PERIBUS_0/1 that still has
//     a not-integrated open target reaches it and returns TLM_GENERIC_ERROR_RESPONSE;
//   - unmapped space returns TLM_ADDRESS_ERROR_RESPONSE and reaches no responder.
// Starts once the system domain is out of reset; stops the simulation when done.
class self_test : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<self_test> socket{"socket"};

    SC_HAS_PROCESS(self_test);
    self_test(sc_core::sc_module_name name, fu3_soc_top& top, std::ostream& log);

    bool done() const { return done_; }
    bool passed() const { return done_ && failures_ == 0; }
    unsigned checks() const { return checks_; }
    unsigned failures() const { return failures_; }

private:
    void run();
    void check(bool ok, const std::string& what);
    tlm::tlm_response_status access(bool write, std::uint64_t address);
    unsigned debug_access(bool write, std::uint64_t address, unsigned char* data);
    std::uint64_t total_hits() const;

    fu3_soc_top& top_;
    std::ostream& log_;
    bool done_ = false;
    unsigned checks_ = 0;
    unsigned failures_ = 0;
};

} // namespace cdc::platforms::fu3_soc
