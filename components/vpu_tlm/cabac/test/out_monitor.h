#ifndef OUT_MONITOR_H
#define OUT_MONITOR_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include <vector>

class OutMonitor : public sc_core::sc_module {
  public:
    OutMonitor(sc_core::sc_module_name name);
    SC_HAS_PROCESS(OutMonitor);
    tlm_utils::simple_target_socket<OutMonitor> cabac_socket;
    std::vector<uint8_t> last_data;

  private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

#endif
