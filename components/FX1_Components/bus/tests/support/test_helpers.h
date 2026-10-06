#pragma once
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>
#include <string>

struct Checks {
    unsigned checks=0, failures=0;
    void expect(bool value, const std::string& message) {
        ++checks;
        if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    }
};
struct Master : sc_core::sc_module {
    tlm_utils::simple_initiator_socket<Master> socket{"socket"};
    explicit Master(sc_core::sc_module_name name) : sc_module(name) {}
};
inline void payload(tlm::tlm_generic_payload& tx, tlm::tlm_command command,
                    std::uint64_t address, unsigned char* data, unsigned length) {
    tx.set_command(command); tx.set_address(address);
    tx.set_data_ptr(data); tx.set_data_length(length); tx.set_streaming_width(length);
    tx.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
}
