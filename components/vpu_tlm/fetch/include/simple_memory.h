#ifndef SIMPLE_MEMORY_H
#define SIMPLE_MEMORY_H

#include <systemc>
#include "tlm.h"
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <iostream>
#include <fstream>
// Minimal byte-addressable memory used by FetchWrapper in TLM tests.
// Provides both a TLM target socket (for compatibility) and direct
// read/write accessors so FetchWrapper can call it without going
// through TLM sockets.
class SimpleMemory : public sc_core::sc_module {
  public:
    SimpleMemory(sc_core::sc_module_name name);
    // No TLM socket here; FetchWrapper calls read_region/write_region directly.

    // TLM target entry point
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);

    // Direct read/write helpers (non-TLM) used by FetchWrapper
    std::vector<uint8_t> read_region(uint64_t addr, size_t len) const;
    void write_region(uint64_t addr, const uint8_t* data, size_t len);

    void load_data(uint64_t addr, const std::vector<uint8_t>& data);

  private:
    std::unordered_map<uint64_t, uint8_t> mem_; // sparse byte storage
};

#endif
