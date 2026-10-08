#pragma once
#include <cstdint>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

namespace fx1 {
// Pass-through between a RISC-V VP++ hart and the FX1 bus. It tells guest
// mistakes (become architectural access faults) apart from model or
// integration defects (stop the simulation with a diagnostic).
//
// Before forwarding, the payload must match what the VP++ hart produces:
// read/write command, non-null data, length 1/2/4/8, no byte enables, and a
// streaming width that is 0 (VP++ never sets it; completed here to the data
// length) or at least the data length. Anything else is an integration error.
// The response is reset to INCOMPLETE before forwarding: VP++ pre-sets OK, so a
// target that never writes a response would otherwise look like a success.
//
// After the target returns:
//   OK, ADDRESS_ERROR, GENERIC_ERROR  -> unchanged
//   BURST_ERROR, COMMAND_ERROR        -> GENERIC_ERROR (plan C4): a valid guest
//                                        access the target refuses (size, write
//                                        to read-only), so VP++ raises access
//                                        fault 1/5/7 instead of aborting
//   BYTE_ENABLE_ERROR                 -> integration error (no byte enables
//                                        were sent, so the target is wrong)
//   INCOMPLETE                        -> integration error (no target answered)
// Integration errors are raised with SC_REPORT_ERROR naming the address.
class CpuPortAdapter : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<CpuPortAdapter> target{"target"};
    tlm_utils::simple_initiator_socket<CpuPortAdapter> out{"out"};

    explicit CpuPortAdapter(sc_core::sc_module_name name);

    // Valid guest accesses whose refusal was mapped to GENERIC_ERROR.
    std::uint64_t mapped_errors() const noexcept { return mapped_; }
    // Integration errors reported.
    std::uint64_t integration_errors() const noexcept { return defects_; }

private:
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);
    unsigned transport_dbg(tlm::tlm_generic_payload& tx);
    void integration_error(const tlm::tlm_generic_payload& tx, const char* what);

    std::uint64_t mapped_ = 0, defects_ = 0;
};
} // namespace fx1
