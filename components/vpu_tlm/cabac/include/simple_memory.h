#ifndef SIMPLE_MEMORY_H
#define SIMPLE_MEMORY_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>

// Minimal byte-addressable TLM memory that stores only written/loaded bytes.
class SimpleMemory : public sc_core::sc_module {
  public:
    tlm_utils::simple_target_socket<SimpleMemory> socket;

    // name: SystemC module name
    // The module stores bytes on demand; no large contiguous allocation required.
    SimpleMemory(sc_core::sc_module_name name);

    // blocking transport entry
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    // helper to preload a block of data at a given address
    void load_data(uint64_t addr, const std::vector<uint8_t>& data);

    // Parse RTL and preload CABAC context/init tables into this memory.
    // If `candidates` is empty a small set of default paths will be tried.
    // Returns true when at least one table was successfully loaded.
    bool load_cabac_tables(const std::vector<std::string>& candidates = {});

  private:
    std::unordered_map<uint64_t, uint8_t> mem_; // sparse byte storage
};

#endif
