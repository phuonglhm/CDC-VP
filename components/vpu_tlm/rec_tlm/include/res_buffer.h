#ifndef RES_BUFFER_H
#define RES_BUFFER_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "rec_packet.h"
#include "rec_memory.h"

class ResBuffer : sc_core::sc_module {
    public:
    ResBuffer(sc_core::sc_module_name name);
    SC_HAS_PROCESS(ResBuffer);
    tlm_utils::simple_initiator_socket<ResBuffer> buffer_socket; //consder having rec_tq read directly from fifo
    sc_core::sc_fifo<RecPacket> fifo_buffer;
    tlm_utils::simple_initiator_socket<ResBuffer> frame_socket;
    tlm_utils::simple_target_socket<ResBuffer> mc_socket;
    tlm_utils::simple_target_socket<ResBuffer> intra_socket;

    // Bind a RecMemoryIf implementation so this buffer can fetch original
    // pixels to compute residual = original - prediction.
    void bindMemory(RecMemoryIf &mem);

    private:
    void intra_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void mc_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void forward_thread();
    RecMemoryIf *mem_if{nullptr};
};
#endif