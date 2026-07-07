#ifndef FETCH_WRAPPER_TLM_H
#define FETCH_WRAPPER_TLM_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "simple_memory.h"

class FetchWrapper : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<FetchWrapper> start_socket;
    tlm_utils::simple_initiator_socket<FetchWrapper> out_socket;
        // Optional initiator socket (used when `use_mem_socket` is true).
        // This is kept as a socket object so call sites can use
        // `mem_socket->b_transport(...)` like other modules.
        tlm_utils::simple_initiator_socket<FetchWrapper> mem_socket;
        // Local memory used by the wrapper for simplified tests (allocated
        // only when not using an external mem_socket to avoid creating an
        // unbound TLM socket inside the embedded memory instance).
        SimpleMemory* simple_mem{nullptr};

        FetchWrapper(sc_core::sc_module_name name, bool use_mem_socket = false);

        // Helpers used internally for reading/writing memory; these will
        // either call `mem_socket->b_transport` (when enabled) or use the
        // embedded `simple_mem`.
        std::vector<uint8_t> read_from_mem(uint64_t addr, size_t len);
        void write_to_mem(uint64_t addr, const uint8_t* data, size_t len);

        void b_transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);

    private:
        bool use_mem_socket_{false};
};

#endif
