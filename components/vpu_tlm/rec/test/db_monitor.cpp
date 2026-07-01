#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

class DBMonitor : sc_core::sc_module {
    public:
    DBMonitor(sc_core::sc_module_name name);
    SC_HAS_PROCESS(DBMonitor);
    tlm_utils::simple_target_socket<DBMonitor> inv_tq_socket;
    std::vector<uint8_t> last_data; // stores last received raw payload

    private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};

DBMonitor::DBMonitor(sc_core::sc_module_name name)
  : sc_module(name), inv_tq_socket("inv_tq_socket"), last_data() {
    inv_tq_socket.register_b_transport(this, &DBMonitor::b_transport);
}

void DBMonitor::b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay) {
    last_data.clear();
    auto len = trans.get_data_length();
    if (len > 0 && trans.get_data_ptr()) {
        auto ptr = reinterpret_cast<uint8_t*>(trans.get_data_ptr());
        last_data.assign(ptr, ptr + len);
    }
    trans.set_response_status(tlm::TLM_OK_RESPONSE);
}