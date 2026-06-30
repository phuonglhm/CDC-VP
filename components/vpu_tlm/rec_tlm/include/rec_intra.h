#ifndef REC_INTRA_H
#define REC_INTRA_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "rec_packet.h"
#include "rec_memory.h"

class RecIntra : sc_core::sc_module {
    public:
    RecIntra(sc_core::sc_module_name name);
    SC_HAS_PROCESS(RecIntra);
    tlm_utils::simple_initiator_socket<RecIntra> buffer_socket;
    tlm_utils::simple_target_socket<RecIntra> start_socket;
    // tlm_utils::simple_initiator_socket<RecIntra> mem_socket; kept for compatibility (unused)

    // Blocking helper: fetch a contiguous reference block via TLM
    bool getRefBlock(RecPlane plane,
                     uint32_t x,
                     uint32_t y,
                     uint8_t size4x4,
                     PaddingMode pad,
                     RefBlock &out);

    //fetch reference bytes as a flat vector (row-major)
    bool fetchRefAsVector(RecPlane plane,
                          uint32_t x,
                          uint32_t y,
                          uint8_t size4x4,
                          PaddingMode pad,
                          std::vector<uint8_t> &out);
    // Bind a RecMemoryIf implementation to serve reference pixels
    void bindMemory(RecMemoryIf &mem);

    private:
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    RecMemoryIf *mem_if{nullptr};
    void DC_mode(const RefBlock& win, uint8_t size4x4, uint8_t pre_sel, uint32_t i4x4_x, uint32_t i4x4_y, std::vector<uint8_t>& out);
    // Planar prediction for the 4x4 sub-block at `i4x4_x,i4x4_y` inside the
    // N×N block indicated by `size4x4`.
    void planar_mode(const RefBlock& win, uint8_t size4x4, uint32_t i4x4_x, uint32_t i4x4_y, std::vector<uint8_t>& out);
    // Angular prediction for a given intra `mode` id.
    // `mode` follows RTL numbering (planar=0, dc=1, angular modes >=2).
    void angular_mode(const RefBlock& win, uint8_t size4x4, uint8_t mode, uint32_t i4x4_x, uint32_t i4x4_y, std::vector<uint8_t>& out);
};

#endif