#ifndef VPU_TLM_COMMON_SIMPLE_MEMORY_H
#define VPU_TLM_COMMON_SIMPLE_MEMORY_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <iostream>

// Canonical SimpleMemory used by tests and modules. Provides both
// a TLM target socket (for external bindings) and direct read/write
// helpers used by FetchWrapper in unit-mode.
class SimpleMemory : public sc_core::sc_module {
  public:
    SimpleMemory(sc_core::sc_module_name name);
    tlm_utils::simple_target_socket<SimpleMemory> socket;

    // TLM entry point
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    // Direct read/write helpers (non-TLM) used by FetchWrapper
    std::vector<uint8_t> read_region(uint64_t addr, size_t len) const;
    void write_region(uint64_t addr, const uint8_t* data, size_t len);

    void load_data(uint64_t addr, const std::vector<uint8_t>& data);

    // Cabac-specific helper (implemented in cabac build if needed)
    bool load_cabac_tables(const std::vector<std::string>& candidates = {});

  private:
    std::unordered_map<uint64_t, uint8_t> mem_; // sparse byte storage
};

#endif
