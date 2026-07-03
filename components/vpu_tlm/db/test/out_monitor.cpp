#include "out_monitor.h"
#include "custom_packet.h"
#include <iostream>

using namespace sc_core;

OutMonitor::OutMonitor(sc_core::sc_module_name name)
    : sc_module(name), filter_socket("tq_socket") {
    filter_socket.register_b_transport(this, &OutMonitor::b_transport);
}

void OutMonitor::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(trans.get_data_ptr());
    size_t len = trans.get_data_length();
    CustomPacket pkt = unpackCustomPacket(data, len);
    last_pkt_ = pkt;
    have_last_ = true;
    std::cout << "OutMonitor received:" << std::endl << pkt << std::endl;
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

bool OutMonitor::get_last(CustomPacket &pkt) {
    if (!have_last_) return false;
    pkt = last_pkt_;
    return true;
}

void OutMonitor::clear_last() {
    have_last_ = false;
    last_pkt_ = CustomPacket();
}