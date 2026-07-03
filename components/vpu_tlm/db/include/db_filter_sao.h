#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

class Filter_SAO : sc_core::sc_module {
    public:
    Filter_SAO(sc_core::sc_module_name name);
    tlm_utils::simple_initiator_socket<Filter_SAO> out_socket;
    tlm_utils::simple_target_socket<Filter_SAO> bs_socket;
    tlm_utils::simple_target_socket<Filter_SAO> mv_socket;

    private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};