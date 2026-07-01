#ifndef REC_MC_H
#define REC_MC_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "rec_packet.h"
#include "rec_memory.h"
#include "rec_mv.h"


class RecMc : sc_core::sc_module {
    public:
    RecMc (sc_core::sc_module_name name);
    SC_HAS_PROCESS(RecMc);
    tlm_utils::simple_initiator_socket<RecMc> buffer_socket;
    tlm_utils::simple_target_socket<RecMc> start_socket;

    // Bind a RecMemoryIf implementation to serve reference pixels
    void bindMemory(RecMemoryIf &mem);

    // Bind an MV memory provider for motion-vector storage/lookup
    void bindMvMemory(RecMvIf &mvmem);

    // Blocking helper: fetch a contiguous reference block via memory interface
    bool getRefBlock(RecPlane plane,
                     uint32_t x,
                     uint32_t y,
                     uint8_t size4x4,
                     PaddingMode pad,
                     RefBlock &out);

    // Fetch reference bytes as a flat vector (row-major)
    bool fetchRefAsVector(RecPlane plane,
                          uint32_t x,
                          uint32_t y,
                          uint8_t size4x4,
                          PaddingMode pad,
                          std::vector<uint8_t> &out);

    private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    void handle_pre(const RecPacket &pkt);
    void handle_read_req(const RecPacket &pkt, tlm::tlm_generic_payload &trans);
    void handle_coeff(const RecPacket &pkt);
    void handle_residual(const RecPacket &pkt);
    RecMemoryIf *mem_if{nullptr};
    RecMvIf *mvd_if{nullptr};
};

#endif