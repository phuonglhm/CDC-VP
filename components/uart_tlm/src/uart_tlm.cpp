#include "uart_tlm.h"
#include <cstdint>

namespace cdc::components {

namespace {
constexpr std::uint64_t kRegTxData = 0x0;
} // namespace

uart_tlm::uart_tlm(sc_core::sc_module_name name, std::ostream &out, sc_core::sc_time access_latency)
    : sc_core::sc_module(name)
    , socket("socket")
    , out_(out)
    , access_latency_(access_latency) {
   socket.register_b_transport(this, &uart_tlm::b_transport);
}

void uart_tlm::b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
   delay += access_latency_;

   if (trans.get_command() != tlm::TLM_WRITE_COMMAND || trans.get_address() != kRegTxData ||
       trans.get_data_length() == 0) {
      trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
      return;
   }

   out_ << static_cast<char>(trans.get_data_ptr()[0]);
   trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

} // namespace cdc::components
