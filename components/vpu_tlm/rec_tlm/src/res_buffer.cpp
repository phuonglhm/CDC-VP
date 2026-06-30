#include "res_buffer.h"

ResBuffer::ResBuffer(sc_core::sc_module_name name) : 
    sc_module(name), intra_socket("intra_socket"), mc_socket("mc_socket") {
    intra_socket.register_b_transport(this, &ResBuffer::intra_transport);
    mc_socket.register_b_transport(this, &ResBuffer::mc_transport); 
    SC_THREAD(forward_thread);
}

// (no global b_transport) - ResBuffer exposes intra_transport and mc_transport

void ResBuffer::intra_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    RecPacket pkt;
    auto len = trans.get_data_length();
    if (len > 0 && trans.get_data_ptr()) {
        const uint8_t *ptr = reinterpret_cast<const uint8_t*>(trans.get_data_ptr());
        pkt = unpackRecPacket(ptr, len);
    }
    fifo_buffer.write(std::move(pkt));
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

void ResBuffer::mc_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    RecPacket pkt;
    auto len = trans.get_data_length();
    if (len > 0 && trans.get_data_ptr()) {
        const uint8_t *ptr = reinterpret_cast<const uint8_t*>(trans.get_data_ptr());
        pkt = unpackRecPacket(ptr, len);
    }
    fifo_buffer.write(std::move(pkt));
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

void ResBuffer::forward_thread() {
    while (true) {
        RecPacket pkt = fifo_buffer.read(); // blocks until data available

        // Build a TLM write transaction carrying the packed RecPacket (header + payload)
        std::vector<uint8_t> buf = packRecPacket(pkt); // owns a copy while we call b_transport
        tlm::tlm_generic_payload trans;
        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(0);
        trans.set_data_ptr(buf.data());
        trans.set_data_length(buf.size());
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

        // Send to downstream RecTQ
        buffer_socket->b_transport(trans, delay);
        // ignore response for now
    }
}