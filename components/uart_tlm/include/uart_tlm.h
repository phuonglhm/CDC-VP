#pragma once

#include <iostream>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace cdc::components {

// Memory-mapped UART, TX-only for now.
//
// Register map (offset from base):
//   0x0  TXDATA  (write byte)  -> emits the low 8 bits to the output stream
//
// Any other offset, or a read, returns TLM_ADDRESS_ERROR_RESPONSE.
// The model is intentionally minimal; status/RX registers can be added later
// without breaking the existing TXDATA contract.
class uart_tlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<uart_tlm> socket;

    // out:            backing stream for emitted characters (default std::cout).
    // access_latency: delay added per transaction.
    explicit uart_tlm(sc_core::sc_module_name name,
                      std::ostream& out = std::cout,
                      sc_core::sc_time access_latency = sc_core::sc_time(10, sc_core::SC_NS));

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    std::ostream& out_;
    sc_core::sc_time access_latency_;
};

} // namespace cdc::components
