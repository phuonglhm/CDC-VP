#pragma once

#include <cstdint>
#include <vector>

#include <systemc>

#include "vpu_cabac_custom_packet.h"
#include "vpu_cabac_simple_memory.h"
#include "vpu_db_custom_packet.h"
#include "vpu_rec_packet.h"
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

namespace cdc::components {

class rec_to_db_bridge : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<rec_to_db_bridge> start_socket;
    tlm_utils::simple_initiator_socket<rec_to_db_bridge> bs_socket;
    tlm_utils::simple_initiator_socket<rec_to_db_bridge> mv_socket;

    explicit rec_to_db_bridge(sc_core::sc_module_name name);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    static DbCustomPacket convert_packet(const RecPacket& packet);
};

class rec_to_cabac_bridge : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<rec_to_cabac_bridge> start_socket;
    tlm_utils::simple_initiator_socket<rec_to_cabac_bridge> cabac_socket;

    explicit rec_to_cabac_bridge(sc_core::sc_module_name name);

    void bind_memory(CabacSimpleMemory& memory);

private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    static CabacCustomPacket convert_packet(const RecPacket& packet);

    CabacSimpleMemory* memory_ = nullptr;
};

} // namespace cdc::components
