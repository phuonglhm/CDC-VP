#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "custom_packet.h"

class FrameBuffer : sc_core::sc_module {
    public:
    FrameBuffer(sc_core::sc_module_name name);
    SC_HAS_PROCESS(FrameBuffer);
    sc_core::sc_fifo<CustomPacket> fifo_buffer;
    tlm_utils::simple_target_socket<FrameBuffer> mc_socket;

    private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};


FrameBuffer::FrameBuffer(sc_core::sc_module_name name)
  : sc_module(name), fifo_buffer(64), mc_socket("mc_socket") {
    mc_socket.register_b_transport(this, &FrameBuffer::b_transport);
}

void FrameBuffer::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    CustomPacket pkt;
    auto len = trans.get_data_length();
    if (len > 0 && trans.get_data_ptr()) {
        pkt.data.assign(trans.get_data_ptr(), trans.get_data_ptr() + len);
    }
    fifo_buffer.write(std::move(pkt));
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}