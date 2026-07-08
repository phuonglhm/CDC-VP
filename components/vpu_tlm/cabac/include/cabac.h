#ifndef CABAC_H
#define CABAC_H

#include <systemc>
#include "tlm.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include <vector>
#include <cstdint>
#include <utility>
#include <algorithm>
#include "custom_packet.h"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <regex>
#include <string>
#include "cabac_tables.h"

// forward-declare CabacCustomPacket to avoid including the full header in this file
struct CabacCustomPacket;

class Cabac : sc_core::sc_module {
    public:
    Cabac(sc_core::sc_module_name name);
    tlm_utils::simple_initiator_socket<Cabac> mem_socket;
    tlm_utils::simple_initiator_socket<Cabac> out_socket;
    tlm_utils::simple_target_socket<Cabac> start_socket;


    private:
     struct Bin {
        bool bit;         
        uint32_t ctx_idx;
          bool bypass;
          uint8_t bina_type; // binarization type (FL=0,TU=1,EG1=2,CREG=4,SP=5)
          uint8_t cmax;      // cMax value used by FL/TU types
    };
    void b_transport(tlm::tlm_generic_payload& trans, sc_core::sc_time& delay);
    std::vector<uint8_t> binarize(const std::vector<uint8_t>& coeffs);
    std::vector<Bin> bin_buffer_select(const CabacCustomPacket &pkt, const std::vector<uint8_t>& binstream);
    std::pair<uint8_t, uint8_t> read_context(uint32_t ctx_idx);
    void write_context(uint32_t ctx_idx, uint8_t state, uint8_t mps);
    void emit_bytes_to_mem(uint64_t addr, const std::vector<uint8_t>& data);

    std::vector<Bin> assign_contexts(const CabacCustomPacket &pkt, const std::vector<uint8_t>& binstream);
    std::vector<uint8_t> encode_bins(const std::vector<Bin>& bins, uint64_t emit_base_addr = 0x20000000ULL);
    uint16_t rlps_for_state(uint8_t state, uint16_t range);
    void range_update(bool bin, uint16_t &range, uint16_t &low, uint8_t &state, uint8_t &mps);    

    // Current QP
    uint8_t qp_ = 22;
};
#endif
