#ifndef FETCH_TEST_SIMPLE_MEMORY_H
#define FETCH_TEST_SIMPLE_MEMORY_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include <vector>
#include <unordered_map>

// Test-scoped TLM memory (no CABAC table loading).
class FetchSimpleMemory : public sc_core::sc_module {
public:
    tlm_utils::simple_target_socket<FetchSimpleMemory> socket;
    FetchSimpleMemory(sc_core::sc_module_name name);
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void load_data(uint64_t addr, const std::vector<uint8_t>& data);

private:
    std::unordered_map<uint64_t, uint8_t> mem_;
};

#endif
