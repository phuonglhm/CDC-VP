#ifndef OUT_MONITOR_H
#define OUT_MONITOR_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include "custom_packet.h"

using namespace sc_core;

class OutMonitor : public sc_core::sc_module {
  public:
    OutMonitor(sc_core::sc_module_name name);
    tlm_utils::simple_target_socket<OutMonitor> filter_socket;

    bool get_last(CustomPacket &pkt);
    void clear_last();

  private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    CustomPacket last_pkt_;
    bool have_last_{false};
};

#endif
