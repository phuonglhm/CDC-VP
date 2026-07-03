#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

class MotionVector : sc_core::sc_module {
    public:
    MotionVector(sc_core::sc_module_name name);
    tlm_utils::simple_initiator_socket<MotionVector> filter_socket;
    tlm_utils::simple_target_socket<MotionVector> start_socket;


    private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
};