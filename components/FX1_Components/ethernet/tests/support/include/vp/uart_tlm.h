// =============================================================================
// Integrated Circuits and Systems laboratory
// Author: Hung Nguyen <ngthung@hcmut.edu.vn>
// Module: vp.common.uart_tlm
// Description: Minimal transmit-only UART target: firmware log to stdout.
// Changelog:
//   2026-09-28: Initial version.
// =============================================================================
//
//   0x0 TXDATA  W   byte to send (bits 7:0)
//   0x4 STATUS  R   bit 0 TX ready (always 1)
//
// Output is line-buffered and prefixed with nothing, so a regression can grep
// the firmware's markers exactly.  Other offsets: ADDRESS_ERROR.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace vp {

class UartTlm : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<UartTlm> socket;

    explicit UartTlm(sc_core::sc_module_name name) : sc_module(name), socket("socket") {
        socket.register_b_transport(this, &UartTlm::b_transport);
    }
    ~UartTlm() override {
        if (!line_.empty()) std::printf("%s\n", line_.c_str());
    }
    const std::string& log() const { return log_; }

private:
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay) {
        delay += sc_core::sc_time(20, sc_core::SC_NS);
        tx.set_dmi_allowed(false);
        unsigned char* d = tx.get_data_ptr();
        if (!d || tx.get_data_length() == 0 || tx.get_data_length() > 4) {
            tx.set_response_status(tlm::TLM_GENERIC_ERROR_RESPONSE);
            return;
        }
        const uint64_t a = tx.get_address();
        if (a == 0x0 && tx.is_write()) {
            put(char(d[0]));
        } else if (a == 0x4 && tx.is_read()) {
            std::memset(d, 0, tx.get_data_length());
            d[0] = 1;
        } else if (a == 0x0 && tx.is_read()) {
            std::memset(d, 0, tx.get_data_length());
        } else {
            tx.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        tx.set_response_status(tlm::TLM_OK_RESPONSE);
    }
    void put(char c) {
        log_ += c;
        if (c == '\n') {
            std::printf("%s\n", line_.c_str());
            std::fflush(stdout);
            line_.clear();
        } else if (c != '\r') {
            line_ += c;
        }
    }
    std::string line_, log_;
};

}  // namespace vp
