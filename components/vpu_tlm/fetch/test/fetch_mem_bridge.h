#ifndef FETCH_TEST_MEM_BRIDGE_H
#define FETCH_TEST_MEM_BRIDGE_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "../include/memory_if.h"

// Tiny test-scoped MemBridge: forwards target b_transport to an initiator socket.
// Also recognizes fetch-origin addresses (marked with a magic bit) and
// translates them into the Rec-packed address format before forwarding.
class FetchMemBridge : public sc_core::sc_module {
public:
    tlm_utils::simple_initiator_socket<FetchMemBridge> socket;
    tlm_utils::simple_target_socket<FetchMemBridge> t_socket;

    static constexpr uint64_t FETCH_ADDR_MAGIC = (1ULL << 55);

    FetchMemBridge(sc_core::sc_module_name name)
        : sc_core::sc_module(name), socket("socket"), t_socket("t_socket")
    {
        t_socket.register_b_transport(this, &FetchMemBridge::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
        try {
            uint64_t addr = trans.get_address();
            if ((addr & FETCH_ADDR_MAGIC) != 0) {
                uint64_t clean = addr & ~FETCH_ADDR_MAGIC;
                uint8_t plane = static_cast<uint8_t>((clean >> 56) & 0xFFULL);
                uint32_t x = static_cast<uint32_t>(clean & 0xFFFFULL);
                uint32_t y = static_cast<uint32_t>((clean >> 16) & 0xFFFFFFFFULL);

                unsigned char* ptr = trans.get_data_ptr();
                unsigned int len = trans.get_data_length();

                // Translate each byte to a Rec-packed per-pixel access
                for (unsigned int i = 0; i < len; ++i) {
                    uint32_t xi = x + i;
                    uint32_t yi = y;
                    uint64_t xx = static_cast<uint64_t>(xi) & 0x1FFULL;
                    uint64_t yy = static_cast<uint64_t>(yi) & 0x1FFULL;
                    uint64_t pp = static_cast<uint64_t>((plane <= 2) ? plane : 0) & 0x3ULL;
                    uint64_t pd = static_cast<uint64_t>(0) & 0x3ULL;
                    uint64_t s = static_cast<uint64_t>(0) & 0x3ULL;
                    uint64_t rec_addr = (pp << (2 + 2 + 9)) | (pd << (2 + 9)) | (s << 9) | (xx << 9) | yy;

                    tlm::tlm_generic_payload t2;
                    if (trans.get_command() == tlm::TLM_READ_COMMAND) {
                        t2.set_command(tlm::TLM_READ_COMMAND);
                        t2.set_address(rec_addr);
                        t2.set_data_ptr(&ptr[i]);
                        t2.set_data_length(1);
                        t2.set_streaming_width(1);
                        t2.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
                        socket->b_transport(t2, delay);
                        if (t2.get_response_status() != tlm::TLM_OK_RESPONSE) { trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return; }
                    } else if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
                        t2.set_command(tlm::TLM_WRITE_COMMAND);
                        t2.set_address(rec_addr);
                        t2.set_data_ptr(&ptr[i]);
                        t2.set_data_length(1);
                        t2.set_streaming_width(1);
                        t2.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
                        socket->b_transport(t2, delay);
                        if (t2.get_response_status() != tlm::TLM_OK_RESPONSE) { trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE); return; }
                    } else {
                        trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
                        return;
                    }
                }
                trans.set_response_status(tlm::TLM_OK_RESPONSE);
                return;
            }

            socket->b_transport(trans, delay);
        } catch (...) {
            trans.set_response_status(tlm::TLM_COMMAND_ERROR_RESPONSE);
        }
    }
};

#endif
