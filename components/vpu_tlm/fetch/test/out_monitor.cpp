#include <systemc>
#include "tlm_utils/simple_target_socket.h"
#include "fetch_packet.h"

// Simple out receiver for wrapper responses
struct FetchOutReceiver : sc_core::sc_module {
    tlm_utils::simple_target_socket<FetchOutReceiver> start_socket;
    FetchPacket last;
    bool got;

    FetchOutReceiver(sc_core::sc_module_name name) : sc_core::sc_module(name), start_socket("start_socket"), got(false) {
        start_socket.register_b_transport(this, &FetchOutReceiver::b_transport);
    }

    void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
        const uint8_t *buf = reinterpret_cast<const uint8_t*>(trans.get_data_ptr());
        size_t len = trans.get_data_length();
        last = unpackFetchPacket(buf, len);
        got = true;
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};