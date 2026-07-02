#ifndef SIMPLE_MEMORY_H
#define SIMPLE_MEMORY_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_target_socket.h"
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <iostream>
#include <fstream>
#include "cabac_tables.h"

// Minimal byte-addressable TLM memory that stores only written/loaded bytes.
class SimpleMemory : public sc_core::sc_module {
  public:
    SimpleMemory(sc_core::sc_module_name name);
    tlm_utils::simple_target_socket<SimpleMemory> socket;
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    void load_data(uint64_t addr, const std::vector<uint8_t>& data);
    bool load_cabac_tables(const std::vector<std::string>& candidates = {});

  private:
    std::unordered_map<uint64_t, uint8_t> mem_; // sparse byte storage
};

#endif
