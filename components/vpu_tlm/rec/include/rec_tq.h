#ifndef REC_TQ_H
#define REC_TQ_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "custom_packet.h"

class RecTQ : sc_core::sc_module {
    public:
    RecTQ(sc_core::sc_module_name name);
    SC_HAS_PROCESS(RecTQ);
    tlm_utils::simple_initiator_socket<RecTQ> inv_tq_socket;
    tlm_utils::simple_initiator_socket<RecTQ> cabac_socket;
    tlm_utils::simple_target_socket<RecTQ> buffer_socket;

    private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void dct(uint8_t size4x4, const std::vector<int16_t>& in, std::vector<int32_t>& out);
    void quantize(uint8_t size4x4, const std::vector<int32_t>& in, uint8_t qp, bool type_i, std::vector<int16_t>& out);

};
#endif