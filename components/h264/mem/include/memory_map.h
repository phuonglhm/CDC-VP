#pragma once

#include "internal_memory.h"
#include "external_memory.h"

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>

namespace h264::mem {
struct MemoryMap : sc_core::sc_module {
    tlm_utils::simple_target_socket<MemoryMap> socket{"socket"};

    SC_HAS_PROCESS(MemoryMap);
    MemoryMap(sc_core::sc_module_name name);

private:
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);

    InternalMemory internal_mem_;
    ExternalMemory external_mem_;

    // Memory Map
    static constexpr std::uint32_t INT_MEM_BASE = 0x00000000;
    static constexpr std::uint32_t INT_MEM_SIZE = 64 * 1024; // 64KB

    static constexpr std::uint32_t EXT_MEM_BASE = 0x10000000;
    static constexpr std::uint32_t EXT_MEM_SIZE = 16 * 1024 * 1024; // 16MB
};
} // namespace h264::mem