#pragma once

// Shared test support: a minimal TLM-2.0 initiator ("probe") plus a tiny
// assertion helper. Component unit tests bind the probe to the component's
// target socket and drive transactions from within an sc_spawn'd process.

#include <cstdint>
#include <iostream>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>

namespace cdc::test {

// Process-wide failure counter; tests return non-zero if it is > 0.
inline int& failures()
{
    static int f = 0;
    return f;
}

class tlm_probe : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<tlm_probe> socket;

    explicit tlm_probe(sc_core::sc_module_name name)
        : sc_core::sc_module(name)
        , socket("socket")
    {
    }

    tlm::tlm_response_status write(std::uint64_t addr, const void* data, unsigned len)
    {
        return access(tlm::TLM_WRITE_COMMAND, addr, const_cast<void*>(data), len);
    }

    tlm::tlm_response_status read(std::uint64_t addr, void* data, unsigned len)
    {
        return access(tlm::TLM_READ_COMMAND, addr, data, len);
    }

    // Backdoor (untimed) access; returns number of bytes transferred.
    unsigned debug(tlm::tlm_command cmd, std::uint64_t addr, void* data, unsigned len)
    {
        tlm::tlm_generic_payload trans;
        fill(trans, cmd, addr, data, len);
        return socket->transport_dbg(trans);
    }

private:
    tlm::tlm_response_status access(tlm::tlm_command cmd,
                                    std::uint64_t addr,
                                    void* data,
                                    unsigned len)
    {
        tlm::tlm_generic_payload trans;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        fill(trans, cmd, addr, data, len);
        socket->b_transport(trans, delay);
        return trans.get_response_status();
    }

    static void fill(tlm::tlm_generic_payload& trans,
                     tlm::tlm_command cmd,
                     std::uint64_t addr,
                     void* data,
                     unsigned len)
    {
        trans.set_command(cmd);
        trans.set_address(addr);
        trans.set_data_ptr(static_cast<unsigned char*>(data));
        trans.set_data_length(len);
        trans.set_streaming_width(len);
        trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    }
};

} // namespace cdc::test

#define CDC_CHECK(cond)                                                       \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "CHECK failed: " #cond " @ " << __FILE__ << ":"      \
                      << __LINE__ << '\n';                                    \
            ++cdc::test::failures();                                          \
        }                                                                     \
    } while (0)
