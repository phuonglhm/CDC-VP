#pragma once

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <vector>
#include <cstdint>

namespace h264::mem {
struct MemoryMap : sc_core::sc_module {
    tlm_utils::simple_target_socket<MemoryMap> socket{"socket"};

    SC_HAS_PROCESS(MemoryMap);
    MemoryMap(sc_core::sc_module_name name);

private:
    void b_transport(tlm::tlm_generic_payload& tx, sc_core::sc_time& delay);

    // Mảng bộ nhớ thực tế mô phỏng Hardware
    std::vector<std::uint8_t> internal_mem_;
    std::vector<std::uint8_t> external_mem_;

    // Memory Map
    static constexpr std::uint32_t INT_MEM_BASE = 0x00000000;
    static constexpr std::uint32_t INT_MEM_SIZE = 64 * 1024; // 64KB

    static constexpr std::uint32_t EXT_MEM_BASE = 0x10000000;
    static constexpr std::uint32_t EXT_MEM_SIZE = 16 * 1024 * 1024; // 16MB
};
} // namespace h264::mem