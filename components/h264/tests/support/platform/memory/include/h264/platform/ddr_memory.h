#pragma once
#include <h264/sync/reset_domain.h>
#include <tlm_utils/multi_passthrough_target_socket.h>
namespace h264 {
struct MemoryRecord { bool write; uint64_t address; unsigned bytes; sc_core::sc_time completed; };
class DdrMemory : public sc_core::sc_module {
public:
    tlm_utils::multi_passthrough_target_socket<DdrMemory> socket{"socket"};
    sc_core::sc_time latency{40,sc_core::SC_NS};
    uint64_t error_begin=0, error_end=0; // Half-open fault region, empty by default.
    bool in_flight=false, active_write=false;
    uint64_t active_address=0;
    std::vector<MemoryRecord> trace;
    DdrMemory(sc_core::sc_module_name, size_t bytes);
private:
    std::vector<unsigned char> bytes_;
    sc_core::sc_mutex lock_;
    void transport(int,tlm::tlm_generic_payload&,sc_core::sc_time&);
};
}
